#include "MainWindow.h"

#include "AppIds.h"
#include "AppTheme.h"
#include "UiText.h"

#include <dwmapi.h>
#include <commdlg.h>
#include <cstring>
#include <shellapi.h>

namespace app {

namespace {

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

std::wstring LoadS(UINT id) {
    return LoadStringResource(id);
}

std::vector<darkui::ComboItem> ToModelItems(const ProviderConfig& provider) {
    std::vector<darkui::ComboItem> items;
    for (std::size_t i = 0; i < provider.models.size(); ++i) {
        if (provider.models[i].enabled) {
            items.push_back({provider.models[i].id, i, false});
        }
    }
    return items;
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
}

bool MainWindow::Create(HINSTANCE instance) {
    instance_ = instance;

    std::wstring error;
    configStore_.Load(config_, error);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance_;
    wc.lpszClassName = L"Win32OcrMainWindow";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(18, 20, 24));
    RegisterClassExW(&wc);

    hwnd_ = CreateWindowExW(WS_EX_APPWINDOW,
                            wc.lpszClassName,
                            LoadS(IDS_APP_TITLE).c_str(),
                            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                            CW_USEDEFAULT,
                            CW_USEDEFAULT,
                            1040,
                            720,
                            nullptr,
                            nullptr,
                            instance_,
                            this);
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
    case WM_COMMAND:
        OnCommand(wParam, lParam);
        return 0;
    case WM_HOTKEY:
        OnHotkey(static_cast<UINT>(wParam));
        return 0;
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
            if (lParam == WM_LBUTTONDBLCLK) ShowFromTray();
            if (lParam == WM_RBUTTONUP) ShowTrayMenu();
            return 0;
        }
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool MainWindow::OnCreate() {
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
    if (!openButton_.Create(hwnd_, ID_TOOL_OPEN_IMAGE, host_.theme(), openOptions)) {
        return false;
    }

    darkui::Button::Options translateOptions;
    translateOptions.text = LoadS(IDS_TOOL_TRANSLATE);
    translateOptions.variant = darkui::ButtonVariant::Secondary;
    if (!translateButton_.Create(hwnd_, ID_TOOL_TRANSLATE, host_.theme(), translateOptions)) {
        return false;
    }

    darkui::Button::Options copyOptions;
    copyOptions.text = LoadS(IDS_TOOL_COPY);
    copyOptions.variant = darkui::ButtonVariant::Secondary;
    if (!copyButton_.Create(hwnd_, ID_TOOL_COPY, host_.theme(), copyOptions)) {
        return false;
    }

    darkui::Button::Options settingsOptions;
    settingsOptions.text = LoadS(IDS_TOOL_SETTINGS);
    settingsOptions.variant = darkui::ButtonVariant::Ghost;
    if (!settingsButton_.Create(hwnd_, ID_TOOL_SETTINGS, host_.theme(), settingsOptions)) {
        return false;
    }

    darkui::ComboBox::Options comboOptions;
    comboOptions.variant = darkui::FieldVariant::Default;
    if (!modelCombo_.Create(hwnd_, ID_TOOL_MODEL_COMBO, host_.theme(), comboOptions)) {
        return false;
    }

    darkui::Edit::Options resultOptions;
    resultOptions.variant = darkui::FieldVariant::Default;
    resultOptions.style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL;
    if (!resultEdit_.Create(hwnd_, ID_MAIN_RESULT_EDIT, host_.theme(), resultOptions)) {
        return false;
    }
    resultEdit_.SetReadOnly(true);

    host_.theme_manager().Bind(
        openButton_, translateButton_, copyButton_, settingsButton_, modelCombo_, resultEdit_);
    host_.theme_manager().Apply();

    RefreshModelCombo();
    SetStatus(LoadS(IDS_STATUS_TRAY));
    Layout();
    AddTrayIcon();
    RegisterAppHotkeys();
    ShowWindow(hwnd_, SW_HIDE);
    return true;
}

void MainWindow::OnDestroy() {
    RemoveTrayIcon();
    PostQuitMessage(0);
}

void MainWindow::OnSize() {
    Layout();
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
            config_.defaultOcrModel = modelCombo_.GetText();
        }
        return;
    default:
        break;
    }
}

void MainWindow::OnHotkey(UINT id) {
    if (id == 1) {
        ShowFromTray();
    } else if (id == 2) {
        TranslateResult();
    }
}

void MainWindow::Layout() {
    if (!hwnd_) {
        return;
    }
    RECT client{};
    GetClientRect(hwnd_, &client);

    const int top = 24;
    const int buttonHeight = 36;
    const int gap = 12;
    const int left = 28;
    const int rightMargin = 28;
    const int comboWidth = 250;
    const int settingsWidth = 96;
    const int secondaryWidth = 96;
    const int primaryWidth = 132;

    int x = left;
    MoveWindow(openButton_.hwnd(), x, top, primaryWidth, buttonHeight, TRUE);
    x += primaryWidth + gap;
    MoveWindow(translateButton_.hwnd(), x, top, secondaryWidth, buttonHeight, TRUE);
    x += secondaryWidth + gap;
    MoveWindow(copyButton_.hwnd(), x, top, secondaryWidth, buttonHeight, TRUE);

    const int comboLeft = client.right - rightMargin - settingsWidth - gap - comboWidth;
    MoveWindow(modelCombo_.hwnd(), comboLeft, top, comboWidth, buttonHeight, TRUE);
    MoveWindow(settingsButton_.hwnd(), client.right - rightMargin - settingsWidth, top, settingsWidth, buttonHeight, TRUE);
    MoveWindow(resultEdit_.hwnd(), 28, 84, client.right - 56, client.bottom - 112, TRUE);
}

void MainWindow::ApplyConfigTheme() {
    host_.ApplyTheme(MakeAppTheme(config_.themeName, config_.fontSize));
    host_.theme_manager().Apply();
    Layout();
}

void MainWindow::RefreshModelCombo() {
    const ProviderConfig* provider = DefaultProvider();
    if (!provider) {
        return;
    }
    modelCombo_.SetItems(ToModelItems(*provider));
    for (int i = 0; i < static_cast<int>(modelCombo_.GetCount()); ++i) {
        if (modelCombo_.GetItem(i).text == config_.defaultOcrModel) {
            modelCombo_.SetSelection(i, false);
            return;
        }
    }
    if (modelCombo_.GetCount() > 0) {
        modelCombo_.SetSelection(0, false);
        config_.defaultOcrModel = modelCombo_.GetText();
    }
}

void MainWindow::RefreshResultText(const std::wstring& text) {
    resultEdit_.SetText(text);
}

void MainWindow::SetStatus(const std::wstring& text) {
    statusText_ = text;
}

void MainWindow::ShowFromTray() {
    CenterWindow();
    ShowWindow(hwnd_, SW_SHOW);
    SetForegroundWindow(hwnd_);
}

void MainWindow::HideToTray() {
    ShowWindow(hwnd_, SW_HIDE);
    SetStatus(LoadS(IDS_STATUS_TRAY));
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
    nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcsncpy_s(nid.szTip, LoadS(IDS_TRAY_TIP).c_str(), _TRUNCATE);
    trayAdded_ = Shell_NotifyIconW(NIM_ADD, &nid) == TRUE;
}

void MainWindow::RemoveTrayIcon() {
    if (!trayAdded_) {
        return;
    }
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
    std::wstring filter = LoadS(IDS_MSG_IMAGE_DIALOG_FILTER);
    filter += L"\0*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.webp\0";
    filter += L"All Files\0*.*\0";

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd_;
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = filter.c_str();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameW(&ofn)) {
        selectedImagePath_ = path;
        RunOcr();
    }
}

void MainWindow::RunOcr() {
    const ProviderConfig* provider = DefaultProvider();
    if (!provider) {
        return;
    }
    if (selectedImagePath_.empty()) {
        MessageBoxW(hwnd_, LoadS(IDS_MSG_SELECT_IMAGE_FIRST).c_str(), LoadS(IDS_APP_TITLE).c_str(), MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (provider->apiKey.empty()) {
        MessageBoxW(hwnd_, LoadS(IDS_MSG_INPUT_API_KEY).c_str(), LoadS(IDS_APP_TITLE).c_str(), MB_OK | MB_ICONINFORMATION);
        return;
    }

    SetStatus(LoadS(IDS_STATUS_OCR_RUNNING));
    const ServiceResult result = service_.RecognizeImage(*provider, config_.defaultOcrModel, selectedImagePath_);
    if (!result.success) {
        SetStatus(FormatStatus(LoadS(IDS_STATUS_OCR_FAILED), result.error));
        return;
    }
    RefreshResultText(result.text);
    SetStatus(LoadS(IDS_STATUS_OCR_DONE));
}

void MainWindow::TranslateResult() {
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

    SetStatus(LoadS(IDS_STATUS_TRANSLATING));
    const ServiceResult result = service_.TranslateText(*provider, config_.defaultTranslateModel, currentText);
    if (!result.success) {
        SetStatus(FormatStatus(LoadS(IDS_STATUS_TRANSLATE_FAILED), result.error));
        return;
    }
    RefreshResultText(result.text);
    SetStatus(LoadS(IDS_STATUS_TRANSLATE_DONE));
}

void MainWindow::CopyResult() {
    const std::wstring text = resultEdit_.GetText();
    if (text.empty()) {
        MessageBoxW(hwnd_, LoadS(IDS_MSG_COPY_EMPTY).c_str(), LoadS(IDS_APP_TITLE).c_str(), MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (!OpenClipboard(hwnd_)) {
        return;
    }
    EmptyClipboard();
    const std::size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (memory) {
        void* locked = GlobalLock(memory);
        memcpy(locked, text.c_str(), bytes);
        GlobalUnlock(memory);
        SetClipboardData(CF_UNICODETEXT, memory);
    }
    CloseClipboard();
    SetStatus(LoadS(IDS_STATUS_COPY_DONE));
}

void MainWindow::OpenSettings() {
    if (!settingsWindow_.IsOpen()) {
        settingsWindow_.Create(instance_, hwnd_);
    }
    settingsWindow_.Show();
}

void MainWindow::RegisterAppHotkeys() {
    UnregisterHotKey(hwnd_, 1);
    UnregisterHotKey(hwnd_, 2);
    if (config_.ocrHotkey.virtualKey != 0) {
        RegisterHotKey(hwnd_, 1, config_.ocrHotkey.modifiers, config_.ocrHotkey.virtualKey);
    }
    if (config_.translateHotkey.virtualKey != 0) {
        RegisterHotKey(hwnd_, 2, config_.translateHotkey.modifiers, config_.translateHotkey.virtualKey);
    }
}

const ProviderConfig* MainWindow::DefaultProvider() const {
    for (const auto& provider : config_.providers) {
        if (provider.id == config_.defaultProviderId) {
            return &provider;
        }
    }
    return config_.providers.empty() ? nullptr : &config_.providers.front();
}

}  // namespace app
