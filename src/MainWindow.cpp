#include "MainWindow.h"

#include "AppIds.h"
#include "AppTheme.h"
#include "CaptureOverlay.h"
#include "Screenshot.h"
#include "UiText.h"
#include "res/resource.h"

#include <algorithm>
#include <dwmapi.h>
#include <commdlg.h>
#include <cwctype>
#include <cstring>
#include <memory>
#include <regex>
#include <shellapi.h>
#include <thread>

namespace app {

namespace {

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef NIN_SELECT
#define NIN_SELECT (WM_USER + 0)
#endif
#ifndef NIN_KEYSELECT
#define NIN_KEYSELECT (WM_USER + 1)
#endif

constexpr UINT WM_APP_STREAM_CHUNK = WM_APP + 10;
constexpr UINT WM_APP_REQUEST_DONE = WM_APP + 11;
constexpr UINT WM_APP_SHOW_OCR_ERROR = WM_APP + 12;
constexpr UINT WM_APP_GLOBAL_HOTKEY = WM_APP + 13;

struct StreamChunkPayload {
    unsigned long long requestId = 0;
    bool translate = false;
    std::wstring text;
};

struct RequestDonePayload {
    unsigned long long requestId = 0;
    bool translate = false;
    ServiceResult result;
};

struct OcrErrorPayload {
    ServiceResult result;
};

struct RecognitionToastState {
    std::wstring text;
    HFONT font = nullptr;
};

struct ClipboardTextSnapshot {
    bool hasText = false;
    std::wstring text;
};

constexpr wchar_t kRecognitionToastClassName[] = L"Win32OcrRecognitionToast";

HWND g_hotkeyMainHwnd = nullptr;
HotkeyConfig g_ocrHotkey {};
HotkeyConfig g_translateHotkey {};

std::wstring LoadS(UINT id) {
    return LoadStringResource(id);
}

std::wstring ClipForDialog(const std::wstring& text, std::size_t maxChars) {
    if (text.size() <= maxChars) {
        return text;
    }
    return text.substr(0, maxChars) + L"\r\n...(truncated)";
}

bool EndsWithNoCase(const std::wstring& text, const wchar_t* suffix) {
    if (!suffix) {
        return false;
    }
    const std::size_t n = wcslen(suffix);
    if (text.size() < n) {
        return false;
    }
    for (std::size_t i = 0; i < n; ++i) {
        const wchar_t a = static_cast<wchar_t>(towlower(text[text.size() - n + i]));
        const wchar_t b = static_cast<wchar_t>(towlower(suffix[i]));
        if (a != b) {
            return false;
        }
    }
    return true;
}

bool IsBlankText(const std::wstring& text) {
    for (wchar_t ch : text) {
        if (!iswspace(ch)) {
            return false;
        }
    }
    return true;
}

std::wstring TrimCopy(const std::wstring& text) {
    std::size_t begin = 0;
    while (begin < text.size() && iswspace(text[begin])) {
        ++begin;
    }
    std::size_t end = text.size();
    while (end > begin && iswspace(text[end - 1])) {
        --end;
    }
    return text.substr(begin, end - begin);
}

std::wstring ModelComboDisplayText(const std::wstring& fullText) {
    const std::size_t dash = fullText.find(L'-');
    if (dash == std::wstring::npos) {
        return TrimCopy(fullText);
    }
    return TrimCopy(fullText.substr(dash + 1));
}

std::vector<std::wstring> SplitFilterPatterns(const std::wstring& filterExpression) {
    std::vector<std::wstring> patterns;
    std::wstring current;
    bool escaping = false;
    for (wchar_t ch : filterExpression) {
        if (escaping) {
            current.push_back(ch);
            escaping = false;
            continue;
        }
        if (ch == L'\\') {
            current.push_back(ch);
            escaping = true;
            continue;
        }
        if (ch == L'|') {
            if (!current.empty()) {
                patterns.push_back(current);
                current.clear();
            }
            continue;
        }
        current.push_back(ch);
    }
    if (!current.empty()) {
        patterns.push_back(current);
    }
    return patterns;
}

bool IsModifierKey(UINT vk) {
    return vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT ||
           vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL ||
           vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU ||
           vk == VK_LWIN || vk == VK_RWIN;
}

UINT CurrentModifiers() {
    UINT modifiers = 0;
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) modifiers |= MOD_CONTROL;
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) modifiers |= MOD_SHIFT;
    if (GetAsyncKeyState(VK_MENU) & 0x8000) modifiers |= MOD_ALT;
    if ((GetAsyncKeyState(VK_LWIN) & 0x8000) || (GetAsyncKeyState(VK_RWIN) & 0x8000)) modifiers |= MOD_WIN;
    return modifiers;
}

bool MatchHotkey(const HotkeyConfig& cfg, UINT vk, UINT modifiers) {
    if (cfg.virtualKey == 0) {
        return false;
    }
    return cfg.virtualKey == vk && cfg.modifiers == modifiers;
}

bool OpenClipboardWithRetry(HWND owner) {
    for (int attempt = 0; attempt < 20; ++attempt) {
        if (OpenClipboard(owner)) {
            return true;
        }
        Sleep(25);
    }
    return false;
}

bool ReadClipboardUnicodeText(HWND owner, std::wstring& text) {
    text.clear();
    if (!OpenClipboardWithRetry(owner)) {
        return false;
    }
    HANDLE data = GetClipboardData(CF_UNICODETEXT);
    if (!data) {
        CloseClipboard();
        return false;
    }
    const wchar_t* raw = static_cast<const wchar_t*>(GlobalLock(data));
    if (!raw) {
        CloseClipboard();
        return false;
    }
    text = raw;
    GlobalUnlock(data);
    CloseClipboard();
    return true;
}

bool SnapshotClipboardText(HWND owner, ClipboardTextSnapshot& snapshot) {
    snapshot = {};
    std::wstring text;
    if (!ReadClipboardUnicodeText(owner, text)) {
        return false;
    }
    snapshot.hasText = true;
    snapshot.text = std::move(text);
    return true;
}

void RestoreClipboardText(HWND owner, const ClipboardTextSnapshot& snapshot) {
    if (!snapshot.hasText) {
        return;
    }
    if (!OpenClipboardWithRetry(owner)) {
        return;
    }
    if (!EmptyClipboard()) {
        CloseClipboard();
        return;
    }
    const std::size_t bytes = (snapshot.text.size() + 1) * sizeof(wchar_t);
    HGLOBAL buffer = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!buffer) {
        CloseClipboard();
        return;
    }
    void* memory = GlobalLock(buffer);
    if (!memory) {
        GlobalFree(buffer);
        CloseClipboard();
        return;
    }
    memcpy(memory, snapshot.text.c_str(), bytes);
    GlobalUnlock(buffer);
    if (!SetClipboardData(CF_UNICODETEXT, buffer)) {
        GlobalFree(buffer);
    }
    CloseClipboard();
}

void ClearClipboardText(HWND owner) {
    if (!OpenClipboardWithRetry(owner)) {
        return;
    }
    EmptyClipboard();
    CloseClipboard();
}

bool SendCopyShortcut() {
    INPUT inputs[4] = {};

    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_CONTROL;

    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 'C';

    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = 'C';
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;

    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_CONTROL;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

    return SendInput(static_cast<UINT>(std::size(inputs)), inputs, sizeof(INPUT)) == std::size(inputs);
}

bool CaptureSelectedTextViaClipboard(HWND owner, std::wstring& text) {
    text.clear();
    const HWND foreground = GetForegroundWindow();
    if (!foreground || foreground == owner) {
        return false;
    }

    ClipboardTextSnapshot snapshot;
    SnapshotClipboardText(owner, snapshot);
    bool captured = false;
    for (int copyRound = 0; copyRound < 3 && !captured; ++copyRound) {
        const DWORD sequenceBefore = GetClipboardSequenceNumber();
        if (!SendCopyShortcut()) {
            continue;
        }

        for (int attempt = 0; attempt < 20; ++attempt) {
            Sleep(50);
            const DWORD sequenceNow = GetClipboardSequenceNumber();
            if (sequenceNow == sequenceBefore) {
                continue;
            }
            std::wstring clipboardText;
            if (!ReadClipboardUnicodeText(owner, clipboardText)) {
                continue;
            }
            if (!clipboardText.empty()) {
                text = std::move(clipboardText);
                captured = true;
                break;
            }
        }
        if (!captured) {
            Sleep(80);
        }
    }

    if (captured) {
        ClearClipboardText(owner);
    } else if (snapshot.hasText) {
        RestoreClipboardText(owner, snapshot);
    }
    return captured;
}

LRESULT CALLBACK MainHotkeyHookProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION &&
        g_hotkeyMainHwnd != nullptr &&
        (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        const auto* info = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
        if (!info) {
            return CallNextHookEx(nullptr, code, wParam, lParam);
        }
        const UINT vk = static_cast<UINT>(info->vkCode);
        if (IsModifierKey(vk)) {
            return CallNextHookEx(nullptr, code, wParam, lParam);
        }
        const UINT modifiers = CurrentModifiers();
        int id = 0;
        if (MatchHotkey(g_ocrHotkey, vk, modifiers)) {
            id = 1;
        } else if (MatchHotkey(g_translateHotkey, vk, modifiers)) {
            id = 2;
        }
        if (id != 0) {
            PostMessageW(g_hotkeyMainHwnd, WM_APP_GLOBAL_HOTKEY, static_cast<WPARAM>(id), 0);
            return 1;
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

void ShowWindowWithoutWhiteFlash(HWND hwnd, int showCommand) {
    const LONG_PTR oldExStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    const bool addLayered = (oldExStyle & WS_EX_LAYERED) == 0;
    if (addLayered) {
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE, oldExStyle | WS_EX_LAYERED);
    }
    SetLayeredWindowAttributes(hwnd, 0, 0, LWA_ALPHA);
    ShowWindow(hwnd, showCommand);
    UpdateWindow(hwnd);
    RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN | RDW_FRAME);
    SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
    if (addLayered) {
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE, oldExStyle);
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
}

HICON LoadAppIcon(HINSTANCE instance, int width, int height) {
    return static_cast<HICON>(LoadImageW(instance,
                                         MAKEINTRESOURCEW(IDI_APP_ICON),
                                         IMAGE_ICON,
                                         width,
                                         height,
                                         0));
}

ATOM EnsureRecognitionToastClass(HINSTANCE instance);
LRESULT CALLBACK RecognitionToastProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

ATOM EnsureRecognitionToastClass(HINSTANCE instance) {
    static ATOM atom = 0;
    if (atom != 0) {
        return atom;
    }
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = RecognitionToastProc;
    wc.hInstance = instance;
    wc.lpszClassName = kRecognitionToastClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    atom = RegisterClassExW(&wc);
    return atom;
}

LRESULT CALLBACK RecognitionToastProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<RecognitionToastState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (message) {
    case WM_NCCREATE: {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* toastState = static_cast<RecognitionToastState*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(toastState));
        return TRUE;
    }
    case WM_CREATE:
        SetTimer(hwnd, 1, 1200, nullptr);
        return 0;
    case WM_TIMER:
        DestroyWindow(hwnd);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        RECT client{};
        GetClientRect(hwnd, &client);
        HBRUSH brush = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(dc, &client, brush);
        DeleteObject(brush);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(224, 228, 232));
        HFONT oldFont = nullptr;
        if (state && state->font) {
            oldFont = reinterpret_cast<HFONT>(SelectObject(dc, state->font));
        }
        DrawTextW(dc,
                  state ? state->text.c_str() : L"",
                  -1,
                  &client,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        if (oldFont) {
            SelectObject(dc, oldFont);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_NCDESTROY:
        if (state) {
            if (state->font) {
                DeleteObject(state->font);
            }
            delete state;
        }
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

}  // namespace

MainWindow::MainWindow()
    : settingsWindow_(config_, configStore_, service_, [this]() {
          ApplyConfigTheme();
          RefreshModelCombo();
          RegisterAppHotkeys();
      }) {
}

MainWindow::~MainWindow() {
    RemoveTrayIcon();
    if (appIconLarge_) {
        DestroyIcon(appIconLarge_);
        appIconLarge_ = nullptr;
    }
    if (appIconSmall_) {
        DestroyIcon(appIconSmall_);
        appIconSmall_ = nullptr;
    }
}

bool MainWindow::Create(HINSTANCE instance) {
    instance_ = instance;

    std::wstring error;
    configStore_.Load(config_, error);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance_;
    wc.lpszClassName = kWindowClassName;
    appIconLarge_ = LoadAppIcon(instance_, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON));
    appIconSmall_ = LoadAppIcon(instance_, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON));
    wc.hIcon = appIconLarge_;
    wc.hIconSm = appIconSmall_;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    RegisterClassExW(&wc);

    hwnd_ = CreateWindowExW(WS_EX_APPWINDOW,
                            wc.lpszClassName,
                            LoadS(IDS_APP_TITLE).c_str(),
                            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                            CW_USEDEFAULT,
                            CW_USEDEFAULT,
                            std::max(480, config_.mainWindowWidth),
                            std::max(360, config_.mainWindowHeight),
                            nullptr,
                            nullptr,
                            instance_,
                            this);
    if (hwnd_) {
        if (appIconLarge_) {
            SendMessageW(hwnd_, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(appIconLarge_));
        }
        if (appIconSmall_) {
            SendMessageW(hwnd_, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(appIconSmall_));
        }
    }
    return hwnd_ != nullptr;
}

LRESULT CALLBACK MainWindow::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    MainWindow* self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<MainWindow*>(create->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    return self ? self->HandleMessage(hwnd, message, wParam, lParam) : DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT MainWindow::HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        return OnCreate() ? 0 : -1;
    case WM_SIZE:
        OnSize();
        return 0;
    case WM_EXITSIZEMOVE:
        SaveWindowSizeIfNeeded();
        return 0;
    case WM_COMMAND:
        OnCommand(wParam, lParam);
        return 0;
    case WM_HOTKEY:
        OnHotkey(static_cast<UINT>(wParam));
        return 0;
    case WM_APP_GLOBAL_HOTKEY: {
        const ULONGLONG now = GetTickCount64();
        if (now - lastHotkeyTick_ < 200) {
            return 0;
        }
        lastHotkeyTick_ = now;
        if (settingsWindow_.IsOpen()) {
            return 0;
        }
        OnHotkey(static_cast<UINT>(wParam));
        return 0;
    }
    case WM_APP_STREAM_CHUNK: {
        std::unique_ptr<StreamChunkPayload> payload(reinterpret_cast<StreamChunkPayload*>(lParam));
        if (!payload || payload->requestId != activeRequestId_) {
            return 0;
        }
        if (payload->translate) {
            if (!translationVisible_) {
                translationVisible_ = true;
                Layout();
            }
            AppendTranslateText(payload->text);
        } else {
            AppendResultText(payload->text);
        }
        return 0;
    }
    case WM_APP_REQUEST_DONE: {
        std::unique_ptr<RequestDonePayload> payload(reinterpret_cast<RequestDonePayload*>(lParam));
        if (!payload || payload->requestId != activeRequestId_) {
            return 0;
        }
        const bool silentHotkeyOcr = activeHotkeySilentOcr_;
        requestInFlight_ = false;
        activeTranslateRequest_ = false;
        activeHotkeySilentOcr_ = false;
        const bool blankResult = payload->result.text.empty() || IsBlankText(payload->result.text);
        if (!payload->translate && blankResult) {
            payload->result.success = false;
            if (payload->result.error.empty()) {
                payload->result.error = L"No OCR text returned.";
            }
        }
        if (!payload->result.success) {
            if (!payload->translate) {
                pendingTranslateAfterOcr_ = false;
            }
            SetStatus(FormatStatus(payload->translate ? LoadS(IDS_STATUS_TRANSLATE_FAILED) : LoadS(IDS_STATUS_OCR_FAILED),
                                   payload->result.error));
            if (!payload->translate) {
                if (silentHotkeyOcr) {
                    ShowFromTray();
                }
                auto* err = new OcrErrorPayload();
                err->result = payload->result;
                if (!PostMessageW(hwnd_, WM_APP_SHOW_OCR_ERROR, 0, reinterpret_cast<LPARAM>(err))) {
                    delete err;
                    ShowOcrFailureDialog(payload->result);
                }
            } else {
                ShowServiceFailureDialog(LoadS(IDS_STATUS_TRANSLATE_FAILED), payload->result);
            }
            return 0;
        }
        if (payload->translate) {
            if (!activeRequestStream_) {
                translationVisible_ = true;
                RefreshTranslateText(payload->result.text);
                Layout();
            }
            SetStatus(LoadS(IDS_STATUS_TRANSLATE_DONE));
        } else {
            if (!activeRequestStream_) {
                RefreshResultText(payload->result.text);
            }
            SetStatus(LoadS(IDS_STATUS_OCR_DONE));
            if (silentHotkeyOcr) {
                if (CopyTextToClipboard(FilterOcrResult(payload->result.text), false)) {
                    ShowRecognitionToast();
                    return 0;
                }
                ShowFromTray();
                MessageBoxW(hwnd_,
                            L"Failed to copy OCR text to clipboard.",
                            LoadS(IDS_APP_TITLE).c_str(),
                            MB_OK | MB_ICONERROR);
                return 0;
            }
            if (pendingTranslateAfterOcr_) {
                pendingTranslateAfterOcr_ = false;
                BeginAsyncRequest(true, payload->result.text, L"");
            }
        }
        return 0;
    }
    case WM_APP_SHOW_OCR_ERROR: {
        std::unique_ptr<OcrErrorPayload> payload(reinterpret_cast<OcrErrorPayload*>(lParam));
        if (payload) {
            ShowOcrFailureDialog(payload->result);
        }
        return 0;
    }
    case WM_ERASEBKGND:
        if (host_.HandleEraseBackground(reinterpret_cast<HDC>(wParam))) {
            return 1;
        }
        return 0;
    case WM_CLOSE:
        HideToTray();
        return 0;
    case WM_DESTROY:
        OnDestroy();
        return 0;
    default:
        if (message == WM_APP + 1) {
            const UINT trayEvent = LOWORD(static_cast<DWORD_PTR>(lParam));
            if (trayEvent == WM_LBUTTONDBLCLK || trayEvent == WM_LBUTTONUP ||
                trayEvent == NIN_SELECT || trayEvent == NIN_KEYSELECT) {
                const ULONGLONG now = GetTickCount64();
                if (now - lastTrayToggleTick_ >= 250) {
                    lastTrayToggleTick_ = now;
                    ToggleTrayWindow();
                }
            }
            if (trayEvent == WM_RBUTTONUP || trayEvent == WM_CONTEXTMENU) {
                ShowTrayMenu();
            }
            return 0;
        }
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool MainWindow::OnCreate() {
    g_hotkeyMainHwnd = hwnd_;
    if (!hotkeyHook_) {
        hotkeyHook_ = SetWindowsHookExW(WH_KEYBOARD_LL, MainHotkeyHookProc, GetModuleHandleW(nullptr), 0);
    }

    darkui::ThemedWindowHost::Options hostOptions;
    hostOptions.theme = MakeAppTheme(config_.themeName, config_.fontSize);
    hostOptions.titleBarStyle = darkui::TitleBarStyle::Black;
    if (!host_.Attach(hwnd_, hostOptions)) {
        return false;
    }

    const BOOL immersive = TRUE;
    const COLORREF caption = RGB(10, 10, 12);
    const COLORREF captionText = RGB(240, 242, 245);
    DwmSetWindowAttribute(hwnd_, DWMWA_USE_IMMERSIVE_DARK_MODE, &immersive, sizeof(immersive));
    DwmSetWindowAttribute(hwnd_, DWMWA_CAPTION_COLOR, &caption, sizeof(caption));
    DwmSetWindowAttribute(hwnd_, DWMWA_TEXT_COLOR, &captionText, sizeof(captionText));

    darkui::Button::Options openOptions;
    openOptions.text = LoadS(IDS_TOOL_OPEN_IMAGE);
    openOptions.variant = darkui::ButtonVariant::Primary;
    openOptions.cornerRadius = 0;
    if (!openButton_.Create(hwnd_, ID_TOOL_OPEN_IMAGE, host_.theme(), openOptions)) {
        return false;
    }

    darkui::Button::Options translateOptions;
    translateOptions.text = LoadS(IDS_TOOL_TRANSLATE);
    translateOptions.variant = darkui::ButtonVariant::Secondary;
    translateOptions.cornerRadius = 0;
    if (!translateButton_.Create(hwnd_, ID_TOOL_TRANSLATE, host_.theme(), translateOptions)) {
        return false;
    }

    darkui::Button::Options copyOptions;
    copyOptions.text = LoadS(IDS_TOOL_COPY);
    copyOptions.variant = darkui::ButtonVariant::Secondary;
    copyOptions.cornerRadius = 0;
    if (!copyButton_.Create(hwnd_, ID_TOOL_COPY, host_.theme(), copyOptions)) {
        return false;
    }

    darkui::Button::Options settingsOptions;
    settingsOptions.text = LoadS(IDS_TOOL_SETTINGS);
    settingsOptions.variant = darkui::ButtonVariant::Ghost;
    settingsOptions.cornerRadius = 0;
    if (!settingsButton_.Create(hwnd_, ID_TOOL_SETTINGS, host_.theme(), settingsOptions)) {
        return false;
    }

    darkui::ComboBox::Options comboOptions;
    comboOptions.variant = darkui::FieldVariant::Default;
    comboOptions.cornerRadius = 0;
    comboOptions.popupWidth = 520;
    if (!modelCombo_.Create(hwnd_, ID_TOOL_MODEL_COMBO, host_.theme(), comboOptions)) {
        return false;
    }

    darkui::Edit::Options resultOptions;
    resultOptions.variant = darkui::FieldVariant::Default;
    resultOptions.cornerRadius = 0;
    resultOptions.style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL;
    if (!resultEdit_.Create(hwnd_, ID_MAIN_RESULT_EDIT, host_.theme(), resultOptions)) {
        return false;
    }
    darkui::Edit::Options translateOptionsEdit = resultOptions;
    if (!translateEdit_.Create(hwnd_, 7102, host_.theme(), translateOptionsEdit)) {
        return false;
    }
    ShowWindow(translateEdit_.hwnd(), SW_HIDE);

    host_.theme_manager().Bind(
        openButton_, translateButton_, copyButton_, settingsButton_, modelCombo_, resultEdit_, translateEdit_);
    host_.theme_manager().Apply();

    RefreshModelCombo();
    SetStatus(LoadS(IDS_STATUS_TRAY));
    Layout();
    UpdateTextEditScrollBars();
    AddTrayIcon();
    RegisterAppHotkeys();
    if (config_.startInTray) {
        ShowWindow(hwnd_, SW_HIDE);
    } else {
        CenterWindow();
        ShowWindowWithoutWhiteFlash(hwnd_, SW_SHOW);
        SetForegroundWindow(hwnd_);
    }
    return true;
}

void MainWindow::OnDestroy() {
    SaveWindowSizeIfNeeded();
    if (hotkeyHook_) {
        UnhookWindowsHookEx(hotkeyHook_);
        hotkeyHook_ = nullptr;
    }
    g_hotkeyMainHwnd = nullptr;
    RemoveTrayIcon();
    PostQuitMessage(0);
}

void MainWindow::OnSize() {
    if (hwnd_ && !IsZoomed(hwnd_) && !IsIconic(hwnd_)) {
        RECT windowRect{};
        GetWindowRect(hwnd_, &windowRect);
        const int width = windowRect.right - windowRect.left;
        const int height = windowRect.bottom - windowRect.top;
        if (width > 0 && height > 0 &&
            (config_.mainWindowWidth != width || config_.mainWindowHeight != height)) {
            config_.mainWindowWidth = width;
            config_.mainWindowHeight = height;
            windowSizeDirty_ = true;
        }
    }
    Layout();
}

void MainWindow::SaveWindowSizeIfNeeded() {
    if (!windowSizeDirty_ || !hwnd_ || IsZoomed(hwnd_) || IsIconic(hwnd_)) {
        return;
    }
    std::wstring error;
    if (configStore_.Save(config_, error)) {
        windowSizeDirty_ = false;
    }
}

void MainWindow::OnCommand(WPARAM wParam, LPARAM) {
    switch (LOWORD(wParam)) {
    case ID_APP_SHOW:
        ShowFromTray();
        return;
    case ID_APP_EXIT:
        DestroyWindow(hwnd_);
        return;
    case ID_TOOL_OPEN_IMAGE:
        OpenImage();
        return;
    case ID_TOOL_TRANSLATE:
        TranslateResult();
        return;
    case ID_TOOL_COPY:
        CopyResult();
        return;
    case ID_TOOL_SETTINGS:
        OpenSettings();
        return;
    case ID_TOOL_MODEL_COMBO:
        if (HIWORD(wParam) == CBN_SELCHANGE) {
            const int selection = modelCombo_.GetSelection();
            if (selection >= 0 && selection < static_cast<int>(modelChoices_.size())) {
                config_.defaultOcrProviderId = modelChoices_[selection].providerId;
                config_.defaultOcrModel = modelChoices_[selection].modelId;
                config_.defaultProviderId = config_.defaultOcrProviderId;
            }
        }
        return;
    case ID_MAIN_RESULT_EDIT:
    case 7102:
        if (HIWORD(wParam) == EN_CHANGE || HIWORD(wParam) == EN_UPDATE || HIWORD(wParam) == EN_VSCROLL) {
            UpdateTextEditScrollBars();
        }
        return;
    default:
        break;
    }
}

void MainWindow::OnHotkey(UINT id) {
    if (id == 1) {
        RunHotkeyCapture(false);
    } else if (id == 2) {
        RunSelectedTextTranslate();
    }
}

void MainWindow::Layout() {
    if (!hwnd_) {
        return;
    }
    RECT client{};
    GetClientRect(hwnd_, &client);

    const int margin = 5;
    const int top = margin;
    const int buttonHeight = 32;
    const int gap = 5;
    const int left = margin;
    const int rightMargin = margin;
    const int comboWidth = 280;
    const int settingsWidth = 84;
    const int secondaryWidth = 84;
    const int primaryWidth = 188;

    int x = left;
    MoveWindow(openButton_.hwnd(), x, top, primaryWidth, buttonHeight, TRUE);
    x += primaryWidth + gap;
    MoveWindow(translateButton_.hwnd(), x, top, secondaryWidth, buttonHeight, TRUE);
    x += secondaryWidth + gap;
    MoveWindow(copyButton_.hwnd(), x, top, secondaryWidth, buttonHeight, TRUE);

    const int comboLeft = client.right - rightMargin - settingsWidth - gap - comboWidth;
    MoveWindow(modelCombo_.hwnd(), comboLeft, top, comboWidth, buttonHeight, TRUE);
    MoveWindow(settingsButton_.hwnd(), client.right - rightMargin - settingsWidth, top, settingsWidth, buttonHeight, TRUE);

    const int contentTop = top + buttonHeight + gap;
    const int contentHeight = std::max(0, static_cast<int>(client.bottom) - contentTop - margin);
    const int contentWidth = std::max(0, static_cast<int>(client.right) - (margin * 2));
    if (translationVisible_) {
        const int paneGap = 5;
        const int paneWidth = (contentWidth - paneGap) / 2;
        MoveWindow(resultEdit_.hwnd(), margin, contentTop, paneWidth, contentHeight, TRUE);
        MoveWindow(translateEdit_.hwnd(), margin + paneWidth + paneGap, contentTop, paneWidth, contentHeight, TRUE);
        ShowWindow(translateEdit_.hwnd(), SW_SHOW);
    } else {
        MoveWindow(resultEdit_.hwnd(), margin, contentTop, contentWidth, contentHeight, TRUE);
        ShowWindow(translateEdit_.hwnd(), SW_HIDE);
    }
    UpdateTextEditScrollBars();
}

void MainWindow::ApplyConfigTheme() {
    host_.ApplyTheme(MakeAppTheme(config_.themeName, config_.fontSize));
    host_.theme_manager().Apply();
    Layout();
}

void MainWindow::RefreshModelCombo() {
    std::vector<darkui::ComboItem> items;
    modelChoices_.clear();
    for (const auto& provider : config_.providers) {
        if (!provider.enabled) {
            continue;
        }
        for (const auto& model : provider.models) {
            if (!model.enabled) {
                continue;
            }
            const std::wstring itemText = provider.name + L" - " + model.id;
            darkui::ComboItem item;
            item.text = itemText;
            item.displayText = ModelComboDisplayText(itemText);
            item.userData = modelChoices_.size();
            item.accent = false;
            items.push_back(std::move(item));
            modelChoices_.push_back({provider.id, model.id});
        }
    }
    modelCombo_.SetItems(items);
    if (modelChoices_.empty()) {
        config_.defaultOcrProviderId.clear();
        config_.defaultOcrModel.clear();
        return;
    }
    for (int i = 0; i < static_cast<int>(modelCombo_.GetCount()); ++i) {
        if (modelChoices_[i].providerId == config_.defaultOcrProviderId &&
            modelChoices_[i].modelId == config_.defaultOcrModel) {
            modelCombo_.SetSelection(i, false);
            return;
        }
    }
    if (modelCombo_.GetCount() > 0) {
        modelCombo_.SetSelection(0, false);
        config_.defaultOcrProviderId = modelChoices_[0].providerId;
        config_.defaultOcrModel = modelChoices_[0].modelId;
        config_.defaultProviderId = config_.defaultOcrProviderId;
    } else {
        config_.defaultOcrProviderId.clear();
        config_.defaultOcrModel.clear();
    }
}

std::wstring MainWindow::ApplyResultFilter(const std::wstring& text, const std::wstring& filterExpression) const {
    if (text.empty() || filterExpression.empty()) {
        return text;
    }

    std::wstring filtered = text;
    for (const std::wstring& rawPattern : SplitFilterPatterns(filterExpression)) {
        if (rawPattern.empty()) {
            continue;
        }
        try {
            const std::wregex pattern(rawPattern, std::regex_constants::ECMAScript);
            filtered = std::regex_replace(filtered, pattern, std::wstring());
        } catch (const std::regex_error&) {
            continue;
        }
    }
    return filtered;
}

std::wstring MainWindow::FilterOcrResult(const std::wstring& text) const {
    return ApplyResultFilter(text, config_.ocrResultFilter);
}

std::wstring MainWindow::FilterTranslateResult(const std::wstring& text) const {
    return ApplyResultFilter(text, config_.translateResultFilter);
}

void MainWindow::RefreshResultText(const std::wstring& text) {
    resultEdit_.SetText(FilterOcrResult(text));
    UpdateTextEditScrollBars();
}

void MainWindow::RefreshTranslateText(const std::wstring& text) {
    translateEdit_.SetText(FilterTranslateResult(text));
    UpdateTextEditScrollBars();
}

void MainWindow::AppendResultText(const std::wstring& text) {
    const std::wstring filteredText = FilterOcrResult(text);
    if (filteredText.empty()) {
        return;
    }
    HWND edit = resultEdit_.edit_hwnd();
    SendMessageW(edit, EM_SETSEL, static_cast<WPARAM>(-1), static_cast<LPARAM>(-1));
    SendMessageW(edit, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(filteredText.c_str()));
    RedrawWindow(resultEdit_.hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
    UpdateTextEditScrollBars();
}

void MainWindow::AppendTranslateText(const std::wstring& text) {
    const std::wstring filteredText = FilterTranslateResult(text);
    if (filteredText.empty()) {
        return;
    }
    HWND edit = translateEdit_.edit_hwnd();
    SendMessageW(edit, EM_SETSEL, static_cast<WPARAM>(-1), static_cast<LPARAM>(-1));
    SendMessageW(edit, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(filteredText.c_str()));
    RedrawWindow(translateEdit_.hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
    UpdateTextEditScrollBars();
}

void MainWindow::UpdateTextEditScrollBars() {
    UpdateEditVerticalScrollBar(resultEdit_, true);
    UpdateEditVerticalScrollBar(translateEdit_, translationVisible_);
}

void MainWindow::UpdateEditVerticalScrollBar(darkui::Edit& edit, bool visible) {
    HWND editHwnd = edit.edit_hwnd();
    if (!editHwnd || !IsWindow(editHwnd)) {
        return;
    }
    if (!visible || !IsWindowVisible(edit.hwnd())) {
        edit.SetVerticalScrollVisible(false);
        return;
    }

    RECT client{};
    GetClientRect(editHwnd, &client);
    HDC dc = GetDC(editHwnd);
    TEXTMETRICW metrics{};
    if (dc) {
        HFONT font = reinterpret_cast<HFONT>(SendMessageW(editHwnd, WM_GETFONT, 0, 0));
        HFONT oldFont = font ? reinterpret_cast<HFONT>(SelectObject(dc, font)) : nullptr;
        GetTextMetricsW(dc, &metrics);
        if (oldFont) {
            SelectObject(dc, oldFont);
        }
        ReleaseDC(editHwnd, dc);
    }
    const int lineHeight = std::max(1, static_cast<int>(metrics.tmHeight));
    const int clientHeight = static_cast<int>(client.bottom - client.top);
    const int visibleLines = std::max(1, clientHeight / lineHeight);
    const int lineCount = static_cast<int>(SendMessageW(editHwnd, EM_GETLINECOUNT, 0, 0));
    edit.SetVerticalScrollVisible(lineCount > visibleLines);
}

void MainWindow::SetStatus(const std::wstring& text) {
    statusText_ = text;
}

void MainWindow::ShowFromTray() {
    CenterWindow();
    if (IsIconic(hwnd_)) {
        ShowWindow(hwnd_, SW_RESTORE);
    } else {
        ShowWindowWithoutWhiteFlash(hwnd_, SW_SHOW);
    }
    SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    SetWindowPos(hwnd_, HWND_NOTOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    BringWindowToTop(hwnd_);
    SetActiveWindow(hwnd_);
    SetForegroundWindow(hwnd_);
    SetFocus(hwnd_);
}

void MainWindow::HideToTray() {
    ShowWindow(hwnd_, SW_HIDE);
    SetStatus(LoadS(IDS_STATUS_TRAY));
}

void MainWindow::ToggleTrayWindow() {
    const bool visible = IsWindowVisible(hwnd_) != FALSE;
    const bool iconic = IsIconic(hwnd_) != FALSE;
    if (visible && !iconic) {
        HideToTray();
    } else {
        ShowFromTray();
    }
}

void MainWindow::CenterWindow() {
    RECT windowRect{};
    GetWindowRect(hwnd_, &windowRect);
    const int width = windowRect.right - windowRect.left;
    const int height = windowRect.bottom - windowRect.top;
    RECT workArea{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    const int left = workArea.left + ((workArea.right - workArea.left) - width) / 2;
    const int top = workArea.top + ((workArea.bottom - workArea.top) - height) / 2;
    SetWindowPos(hwnd_, nullptr, left, top, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
}

void MainWindow::AddTrayIcon() {
    if (trayAdded_) {
        return;
    }
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd_;
    nid.uID = ID_TRAYICON;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = WM_APP + 1;
    nid.hIcon = appIconSmall_ ? appIconSmall_ : appIconLarge_;
    wcsncpy_s(nid.szTip, LoadS(IDS_TRAY_TIP).c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_DELETE, &nid);
    trayAdded_ = Shell_NotifyIconW(NIM_ADD, &nid) == TRUE;
    if (trayAdded_) {
        nid.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &nid);
    }
}

void MainWindow::RemoveTrayIcon() {
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd_;
    nid.uID = ID_TRAYICON;
    Shell_NotifyIconW(NIM_DELETE, &nid);
    trayAdded_ = false;
}

void MainWindow::ShowTrayMenu() {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, ID_APP_SHOW, LoadS(IDS_APP_TITLE).c_str());
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_APP_EXIT, LoadS(IDS_CLOSE).c_str());
    POINT cursor{};
    GetCursorPos(&cursor);
    SetForegroundWindow(hwnd_);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, cursor.x, cursor.y, 0, hwnd_, nullptr);
    DestroyMenu(menu);
}

void MainWindow::OpenImage() {
    wchar_t path[MAX_PATH] = {};
    static const wchar_t kImageFilter[] =
        L"Image Files (*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.webp)\0"
        L"*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.webp\0"
        L"All Files (*.*)\0"
        L"*.*\0";

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd_;
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = kImageFilter;
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameW(&ofn)) {
        selectedImagePath_ = path;
        selectedImageDataUrl_.clear();
        RunOcr();
    }
}

void MainWindow::RunHotkeyCapture(bool translateAfterOcr) {
    pendingTranslateAfterOcr_ = false;
    pendingHotkeySilentOcr_ = false;
    selectedImagePath_.clear();
    selectedImageDataUrl_.clear();
    if (requestInFlight_) {
        MessageBoxW(hwnd_, L"A request is already running. Please wait.", LoadS(IDS_APP_TITLE).c_str(), MB_OK | MB_ICONINFORMATION);
        return;
    }

    const bool wasVisible = IsWindowVisible(hwnd_) != FALSE;
    if (wasVisible) {
        ShowWindow(hwnd_, SW_HIDE);
    }
    Sleep(100);

    std::string imageDataUrl;
    bool canceled = false;
    const bool captured = CaptureSelectionToDataUrl(imageDataUrl, canceled);
    if (!captured) {
        lastHotkeyTick_ = 0;
        EnableWindow(hwnd_, TRUE);
        if (canceled) {
            if (wasVisible) {
                ShowFromTray();
            }
            return;
        }
        if (!canceled) {
            ShowFromTray();
            EnableWindow(hwnd_, TRUE);
            SetForegroundWindow(hwnd_);
        }
        return;
    }

    selectedImagePath_.clear();
    selectedImageDataUrl_ = std::move(imageDataUrl);
    pendingTranslateAfterOcr_ = false;
    const bool silentHotkeyOcr = !translateAfterOcr && config_.copyAfterHotkeyOcr;
    if (silentHotkeyOcr) {
        ShowWindow(hwnd_, SW_HIDE);
        EnableWindow(hwnd_, TRUE);
    } else {
        ShowFromTray();
        EnableWindow(hwnd_, TRUE);
        SetForegroundWindow(hwnd_);
    }
    pendingHotkeySilentOcr_ = silentHotkeyOcr;
    if (translateAfterOcr) {
        RunTranslateImage();
    } else {
        RunOcr();
    }
}

void MainWindow::RunSelectedTextTranslate() {
    if (requestInFlight_) {
        MessageBoxW(hwnd_, L"A request is already running. Please wait.", LoadS(IDS_APP_TITLE).c_str(), MB_OK | MB_ICONINFORMATION);
        return;
    }

    std::wstring selectedText;
    if (!CaptureSelectedTextViaClipboard(hwnd_, selectedText) || TrimCopy(selectedText).empty()) {
        ShowFromTray();
        MessageBoxW(hwnd_,
                    L"Failed to get selected text from the foreground application.",
                    LoadS(IDS_APP_TITLE).c_str(),
                    MB_OK | MB_ICONINFORMATION);
        return;
    }

    pendingTranslateAfterOcr_ = false;
    pendingHotkeySilentOcr_ = false;
    activeHotkeySilentOcr_ = false;
    translationVisible_ = true;
    selectedImagePath_.clear();
    selectedImageDataUrl_.clear();
    RefreshResultText(selectedText);
    RefreshTranslateText(L"");
    ShowFromTray();
    Layout();
    BeginAsyncRequest(true, selectedText, L"");
}

bool MainWindow::CaptureSelectionToDataUrl(std::string& imageDataUrl, bool& canceled) {
    canceled = false;
    CaptureOverlay overlay(instance_);
    RECT selected {};
    if (!overlay.SelectArea(nullptr, &selected)) {
        canceled = true;
        return false;
    }

    ScreenshotService screenshot;
    ScreenshotResult shot = screenshot.CaptureArea(selected);
    if (!shot.bitmap) {
        return false;
    }

    const bool ok = screenshot.BitmapToDataUrl(shot.bitmap, imageDataUrl);
    DeleteObject(shot.bitmap);
    if (!ok) {
        MessageBoxW(hwnd_, L"Failed to encode captured image.", LoadS(IDS_APP_TITLE).c_str(), MB_OK | MB_ICONERROR);
        return false;
    }
    return true;
}

void MainWindow::RunOcr() {
    const ProviderConfig* provider = DefaultProvider();
    if (!provider) {
        return;
    }
    if (selectedImagePath_.empty() && selectedImageDataUrl_.empty()) {
        MessageBoxW(hwnd_, LoadS(IDS_MSG_SELECT_IMAGE_FIRST).c_str(), LoadS(IDS_APP_TITLE).c_str(), MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (provider->apiKey.empty()) {
        MessageBoxW(hwnd_, LoadS(IDS_MSG_INPUT_API_KEY).c_str(), LoadS(IDS_APP_TITLE).c_str(), MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (selectedImageDataUrl_.empty() && !selectedImagePath_.empty() && EndsWithNoCase(selectedImagePath_, L".bmp")) {
        HBITMAP bmp = static_cast<HBITMAP>(LoadImageW(nullptr, selectedImagePath_.c_str(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE));
        if (bmp) {
            ScreenshotService screenshot;
            std::string dataUrl;
            if (screenshot.BitmapToDataUrl(bmp, dataUrl)) {
                selectedImageDataUrl_ = std::move(dataUrl);
                selectedImagePath_.clear();
            }
            DeleteObject(bmp);
        }
    }
    BeginAsyncRequest(false, L"", selectedImagePath_);
}

void MainWindow::RunTranslateImage() {
    const ProviderConfig* provider = DefaultProvider();
    if (!provider) {
        return;
    }
    if (provider->apiKey.empty()) {
        MessageBoxW(hwnd_, LoadS(IDS_MSG_INPUT_API_KEY).c_str(), LoadS(IDS_APP_TITLE).c_str(), MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (selectedImageDataUrl_.empty()) {
        MessageBoxW(hwnd_, LoadS(IDS_MSG_SELECT_IMAGE_FIRST).c_str(), LoadS(IDS_APP_TITLE).c_str(), MB_OK | MB_ICONINFORMATION);
        return;
    }
    BeginAsyncRequest(true, L"", L"");
}

void MainWindow::BeginAsyncRequest(bool translateOnly, const std::wstring& inputText, const std::wstring& imagePath) {
    if (requestInFlight_) {
        SetStatus(L"Request is already running.");
        MessageBoxW(hwnd_, L"A request is already running. Please wait.", LoadS(IDS_APP_TITLE).c_str(), MB_OK | MB_ICONINFORMATION);
        return;
    }
    const std::wstring providerId = translateOnly ? config_.defaultTranslateProviderId : config_.defaultOcrProviderId;
    const ProviderConfig* provider = nullptr;
    for (const auto& item : config_.providers) {
        if (item.enabled && item.id == providerId) {
            provider = &item;
            break;
        }
    }
    if (!provider) {
        provider = DefaultProvider();
    }
    if (!provider) {
        MessageBoxW(hwnd_, L"No enabled provider.", LoadS(IDS_APP_TITLE).c_str(), MB_OK | MB_ICONINFORMATION);
        return;
    }
    ProviderConfig providerCopy = *provider;
    std::wstring modelId = translateOnly ? config_.defaultTranslateModel : config_.defaultOcrModel;
    if (modelId.empty()) {
        const std::wstring prompt = translateOnly
            ? (LoadS(IDS_DEFAULT_TRANSLATE_MODEL) + L" is not set.")
            : (LoadS(IDS_DEFAULT_OCR_MODEL) + L" is not set.");
        MessageBoxW(hwnd_, prompt.c_str(), LoadS(IDS_APP_TITLE).c_str(), MB_OK | MB_ICONINFORMATION);
        return;
    }
    bool modelReasoning = false;
    bool hasModelReasoning = false;
    for (const auto& model : providerCopy.models) {
        if (model.id == modelId) {
            modelReasoning = model.reasoning;
            hasModelReasoning = true;
            break;
        }
    }
    RequestOptions options;
    options.stream = providerCopy.streamResponse;
    options.enableReasoning = modelReasoning;
    options.includeReasoningOption = hasModelReasoning && modelReasoning;
    options.ocrPrompt = config_.ocrPrompt;
    options.translateTextPrompt = config_.translateTextPrompt;
    // OCR request should fail fast for silent/non-responsive models.
    options.timeoutMs = translateOnly ? 30000 : static_cast<DWORD>(std::clamp(config_.ocrTimeoutSeconds, 1, 300) * 1000);

    requestInFlight_ = true;
    activeTranslateRequest_ = translateOnly;
    activeRequestStream_ = options.stream;
    activeHotkeySilentOcr_ = !translateOnly && pendingHotkeySilentOcr_;
    pendingHotkeySilentOcr_ = false;
    activeRequestId_ = ++requestCounter_;
    if (translateOnly) {
        translationVisible_ = true;
        RefreshTranslateText(L"");
        Layout();
        SetStatus(LoadS(IDS_STATUS_TRANSLATING));
    } else {
        RefreshResultText(L"");
        translationVisible_ = false;
        RefreshTranslateText(L"");
        Layout();
        SetStatus(LoadS(IDS_STATUS_OCR_RUNNING));
    }

    const unsigned long long requestId = activeRequestId_;
    const std::string imageDataUrl = selectedImageDataUrl_;
    HWND targetWindow = hwnd_;
    std::thread([this, providerCopy, modelId, options, translateOnly, inputText, imagePath, imageDataUrl, requestId, targetWindow]() {
        auto postChunk = [targetWindow, requestId, translateOnly](const std::wstring& chunk) {
            auto* payload = new StreamChunkPayload();
            payload->requestId = requestId;
            payload->translate = translateOnly;
            payload->text = chunk;
            if (!PostMessageW(targetWindow, WM_APP_STREAM_CHUNK, 0, reinterpret_cast<LPARAM>(payload))) {
                delete payload;
            }
        };

        ServiceResult result = translateOnly
            ? (inputText.empty() && !imageDataUrl.empty()
                   ? service_.TranslateImageDataUrl(providerCopy, modelId, imageDataUrl, options,
                                                   options.stream ? OcrService::StreamCallback(postChunk) : OcrService::StreamCallback())
                   : service_.TranslateText(providerCopy, modelId, inputText, options,
                                            options.stream ? OcrService::StreamCallback(postChunk) : OcrService::StreamCallback()))
            : (imageDataUrl.empty()
                   ? service_.RecognizeImage(providerCopy, modelId, imagePath, options, options.stream ? OcrService::StreamCallback(postChunk) : OcrService::StreamCallback())
                   : service_.RecognizeImageDataUrl(providerCopy, modelId, imageDataUrl, options, options.stream ? OcrService::StreamCallback(postChunk) : OcrService::StreamCallback()));

        auto* done = new RequestDonePayload();
        done->requestId = requestId;
        done->translate = translateOnly;
        done->result = std::move(result);
        if (!PostMessageW(targetWindow, WM_APP_REQUEST_DONE, 0, reinterpret_cast<LPARAM>(done))) {
            delete done;
        }
    }).detach();
}

void MainWindow::ShowOcrFailureDialog(const ServiceResult& result) {
    ShowServiceFailureDialog(LoadS(IDS_STATUS_OCR_FAILED), result);
}

void MainWindow::ShowServiceFailureDialog(const std::wstring& title, const ServiceResult& result) {
    struct OcrFailureDialogSession {
        darkui::ThemeManager themeManager;
        darkui::Dialog dialog;
        darkui::Panel panel;
        darkui::Static label;
        darkui::Edit edit;
    };

    OcrFailureDialogSession session;
    darkui::Dialog::Options dialogOptions;
    dialogOptions.title = title;
    dialogOptions.confirmText = LoadS(IDS_CLOSE);
    dialogOptions.cancelVisible = false;
    dialogOptions.width = 860;
    dialogOptions.height = 620;
    dialogOptions.messageVisible = false;
    if (!session.dialog.Create(hwnd_, 9050, host_.theme(), dialogOptions)) {
        MessageBoxW(hwnd_, result.error.empty() ? L"Request failed." : result.error.c_str(), title.c_str(), MB_OK | MB_ICONERROR);
        return;
    }

    darkui::Panel::Options panelOptions;
    panelOptions.role = darkui::SurfaceRole::Panel;
    panelOptions.cornerRadius = 16;
    darkui::Static::Options labelOptions;
    labelOptions.variant = darkui::StaticVariant::PanelTitle;
    labelOptions.text = L"Error / Request / Raw Response (copyable)";

    darkui::Edit::Options editOptions;
    editOptions.variant = darkui::FieldVariant::Panel;
    editOptions.style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL;

    if (!session.panel.Create(session.dialog.content_hwnd(), 9051, host_.theme(), panelOptions) ||
        !session.label.Create(session.panel.hwnd(), 9052, host_.theme(), labelOptions) ||
        !session.edit.Create(session.panel.hwnd(), 9053, host_.theme(), editOptions)) {
        MessageBoxW(hwnd_, result.error.empty() ? L"Request failed." : result.error.c_str(), title.c_str(), MB_OK | MB_ICONERROR);
        return;
    }

    std::wstring content = L"[Error]\r\n";
    content += ClipForDialog(result.error.empty() ? L"Request failed." : result.error, 4000) + L"\r\n\r\n";
    content += L"[Request JSON / Params]\r\n";
    content += result.requestText.empty() ? L"(empty)\r\n\r\n" : (ClipForDialog(result.requestText, 12000) + L"\r\n\r\n");
    content += L"[Raw Response]\r\n";
    content += result.responseText.empty() ? L"(empty)\r\n" : ClipForDialog(result.responseText, 12000);

    session.edit.SetText(content);
    session.themeManager.SetTheme(host_.theme());
    session.themeManager.Bind(session.dialog, session.panel, session.label, session.edit);
    MoveWindow(session.panel.hwnd(), 16, 12, 772, 466, TRUE);
    MoveWindow(session.label.hwnd(), 14, 12, 600, 28, TRUE);
    MoveWindow(session.edit.hwnd(), 14, 46, 744, 404, TRUE);
    session.dialog.ShowModal();
}

void MainWindow::TranslateResult() {
    if (translationVisible_) {
        translationVisible_ = false;
        Layout();
        return;
    }
    if (requestInFlight_) {
        MessageBoxW(hwnd_, L"A request is already running. Please wait.", LoadS(IDS_APP_TITLE).c_str(), MB_OK | MB_ICONINFORMATION);
        return;
    }
    const ProviderConfig* provider = DefaultProvider();
    if (!provider || provider->apiKey.empty()) {
        MessageBoxW(hwnd_, LoadS(IDS_MSG_INPUT_API_KEY).c_str(), LoadS(IDS_APP_TITLE).c_str(), MB_OK | MB_ICONINFORMATION);
        return;
    }
    const std::wstring currentText = resultEdit_.GetText();
    if (currentText.empty()) {
        MessageBoxW(hwnd_, LoadS(IDS_MSG_COPY_EMPTY).c_str(), LoadS(IDS_APP_TITLE).c_str(), MB_OK | MB_ICONINFORMATION);
        return;
    }
    BeginAsyncRequest(true, currentText, L"");
}

void MainWindow::CopyResult() {
    const std::wstring text = translationVisible_ ? translateEdit_.GetText() : resultEdit_.GetText();
    if (text.empty()) {
        MessageBoxW(hwnd_, LoadS(IDS_MSG_COPY_EMPTY).c_str(), LoadS(IDS_APP_TITLE).c_str(), MB_OK | MB_ICONINFORMATION);
        return;
    }
    CopyTextToClipboard(text, true);
}

void MainWindow::OpenSettings() {
    if (!settingsWindow_.IsOpen()) {
        settingsWindow_.Create(instance_, hwnd_);
    }
    settingsWindow_.Show();
}

void MainWindow::RegisterAppHotkeys() {
    g_ocrHotkey = config_.ocrHotkey;
    g_translateHotkey = config_.translateHotkey;
}

bool MainWindow::CopyTextToClipboard(const std::wstring& text, bool updateStatus) {
    if (text.empty()) {
        return false;
    }
    if (!OpenClipboard(hwnd_)) {
        return false;
    }
    EmptyClipboard();
    const std::size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    bool success = false;
    if (memory) {
        void* locked = GlobalLock(memory);
        if (locked) {
            memcpy(locked, text.c_str(), bytes);
            GlobalUnlock(memory);
            success = SetClipboardData(CF_UNICODETEXT, memory) != nullptr;
        }
        if (!success) {
            GlobalFree(memory);
        }
    }
    CloseClipboard();
    if (success && updateStatus) {
        SetStatus(LoadS(IDS_STATUS_COPY_DONE));
    }
    return success;
}

void MainWindow::ShowRecognitionToast() {
    if (!EnsureRecognitionToastClass(instance_)) {
        return;
    }
    auto* state = new RecognitionToastState();
    state->text = L"已识别";
    HDC screenDc = GetDC(nullptr);
    const int dpi = screenDc ? GetDeviceCaps(screenDc, LOGPIXELSY) : 96;
    if (screenDc) {
        ReleaseDC(nullptr, screenDc);
    }
    state->font = CreateFontW(-MulDiv(46, dpi, 72),
                              0,
                              0,
                              0,
                              FW_SEMIBOLD,
                              FALSE,
                              FALSE,
                              FALSE,
                              DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS,
                              CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY,
                              DEFAULT_PITCH | FF_DONTCARE,
                              L"Microsoft YaHei UI");

    RECT workArea{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    const int width = 320;
    const int height = 96;
    const int left = workArea.left + ((workArea.right - workArea.left) - width) / 2;
    const int top = workArea.top + ((workArea.bottom - workArea.top) - height) / 2;
    HWND toast = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
                                 kRecognitionToastClassName,
                                 L"",
                                 WS_POPUP,
                                 left,
                                 top,
                                 width,
                                 height,
                                 nullptr,
                                 nullptr,
                                 instance_,
                                 state);
    if (!toast) {
        if (state->font) {
            DeleteObject(state->font);
        }
        delete state;
        return;
    }
    SetLayeredWindowAttributes(toast, RGB(0, 0, 0), 0, LWA_COLORKEY);
    ShowWindow(toast, SW_SHOWNOACTIVATE);
    UpdateWindow(toast);
}

const ProviderConfig* MainWindow::DefaultProvider() const {
    for (const auto& provider : config_.providers) {
        if (provider.enabled && provider.id == config_.defaultOcrProviderId) {
            return &provider;
        }
    }
    for (const auto& provider : config_.providers) {
        if (provider.enabled && provider.id == config_.defaultProviderId) {
            return &provider;
        }
    }
    for (const auto& provider : config_.providers) {
        if (provider.enabled) {
            return &provider;
        }
    }
    return nullptr;
}

}  // namespace app


