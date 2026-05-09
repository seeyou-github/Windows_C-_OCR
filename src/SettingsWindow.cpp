#include "SettingsWindow.h"

#include "AppIds.h"
#include "AppTheme.h"
#include "UiText.h"
#include "res/resource.h"

#include <algorithm>
#include <atomic>
#include <commctrl.h>
#include <thread>

namespace app {

struct SettingsWindow::ProviderPage {
    darkui::Panel root;
    darkui::ListBox providerList;
    darkui::Button addProvider;
    darkui::Static titleText;
    darkui::Button providerToggle;
    darkui::Button providerDelete;
    darkui::Static nameLabel;
    darkui::Static keyLabel;
    darkui::Static baseLabel;
    darkui::Static pathLabel;
    darkui::Edit nameEdit;
    darkui::Edit keyEdit;
    darkui::Edit baseEdit;
    darkui::Edit pathEdit;
    darkui::CheckBox streamCheck;
    darkui::Static modelSearchLabel;
    darkui::Edit modelSearchEdit;
    darkui::Static modelListLabel;
    darkui::ListBox modelList;
    darkui::Button modelAdd;
    darkui::Button modelEdit;
    darkui::Button modelRemove;
    darkui::Button modelToggle;
    darkui::Button fetchModels;
    darkui::Button testModel;
};

struct SettingsWindow::DefaultsPage {
    darkui::Panel root;
    darkui::Static providerLabel;
    darkui::Static ocrLabel;
    darkui::Static translateLabel;
    darkui::Static ocrTimeoutLabel;
    darkui::Static ocrPromptLabel;
    darkui::Static translatePromptLabel;
    darkui::ComboBox providerCombo;
    darkui::ComboBox ocrCombo;
    darkui::ComboBox translateCombo;
    darkui::Edit ocrTimeoutEdit;
    darkui::Edit ocrPromptEdit;
    darkui::Edit translatePromptEdit;
    std::vector<DefaultModelChoice> modelChoices;
};

struct SettingsWindow::DisplayPage {
    darkui::Panel root;
    darkui::Static themeLabel;
    darkui::Static fontLabel;
    darkui::Static ocrResultFilterLabel;
    darkui::Static translateResultFilterLabel;
    darkui::ComboBox themeCombo;
    darkui::ComboBox fontCombo;
    darkui::CheckBox startTrayCheck;
    darkui::CheckBox copyAfterHotkeyOcrCheck;
    darkui::Edit ocrResultFilterEdit;
    darkui::Edit translateResultFilterEdit;
};

struct SettingsWindow::HotkeyPage {
    darkui::Panel root;
    darkui::Static help;
    darkui::Static ocrLabel;
    darkui::Static ocrValue;
    darkui::Static translateLabel;
    darkui::Static translateValue;
    darkui::Button ocrCapture;
    darkui::Button ocrClear;
    darkui::Button translateCapture;
    darkui::Button translateClear;
};

namespace {

constexpr std::uintptr_t kGroupHeaderFlag = static_cast<std::uintptr_t>(1) << ((sizeof(std::uintptr_t) * 8) - 1);
constexpr COLORREF kEnabledGreen = RGB(108, 200, 124);
constexpr COLORREF kCustomAmber = RGB(220, 176, 74);

std::wstring LoadS(UINT id) {
    return LoadStringResource(id);
}

std::vector<darkui::ComboItem> ToComboItems(const std::vector<std::wstring>& values) {
    std::vector<darkui::ComboItem> items;
    for (std::size_t i = 0; i < values.size(); ++i) {
        darkui::ComboItem item;
        item.text = values[i];
        item.userData = i;
        item.accent = false;
        items.push_back(std::move(item));
    }
    return items;
}

std::wstring ThemeStorageName(int index) {
    switch (index) {
    case 1: return L"moss";
    case 2: return L"mono";
    default: return L"graphite";
    }
}

int ThemeSelection(const std::wstring& themeName) {
    if (themeName == L"moss") return 1;
    if (themeName == L"mono") return 2;
    return 0;
}

std::wstring FilterModelText(const ProviderModel& model) {
    const std::wstring title = model.displayName.empty() ? model.id : model.displayName + L" (" + model.id + L")";
    const std::wstring customTag = model.custom ? L" " + LoadS(IDS_PROVIDER_CUSTOM_TAG) : L"";
    return title + customTag + (model.enabled ? L" [" + LoadS(IDS_PROVIDER_ENABLED) + L"]"
                                              : L" [" + LoadS(IDS_PROVIDER_DISABLED) + L"]");
}

std::wstring DefaultModelDisplayText(const ProviderConfig& provider, const ProviderModel& model) {
    const std::wstring modelText = model.displayName.empty() ? model.id : model.displayName + L" (" + model.id + L")";
    return provider.name + L" | " + modelText;
}

bool IsGroupHeaderItem(const darkui::ListBoxItem& item) {
    return (item.userData & kGroupHeaderFlag) != 0;
}

std::wstring GroupNameForModel(const ProviderModel& model) {
    const std::size_t slash = model.id.find(L'/');
    if (slash == std::wstring::npos || slash == 0) {
        return LoadS(IDS_PROVIDER_MODEL_GROUP_MISC);
    }
    return model.id.substr(0, slash);
}

std::wstring TrimCopy(const std::wstring& text) {
    std::size_t start = 0;
    while (start < text.size() && iswspace(text[start])) {
        ++start;
    }
    std::size_t end = text.size();
    while (end > start && iswspace(text[end - 1])) {
        --end;
    }
    return text.substr(start, end - start);
}

void ShowWindowWithoutWhiteFlash(HWND hwnd) {
    const LONG_PTR oldExStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    const bool addLayered = (oldExStyle & WS_EX_LAYERED) == 0;
    if (addLayered) {
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE, oldExStyle | WS_EX_LAYERED);
    }
    SetLayeredWindowAttributes(hwnd, 0, 0, LWA_ALPHA);
    ShowWindow(hwnd, SW_SHOW);
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

struct AddModelDialogSession {
    darkui::ThemeManager themeManager;
    darkui::Dialog dialog;
    darkui::Panel formPanel;
    darkui::Static idLabel;
    darkui::Edit idEdit;
    darkui::Static nameLabel;
    darkui::Edit nameEdit;
    darkui::CheckBox reasoningCheck;
};

enum AddModelDialogControlId {
    ID_DIALOG_MODEL_PANEL = 9101,
    ID_DIALOG_MODEL_ID_LABEL,
    ID_DIALOG_MODEL_ID_EDIT,
    ID_DIALOG_MODEL_NAME_LABEL,
    ID_DIALOG_MODEL_NAME_EDIT,
    ID_DIALOG_MODEL_REASONING_CHECK
};

enum TestResultDialogControlId {
    ID_DIALOG_TEST_PANEL = 9201,
    ID_DIALOG_TEST_PROMPT_LABEL,
    ID_DIALOG_TEST_PROMPT_EDIT,
    ID_DIALOG_TEST_SEND_BUTTON,
    ID_DIALOG_TEST_RESPONSE_LABEL,
    ID_DIALOG_TEST_RESPONSE_EDIT
};

constexpr UINT WM_APP_TEST_STREAM_CHUNK = WM_APP + 40;
constexpr UINT WM_APP_TEST_DONE = WM_APP + 41;
constexpr UINT WM_APP_CAPTURE_HOTKEY = WM_APP + 42;

struct TestChunkPayload {
    unsigned long long requestId = 0;
    std::wstring text;
};

struct TestDonePayload {
    unsigned long long requestId = 0;
    ServiceResult result;
};

struct HotkeyCapturePayload {
    UINT virtualKey = 0;
    UINT modifiers = 0;
};

HWND g_captureTargetHwnd = nullptr;

bool IsModifierKey(UINT vk) {
    return vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT ||
           vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL ||
           vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU ||
           vk == VK_LWIN || vk == VK_RWIN;
}

LRESULT CALLBACK SettingsKeyboardHookProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION && g_captureTargetHwnd != nullptr &&
        (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        const auto* info = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
        if (!info) {
            return CallNextHookEx(nullptr, code, wParam, lParam);
        }
        const UINT vk = static_cast<UINT>(info->vkCode);
        if (vk == VK_ESCAPE) {
            auto* payload = new HotkeyCapturePayload();
            payload->virtualKey = 0;
            payload->modifiers = 0;
            if (!PostMessageW(g_captureTargetHwnd, WM_APP_CAPTURE_HOTKEY, 0, reinterpret_cast<LPARAM>(payload))) {
                delete payload;
            }
            return 1;
        }
        if (!IsModifierKey(vk)) {
            UINT modifiers = 0;
            if (GetAsyncKeyState(VK_CONTROL) & 0x8000) modifiers |= MOD_CONTROL;
            if (GetAsyncKeyState(VK_SHIFT) & 0x8000) modifiers |= MOD_SHIFT;
            if (GetAsyncKeyState(VK_MENU) & 0x8000) modifiers |= MOD_ALT;
            if ((GetAsyncKeyState(VK_LWIN) & 0x8000) || (GetAsyncKeyState(VK_RWIN) & 0x8000)) modifiers |= MOD_WIN;
            auto* payload = new HotkeyCapturePayload();
            payload->virtualKey = vk;
            payload->modifiers = modifiers;
            if (!PostMessageW(g_captureTargetHwnd, WM_APP_CAPTURE_HOTKEY, 0, reinterpret_cast<LPARAM>(payload))) {
                delete payload;
            }
            return 1;
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

}  // namespace

SettingsWindow::SettingsWindow(AppConfig& config, const ConfigStore& store, const OcrService& service, std::function<void()> onConfigSaved)
    : config_(config),
      store_(store),
      service_(service),
      onConfigSaved_(std::move(onConfigSaved)) {
}

SettingsWindow::~SettingsWindow() {
    delete providerPage_;
    delete defaultsPage_;
    delete displayPage_;
    delete hotkeyPage_;
    if (appIconLarge_) {
        DestroyIcon(appIconLarge_);
        appIconLarge_ = nullptr;
    }
    if (appIconSmall_) {
        DestroyIcon(appIconSmall_);
        appIconSmall_ = nullptr;
    }
}

bool SettingsWindow::Create(HINSTANCE instance, HWND owner) {
    instance_ = instance;
    owner_ = owner;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance_;
    wc.lpszClassName = L"Win32OcrSettingsWindow";
    appIconLarge_ = LoadAppIcon(instance_, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON));
    appIconSmall_ = LoadAppIcon(instance_, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON));
    wc.hIcon = appIconLarge_;
    wc.hIconSm = appIconSmall_;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    RegisterClassExW(&wc);

    RECT workArea{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    const int width = std::max(960, static_cast<int>((workArea.right - workArea.left) * 0.8));
    const int height = std::max(700, static_cast<int>((workArea.bottom - workArea.top) * 0.8));
    const int left = workArea.left + ((workArea.right - workArea.left) - width) / 2;
    const int top = workArea.top + ((workArea.bottom - workArea.top) - height) / 2;

    hwnd_ = CreateWindowExW(WS_EX_APPWINDOW,
                            wc.lpszClassName,
                            LoadS(IDS_SETTINGS_TITLE).c_str(),
                            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                            left,
                            top,
                            width,
                            height,
                            owner_,
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

void SettingsWindow::Show() {
    if (hwnd_) {
        PositionWindow();
        if (!IsWindowVisible(hwnd_)) {
            ShowWindowWithoutWhiteFlash(hwnd_);
        } else {
            ShowWindow(hwnd_, SW_SHOW);
            UpdateWindow(hwnd_);
        }
        SetForegroundWindow(hwnd_);
    }
}

void SettingsWindow::Close() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

bool SettingsWindow::IsOpen() const {
    return hwnd_ != nullptr && IsWindow(hwnd_);
}

LRESULT CALLBACK SettingsWindow::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    SettingsWindow* self = reinterpret_cast<SettingsWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<SettingsWindow*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    return self ? self->HandleMessage(hwnd, message, wParam, lParam) : DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT SettingsWindow::HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        return OnCreate(hwnd) ? 0 : -1;
    case WM_SIZE:
        OnSize();
        return 0;
    case WM_COMMAND:        OnCommand(wParam, lParam);
        return 0;
    case WM_NOTIFY:
        OnNotify(lParam);
        return 0;
    case WM_APP_TEST_STREAM_CHUNK: {
        std::unique_ptr<TestChunkPayload> payload(reinterpret_cast<TestChunkPayload*>(lParam));
        if (!testDialog_ || !payload || payload->requestId != testDialog_->requestId) {
            return 0;
        }        HWND edit = testDialog_->responseEdit.edit_hwnd();
        SendMessageW(edit, EM_SETSEL, static_cast<WPARAM>(-1), static_cast<LPARAM>(-1));
        SendMessageW(edit, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(payload->text.c_str()));
        RedrawWindow(testDialog_->responseEdit.hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
        return 0;
    }
    case WM_APP_TEST_DONE: {
        std::unique_ptr<TestDonePayload> payload(reinterpret_cast<TestDonePayload*>(lParam));
        if (!testDialog_ || !payload || payload->requestId != testDialog_->requestId) {
            return 0;
        }        testDialog_->busy = false;
        EnableWindow(testDialog_->sendButton.hwnd(), TRUE);
        if (!payload->result.success) {
            if (!payload->result.responseText.empty()) {
                testDialog_->responseEdit.SetText(payload->result.responseText);
            } else {
                testDialog_->responseEdit.SetText(payload->result.error);
            }
            SetStatus(FormatStatus(LoadS(IDS_STATUS_TEST_MODEL_FAILED), payload->result.error));
        } else {
            if (!payload->result.responseText.empty() && testDialog_->responseEdit.GetText().empty()) {
                testDialog_->responseEdit.SetText(payload->result.responseText);
            }
            SetStatus(LoadS(IDS_STATUS_TEST_MODEL_DONE));
        }
        return 0;
    }
    case WM_APP_CAPTURE_HOTKEY: {
        std::unique_ptr<HotkeyCapturePayload> payload(reinterpret_cast<HotkeyCapturePayload*>(lParam));
        if (!payload || captureTarget_ == CaptureTarget::None) {
            return 0;
        }
        if (payload->virtualKey == 0) {
            if (captureTarget_ == CaptureTarget::Translate) {
                config_.translateHotkey = {};
            }
            StopHotkeyCapture();
            RefreshHotkeysPage();
            return 0;
        }
        if (captureTarget_ == CaptureTarget::Translate) {
            config_.translateHotkey = {payload->modifiers, payload->virtualKey};
        } else {
            config_.ocrHotkey = {payload->modifiers, payload->virtualKey};
        }
        StopHotkeyCapture();
        RefreshHotkeysPage();
        return 0;
    }
    case WM_KEYDOWN:
        OnKeyDown(static_cast<UINT>(wParam));
        return 0;
    case WM_SYSKEYDOWN:
        OnKeyDown(static_cast<UINT>(wParam));
        return 0;
    case WM_ERASEBKGND:
        if (host_.HandleEraseBackground(reinterpret_cast<HDC>(wParam))) {
            return 1;
        }
        break;
    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, host_.theme().text);
        SetBkColor(dc, host_.theme().background);
        return reinterpret_cast<INT_PTR>(host_.background_brush());
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, host_.theme().editText);
        SetBkColor(dc, host_.theme().editBackground);
        return reinterpret_cast<INT_PTR>(host_.background_brush());
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        OnDestroy();
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool SettingsWindow::OnCreate(HWND hwnd) {
    hwnd_ = hwnd;

    darkui::ThemedWindowHost::Options hostOptions;
    hostOptions.theme = MakeAppTheme(config_.themeName, config_.fontSize);
    hostOptions.titleBarStyle = darkui::TitleBarStyle::Black;
    if (!host_.Attach(hwnd_, hostOptions)) {
        return false;
    }

    providerPage_ = new ProviderPage();
    defaultsPage_ = new DefaultsPage();
    displayPage_ = new DisplayPage();
    hotkeyPage_ = new HotkeyPage();

    darkui::Tab::Options tabOptions;
    tabOptions.items = {
        {LoadS(IDS_TAB_PROVIDER), 0},
        {L"模型设置", 1},
        {LoadS(IDS_TAB_DISPLAY), 2},
        {LoadS(IDS_TAB_HOTKEY), 3}
    };
    tabOptions.selection = 0;
    tabOptions.variant = darkui::TabVariant::Accent;
    tabOptions.contentPaddingLeft = 18;
    if (!tab_.Create(hwnd_, ID_SETTINGS_TAB, host_.theme(), tabOptions)) {
        return false;
    }
    tab_.SetVertical(true);

    darkui::Button::Options primaryButton;
    primaryButton.variant = darkui::ButtonVariant::Primary;
    primaryButton.text = LoadS(IDS_SAVE);
    if (!saveButton_.Create(hwnd_, ID_SETTINGS_SAVE, host_.theme(), primaryButton)) {
        return false;
    }

    darkui::Button::Options secondaryButton;
    secondaryButton.variant = darkui::ButtonVariant::Secondary;
    secondaryButton.text = LoadS(IDS_CLOSE);
    if (!closeButton_.Create(hwnd_, ID_SETTINGS_CLOSE, host_.theme(), secondaryButton)) {
        return false;
    }

    darkui::Panel::Options pageOptions;
    providerPage_->root.Create(tab_.hwnd(), 8001, host_.theme(), pageOptions);
    defaultsPage_->root.Create(tab_.hwnd(), 8002, host_.theme(), pageOptions);
    displayPage_->root.Create(tab_.hwnd(), 8003, host_.theme(), pageOptions);
    hotkeyPage_->root.Create(tab_.hwnd(), 8004, host_.theme(), pageOptions);
    tab_.AttachPage(0, providerPage_->root.hwnd());
    tab_.AttachPage(1, defaultsPage_->root.hwnd());
    tab_.AttachPage(2, displayPage_->root.hwnd());
    tab_.AttachPage(3, hotkeyPage_->root.hwnd());

    darkui::ListBox::Options providerListOptions;
    providerListOptions.variant = darkui::FieldVariant::Panel;
    providerPage_->providerList.Create(providerPage_->root.hwnd(), ID_PROVIDER_TABLE, host_.theme(), providerListOptions);

    darkui::Button::Options addProviderOptions;
    addProviderOptions.variant = darkui::ButtonVariant::Secondary;
    addProviderOptions.text = LoadS(IDS_PROVIDER_ADD_CUSTOM);
    providerPage_->addProvider.Create(providerPage_->root.hwnd(), ID_PROVIDER_ADD, host_.theme(), addProviderOptions);

    darkui::Static::Options titleOptions;
    titleOptions.variant = darkui::StaticVariant::Title;
    providerPage_->titleText.Create(providerPage_->root.hwnd(), 8108, host_.theme(), titleOptions);

    darkui::Button::Options providerToggleOptions;
    providerToggleOptions.variant = darkui::ButtonVariant::Secondary;
    providerToggleOptions.text = LoadS(IDS_PROVIDER_ENABLED);
    providerPage_->providerToggle.Create(providerPage_->root.hwnd(), ID_PROVIDER_ENABLED_TOGGLE, host_.theme(), providerToggleOptions);

    darkui::Button::Options providerDeleteOptions;
    providerDeleteOptions.variant = darkui::ButtonVariant::Danger;
    providerDeleteOptions.text = LoadS(IDS_PROVIDER_DELETE_ACTION);
    providerPage_->providerDelete.Create(providerPage_->root.hwnd(), ID_PROVIDER_DELETE, host_.theme(), providerDeleteOptions);

    darkui::Static::Options labelOptions;
    labelOptions.variant = darkui::StaticVariant::PanelBody;
    labelOptions.text = LoadS(IDS_PROVIDER_NAME);
    providerPage_->nameLabel.Create(providerPage_->root.hwnd(), 8110, host_.theme(), labelOptions);
    labelOptions.text = LoadS(IDS_PROVIDER_API_KEY);
    providerPage_->keyLabel.Create(providerPage_->root.hwnd(), 8111, host_.theme(), labelOptions);
    labelOptions.text = LoadS(IDS_PROVIDER_BASE_URL);
    providerPage_->baseLabel.Create(providerPage_->root.hwnd(), 8112, host_.theme(), labelOptions);
    labelOptions.text = LoadS(IDS_PROVIDER_API_PATH);
    providerPage_->pathLabel.Create(providerPage_->root.hwnd(), 8113, host_.theme(), labelOptions);
    labelOptions.text = LoadS(IDS_PROVIDER_MODEL_SEARCH);
    providerPage_->modelSearchLabel.Create(providerPage_->root.hwnd(), 8115, host_.theme(), labelOptions);
    labelOptions.text = LoadS(IDS_PROVIDER_MODEL_LIST);
    providerPage_->modelListLabel.Create(providerPage_->root.hwnd(), 8116, host_.theme(), labelOptions);

    darkui::Edit::Options fieldOptions;
    fieldOptions.variant = darkui::FieldVariant::Default;
    providerPage_->nameEdit.Create(providerPage_->root.hwnd(), ID_PROVIDER_NAME_EDIT, host_.theme(), fieldOptions);
    providerPage_->keyEdit.Create(providerPage_->root.hwnd(), ID_PROVIDER_KEY_EDIT, host_.theme(), fieldOptions);
    providerPage_->baseEdit.Create(providerPage_->root.hwnd(), ID_PROVIDER_BASE_EDIT, host_.theme(), fieldOptions);
    providerPage_->pathEdit.Create(providerPage_->root.hwnd(), ID_PROVIDER_PATH_EDIT, host_.theme(), fieldOptions);
    providerPage_->modelSearchEdit.Create(providerPage_->root.hwnd(), ID_PROVIDER_MODEL_SEARCH_EDIT, host_.theme(), fieldOptions);

    darkui::CheckBox::Options providerCheckOptions;
    providerCheckOptions.variant = darkui::SelectionVariant::Accent;
    providerCheckOptions.text = LoadS(IDS_DEFAULT_STREAM_RESPONSE);
    providerPage_->streamCheck.Create(providerPage_->root.hwnd(), ID_PROVIDER_STREAM_CHECKBOX, host_.theme(), providerCheckOptions);

    darkui::ListBox::Options listOptions;
    listOptions.variant = darkui::FieldVariant::Default;
    providerPage_->modelList.Create(providerPage_->root.hwnd(), ID_PROVIDER_MODEL_LIST, host_.theme(), listOptions);

    darkui::Button::Options smallButton;
    smallButton.variant = darkui::ButtonVariant::Ghost;
    smallButton.cornerRadius = 16;
    smallButton.text = LoadS(IDS_PROVIDER_MODEL_ADD_CUSTOM);
    providerPage_->modelAdd.Create(providerPage_->root.hwnd(), ID_PROVIDER_MODEL_ADD, host_.theme(), smallButton);
    smallButton.text = LoadS(IDS_PROVIDER_MODEL_EDIT);
    providerPage_->modelEdit.Create(providerPage_->root.hwnd(), ID_PROVIDER_MODEL_EDIT, host_.theme(), smallButton);
    smallButton.text = LoadS(IDS_PROVIDER_MODEL_REMOVE);
    providerPage_->modelRemove.Create(providerPage_->root.hwnd(), ID_PROVIDER_MODEL_REMOVE, host_.theme(), smallButton);
    smallButton.text = LoadS(IDS_PROVIDER_MODEL_TOGGLE);
    providerPage_->modelToggle.Create(providerPage_->root.hwnd(), ID_PROVIDER_MODEL_TOGGLE, host_.theme(), smallButton);
    smallButton.text = LoadS(IDS_PROVIDER_FETCH_MODELS);
    providerPage_->fetchModels.Create(providerPage_->root.hwnd(), ID_PROVIDER_FETCH_MODELS, host_.theme(), smallButton);
    smallButton.text = LoadS(IDS_PROVIDER_TEST_MODEL);
    providerPage_->testModel.Create(providerPage_->root.hwnd(), ID_PROVIDER_TEST_MODEL, host_.theme(), smallButton);

    labelOptions.text = LoadS(IDS_DEFAULT_PROVIDER);
    defaultsPage_->providerLabel.Create(defaultsPage_->root.hwnd(), 8200, host_.theme(), labelOptions);
    labelOptions.text = LoadS(IDS_DEFAULT_OCR_MODEL);
    defaultsPage_->ocrLabel.Create(defaultsPage_->root.hwnd(), 8201, host_.theme(), labelOptions);
    labelOptions.text = LoadS(IDS_DEFAULT_TRANSLATE_MODEL);
    defaultsPage_->translateLabel.Create(defaultsPage_->root.hwnd(), 8202, host_.theme(), labelOptions);
    labelOptions.text = LoadS(IDS_DEFAULT_OCR_TIMEOUT);
    defaultsPage_->ocrTimeoutLabel.Create(defaultsPage_->root.hwnd(), 8207, host_.theme(), labelOptions);
    labelOptions.text = L"OCR提示词设置";
    defaultsPage_->ocrPromptLabel.Create(defaultsPage_->root.hwnd(), 8203, host_.theme(), labelOptions);
    labelOptions.text = L"文本翻译提示词设置";
    defaultsPage_->translatePromptLabel.Create(defaultsPage_->root.hwnd(), 8204, host_.theme(), labelOptions);

    darkui::ComboBox::Options comboOptions;
    comboOptions.variant = darkui::FieldVariant::Panel;
    defaultsPage_->providerCombo.Create(defaultsPage_->root.hwnd(), ID_DEFAULT_PROVIDER_COMBO, host_.theme(), comboOptions);
    defaultsPage_->ocrCombo.Create(defaultsPage_->root.hwnd(), ID_DEFAULT_OCR_MODEL_COMBO, host_.theme(), comboOptions);
    defaultsPage_->translateCombo.Create(defaultsPage_->root.hwnd(), ID_DEFAULT_TRANSLATE_MODEL_COMBO, host_.theme(), comboOptions);
    darkui::Edit::Options timeoutEditOptions;
    timeoutEditOptions.variant = darkui::FieldVariant::Panel;
    timeoutEditOptions.style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER;
    defaultsPage_->ocrTimeoutEdit.Create(defaultsPage_->root.hwnd(), ID_DEFAULT_OCR_TIMEOUT_EDIT, host_.theme(), timeoutEditOptions);
    darkui::Edit::Options promptEditOptions;
    promptEditOptions.variant = darkui::FieldVariant::Panel;
    promptEditOptions.style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL;
    defaultsPage_->ocrPromptEdit.Create(defaultsPage_->root.hwnd(), 8205, host_.theme(), promptEditOptions);
    defaultsPage_->translatePromptEdit.Create(defaultsPage_->root.hwnd(), 8206, host_.theme(), promptEditOptions);

    labelOptions.text = LoadS(IDS_DISPLAY_THEME);
    displayPage_->themeLabel.Create(displayPage_->root.hwnd(), 8300, host_.theme(), labelOptions);
    labelOptions.text = LoadS(IDS_DISPLAY_FONT_SIZE);
    displayPage_->fontLabel.Create(displayPage_->root.hwnd(), 8301, host_.theme(), labelOptions);
    labelOptions.text = L"OCR结果过滤";
    displayPage_->ocrResultFilterLabel.Create(displayPage_->root.hwnd(), 8302, host_.theme(), labelOptions);
    labelOptions.text = L"翻译结果过滤";
    displayPage_->translateResultFilterLabel.Create(displayPage_->root.hwnd(), 8303, host_.theme(), labelOptions);
    displayPage_->themeCombo.Create(displayPage_->root.hwnd(), ID_DISPLAY_THEME_COMBO, host_.theme(), comboOptions);
    displayPage_->fontCombo.Create(displayPage_->root.hwnd(), ID_DISPLAY_FONT_COMBO, host_.theme(), comboOptions);
    darkui::CheckBox::Options startTrayOptions;
    startTrayOptions.text = LoadS(IDS_DISPLAY_START_IN_TRAY);
    startTrayOptions.variant = darkui::SelectionVariant::Accent;
    displayPage_->startTrayCheck.Create(displayPage_->root.hwnd(), ID_DISPLAY_START_TRAY_CHECKBOX, host_.theme(), startTrayOptions);
    darkui::CheckBox::Options copyAfterOptions = startTrayOptions;
    copyAfterOptions.text = L"识别后复制到剪切板";
    displayPage_->copyAfterHotkeyOcrCheck.Create(displayPage_->root.hwnd(),
                                                 ID_DISPLAY_COPY_AFTER_HOTKEY_OCR_CHECKBOX,
                                                 host_.theme(),
                                                 copyAfterOptions);
    darkui::Edit::Options filterEditOptions;
    filterEditOptions.variant = darkui::FieldVariant::Panel;
    filterEditOptions.cueBanner = L"支持正则，多个规则用 | 分隔";
    displayPage_->ocrResultFilterEdit.Create(displayPage_->root.hwnd(),
                                             ID_DISPLAY_OCR_RESULT_FILTER_EDIT,
                                             host_.theme(),
                                             filterEditOptions);
    displayPage_->translateResultFilterEdit.Create(displayPage_->root.hwnd(),
                                                   ID_DISPLAY_TRANSLATE_RESULT_FILTER_EDIT,
                                                   host_.theme(),
                                                   filterEditOptions);

    darkui::Static::Options helpOptions;
    helpOptions.text = LoadS(IDS_HOTKEY_CAPTURE);
    hotkeyPage_->help.Create(hotkeyPage_->root.hwnd(), 8400, host_.theme(), helpOptions);
    labelOptions.text = LoadS(IDS_HOTKEY_OCR);
    hotkeyPage_->ocrLabel.Create(hotkeyPage_->root.hwnd(), 8401, host_.theme(), labelOptions);
    labelOptions.text = LoadS(IDS_HOTKEY_TRANSLATE);
    hotkeyPage_->translateLabel.Create(hotkeyPage_->root.hwnd(), 8402, host_.theme(), labelOptions);

    darkui::Static::Options valueOptions;
    valueOptions.variant = darkui::StaticVariant::PanelTitle;
    hotkeyPage_->ocrValue.Create(hotkeyPage_->root.hwnd(), 8403, host_.theme(), valueOptions);
    hotkeyPage_->translateValue.Create(hotkeyPage_->root.hwnd(), 8404, host_.theme(), valueOptions);

    smallButton.text = LoadS(IDS_HOTKEY_CAPTURE);
    hotkeyPage_->ocrCapture.Create(hotkeyPage_->root.hwnd(), ID_HOTKEY_CAPTURE_OCR, host_.theme(), smallButton);
    smallButton.text = LoadS(IDS_HOTKEY_CLEAR);
    hotkeyPage_->ocrClear.Create(hotkeyPage_->root.hwnd(), ID_HOTKEY_CLEAR_OCR, host_.theme(), smallButton);
    smallButton.text = LoadS(IDS_HOTKEY_CAPTURE);
    hotkeyPage_->translateCapture.Create(hotkeyPage_->root.hwnd(), ID_HOTKEY_CAPTURE_TRANSLATE, host_.theme(), smallButton);
    smallButton.text = LoadS(IDS_HOTKEY_CLEAR);
    hotkeyPage_->translateClear.Create(hotkeyPage_->root.hwnd(), ID_HOTKEY_CLEAR_TRANSLATE, host_.theme(), smallButton);

    host_.theme_manager().Bind(
        tab_, saveButton_, closeButton_,
        providerPage_->root, providerPage_->providerList, providerPage_->addProvider, providerPage_->titleText, providerPage_->providerToggle, providerPage_->providerDelete,
        providerPage_->nameLabel, providerPage_->keyLabel, providerPage_->baseLabel, providerPage_->pathLabel,
        providerPage_->nameEdit, providerPage_->keyEdit, providerPage_->baseEdit, providerPage_->pathEdit, providerPage_->streamCheck,
        providerPage_->modelSearchLabel, providerPage_->modelSearchEdit, providerPage_->modelListLabel, providerPage_->modelList,
        providerPage_->modelAdd, providerPage_->modelEdit, providerPage_->modelRemove, providerPage_->modelToggle, providerPage_->fetchModels, providerPage_->testModel,
        defaultsPage_->root, defaultsPage_->providerLabel, defaultsPage_->ocrLabel, defaultsPage_->translateLabel,
        defaultsPage_->ocrTimeoutLabel, defaultsPage_->ocrPromptLabel, defaultsPage_->translatePromptLabel,
        defaultsPage_->providerCombo, defaultsPage_->ocrCombo, defaultsPage_->translateCombo,
        defaultsPage_->ocrTimeoutEdit, defaultsPage_->ocrPromptEdit, defaultsPage_->translatePromptEdit,
        displayPage_->root, displayPage_->themeLabel, displayPage_->fontLabel, displayPage_->ocrResultFilterLabel,
        displayPage_->translateResultFilterLabel, displayPage_->themeCombo, displayPage_->fontCombo,
        displayPage_->startTrayCheck, displayPage_->copyAfterHotkeyOcrCheck,
        displayPage_->ocrResultFilterEdit, displayPage_->translateResultFilterEdit,
        hotkeyPage_->root, hotkeyPage_->help,
        hotkeyPage_->ocrLabel, hotkeyPage_->ocrValue, hotkeyPage_->translateLabel, hotkeyPage_->translateValue,
        hotkeyPage_->ocrCapture, hotkeyPage_->ocrClear, hotkeyPage_->translateCapture, hotkeyPage_->translateClear);
    host_.theme_manager().Apply();
    RefreshAll();
    Layout();
    return true;
}

void SettingsWindow::OnDestroy() {
    StopHotkeyCapture();
    hwnd_ = nullptr;
}

void SettingsWindow::OnSize() {
    Layout();
}

void SettingsWindow::OnCommand(WPARAM wParam, LPARAM) {
    const int command = LOWORD(wParam);
    switch (command) {
    case ID_PROVIDER_ADD:
        CommitCurrentProvider();
        AddCustomProvider();
        return;
    case ID_PROVIDER_ENABLED_TOGGLE:
        ToggleCurrentProviderEnabled();
        return;
    case ID_PROVIDER_DELETE:
        DeleteCurrentProvider();
        return;
    case ID_PROVIDER_TABLE:
        if (HIWORD(wParam) == LBN_SELCHANGE) {
            const int selected = providerPage_->providerList.GetSelection();
            if (selected >= 0 && selected < static_cast<int>(providerPage_->providerList.GetCount())) {
                const int providerIndex = static_cast<int>(providerPage_->providerList.GetItem(selected).userData);
                if (providerIndex >= 0 && providerIndex < static_cast<int>(config_.providers.size()) && providerIndex != providerSelection_) {
                    CommitCurrentProvider();
                    providerSelection_ = providerIndex;
                    RefreshProviderDetails();
                    RefreshModelList();
                    RefreshModelActions();
                }
            }
        }
        return;
    case ID_PROVIDER_MODEL_LIST:
        if (HIWORD(wParam) == LBN_SELCHANGE) {
            const std::wstring groupName = SelectedModelGroup();
            if (!groupName.empty()) {
                auto it = std::find(collapsedModelGroups_.begin(), collapsedModelGroups_.end(), groupName);
                if (it == collapsedModelGroups_.end()) {
                    collapsedModelGroups_.push_back(groupName);
                } else {
                    collapsedModelGroups_.erase(it);
                }
                RefreshModelList();
            }
            RefreshModelActions();
        }
        return;
    case ID_PROVIDER_MODEL_ADD: {
        AddCustomModel();
        return;
    }
    case ID_PROVIDER_MODEL_EDIT: {
        EditSelectedModel();
        return;
    }
    case ID_PROVIDER_MODEL_REMOVE: {
        auto* provider = CurrentProvider();
        if (!provider) return;
        const int modelIndex = SelectedModelIndex();
        if (modelIndex >= 0 && modelIndex < static_cast<int>(provider->models.size())) {
            const std::wstring removedId = provider->models[modelIndex].id;
            provider->models.erase(provider->models.begin() + modelIndex);
            if (config_.defaultOcrModel == removedId) {
                config_.defaultOcrModel.clear();
            }
            if (config_.defaultTranslateModel == removedId) {
                config_.defaultTranslateModel.clear();
            }
            RefreshModelList();
            RefreshDefaultsPage();
            RefreshModelActions();
        } else {
            const std::wstring groupName = SelectedModelGroup();
            if (!groupName.empty()) {
                provider->models.erase(std::remove_if(provider->models.begin(),
                                                      provider->models.end(),
                                                      [&](const ProviderModel& model) {
                                                          return GroupNameForModel(model) == groupName;
                                                      }),
                                       provider->models.end());
                if (config_.defaultProviderId == provider->id) {
                    RefreshDefaultsPage();
                }
                RefreshModelList();
                RefreshModelActions();
            }
        }
        return;
    }
    case ID_PROVIDER_MODEL_TOGGLE: {
        auto* provider = CurrentProvider();
        if (!provider) return;
        const int modelIndex = SelectedModelIndex();
        if (modelIndex >= 0 && modelIndex < static_cast<int>(provider->models.size())) {
            provider->models[modelIndex].enabled = !provider->models[modelIndex].enabled;
            if (!provider->models[modelIndex].enabled) {
                if (config_.defaultOcrModel == provider->models[modelIndex].id) {
                    config_.defaultOcrModel.clear();
                }
                if (config_.defaultTranslateModel == provider->models[modelIndex].id) {
                    config_.defaultTranslateModel.clear();
                }
            }
            RefreshModelList();
            RefreshDefaultsPage();
            RefreshModelActions();
        }
        return;
    }
    case ID_PROVIDER_FETCH_MODELS: {
        auto* provider = CurrentProvider();
        if (!provider || provider->apiKey.empty()) {
            MessageBoxW(hwnd_, LoadS(IDS_MSG_INPUT_API_KEY).c_str(), LoadS(IDS_APP_TITLE).c_str(), MB_OK | MB_ICONINFORMATION);
            return;
        }
        CommitCurrentProvider();
        const ServiceResult result = service_.FetchModels(*provider);
        if (!result.success) {
            SetStatus(FormatStatus(LoadS(IDS_STATUS_FETCH_MODELS_FAILED), result.error));
            return;
        }
        provider->models.erase(std::remove_if(provider->models.begin(),
                                              provider->models.end(),
                                              [](const ProviderModel& model) { return !model.custom; }),
                               provider->models.end());
        for (const auto& model : result.items) {
            const auto exists = std::any_of(provider->models.begin(), provider->models.end(), [&](const ProviderModel& item) {
                return item.id == model;
            });
            if (exists) {
                continue;
            }
            ProviderModel entry;
            entry.id = model;
            entry.enabled = false;
            entry.custom = false;
            provider->models.push_back(std::move(entry));
        }
        if (provider->id == config_.defaultProviderId) {
            config_.defaultOcrModel.clear();
            config_.defaultTranslateModel.clear();
        }
        RefreshModelList();
        RefreshDefaultsPage();
        RefreshModelActions();
        SetStatus(LoadS(IDS_STATUS_FETCH_MODELS_DONE));
        return;
    }
    case ID_PROVIDER_TEST_MODEL: {
        auto* provider = CurrentProvider();
        if (!provider || provider->apiKey.empty()) {
            MessageBoxW(hwnd_, LoadS(IDS_MSG_INPUT_API_KEY).c_str(), LoadS(IDS_APP_TITLE).c_str(), MB_OK | MB_ICONINFORMATION);
            return;
        }
        const int modelIndex = SelectedModelIndex();
        if (modelIndex < 0 || modelIndex >= static_cast<int>(provider->models.size())) {
            MessageBoxW(hwnd_, LoadS(IDS_MSG_INPUT_MODEL_ID).c_str(), LoadS(IDS_APP_TITLE).c_str(), MB_OK | MB_ICONINFORMATION);
            return;
        }
        OpenModelTestDialog();
        return;
    }
    case ID_SETTINGS_SAVE:        SaveConfig();
        return;
    case ID_SETTINGS_CLOSE:
        DestroyWindow(hwnd_);
        return;
    case ID_HOTKEY_CAPTURE_OCR:
        StartHotkeyCapture(CaptureTarget::Ocr);
        return;
    case ID_HOTKEY_CAPTURE_TRANSLATE:
        StartHotkeyCapture(CaptureTarget::Translate);
        return;
    case ID_HOTKEY_CLEAR_OCR:
        config_.ocrHotkey = {};
        RefreshHotkeysPage();
        return;
    case ID_HOTKEY_CLEAR_TRANSLATE:
        config_.translateHotkey = {};
        RefreshHotkeysPage();
        return;
    case ID_DEFAULT_PROVIDER_COMBO:
        if (HIWORD(wParam) == CBN_SELCHANGE) {
            const int selection = defaultsPage_->providerCombo.GetSelection();
            if (selection >= 0 && selection < static_cast<int>(defaultsPage_->providerCombo.GetCount())) {
                const int providerIndex = static_cast<int>(defaultsPage_->providerCombo.GetItem(selection).userData);
                if (providerIndex >= 0 && providerIndex < static_cast<int>(config_.providers.size())) {
                    config_.defaultProviderId = config_.providers[providerIndex].id;
                }
                RefreshDefaultsPage();
            }
        }
        return;
    case ID_DEFAULT_OCR_MODEL_COMBO:
        if (HIWORD(wParam) == CBN_SELCHANGE) {
            const int selection = defaultsPage_->ocrCombo.GetSelection();
            if (selection >= 0 && selection < static_cast<int>(defaultsPage_->modelChoices.size())) {
                config_.defaultOcrProviderId = defaultsPage_->modelChoices[selection].providerId;
                config_.defaultOcrModel = defaultsPage_->modelChoices[selection].modelId;
                config_.defaultProviderId = config_.defaultOcrProviderId;
                RefreshDefaultsPage();
            }
        }
        return;
    case ID_DEFAULT_TRANSLATE_MODEL_COMBO:
        if (HIWORD(wParam) == CBN_SELCHANGE) {
            const int selection = defaultsPage_->translateCombo.GetSelection();
            if (selection >= 0 && selection < static_cast<int>(defaultsPage_->modelChoices.size())) {
                config_.defaultTranslateProviderId = defaultsPage_->modelChoices[selection].providerId;
                config_.defaultTranslateModel = defaultsPage_->modelChoices[selection].modelId;
            }
        }
        return;
    case ID_PROVIDER_STREAM_CHECKBOX:
        if (HIWORD(wParam) == BN_CLICKED) {
            if (auto* provider = CurrentProvider()) {
                provider->streamResponse = providerPage_->streamCheck.GetChecked();
            }
        }
        return;
    case ID_DIALOG_TEST_SEND_BUTTON:
        if (HIWORD(wParam) == BN_CLICKED) {
            StartModelTestRequest();
        }
        return;
    case ID_DISPLAY_THEME_COMBO:
        if (HIWORD(wParam) == CBN_SELCHANGE) {
            config_.themeName = ThemeStorageName(displayPage_->themeCombo.GetSelection());
            ApplyCurrentTheme();
        }
        return;
    case ID_DISPLAY_FONT_COMBO:
        if (HIWORD(wParam) == CBN_SELCHANGE) {
            config_.fontSize = _wtoi(displayPage_->fontCombo.GetText().c_str());
            ApplyCurrentTheme();
        }
        return;
    case ID_DISPLAY_START_TRAY_CHECKBOX:
        if (HIWORD(wParam) == BN_CLICKED) {
            config_.startInTray = displayPage_->startTrayCheck.GetChecked();
        }
        return;
    case ID_DISPLAY_COPY_AFTER_HOTKEY_OCR_CHECKBOX:
        if (HIWORD(wParam) == BN_CLICKED) {
            config_.copyAfterHotkeyOcr = displayPage_->copyAfterHotkeyOcrCheck.GetChecked();
        }
        return;
    case ID_PROVIDER_MODEL_SEARCH_EDIT:
        if (HIWORD(wParam) == EN_CHANGE) {
            RefreshModelList();
        }
        return;
    default:
        break;
    }
}

void SettingsWindow::OnNotify(LPARAM lParam) {
    auto* hdr = reinterpret_cast<NMHDR*>(lParam);
    if (!hdr) {
        return;
    }
    if (hdr->hwndFrom == tab_.hwnd() && hdr->code == TCN_SELCHANGE) {
        return;
    }
}

void SettingsWindow::StartHotkeyCapture(CaptureTarget target) {
    StopHotkeyCapture();
    captureTarget_ = target;
    g_captureTargetHwnd = hwnd_;
    keyboardHook_ = SetWindowsHookExW(WH_KEYBOARD_LL, SettingsKeyboardHookProc, GetModuleHandleW(nullptr), 0);
    if (!keyboardHook_) {
        captureTarget_ = CaptureTarget::None;
        g_captureTargetHwnd = nullptr;
        MessageBoxW(hwnd_, L"Failed to start hotkey capture.", LoadS(IDS_APP_TITLE).c_str(), MB_OK | MB_ICONERROR);
    }
    RefreshHotkeysPage();
    SetForegroundWindow(hwnd_);
    SetFocus(hwnd_);
}

void SettingsWindow::StopHotkeyCapture() {
    if (keyboardHook_) {
        UnhookWindowsHookEx(keyboardHook_);
        keyboardHook_ = nullptr;
    }
    g_captureTargetHwnd = nullptr;
    captureTarget_ = CaptureTarget::None;
}

void SettingsWindow::OnKeyDown(UINT virtualKey) {
    if (captureTarget_ == CaptureTarget::None) {
        return;
    }
    UINT modifiers = 0;
    if (GetKeyState(VK_CONTROL) & 0x8000) modifiers |= MOD_CONTROL;
    if (GetKeyState(VK_SHIFT) & 0x8000) modifiers |= MOD_SHIFT;
    if (GetKeyState(VK_MENU) & 0x8000) modifiers |= MOD_ALT;
    if (GetKeyState(VK_LWIN) & 0x8000 || GetKeyState(VK_RWIN) & 0x8000) modifiers |= MOD_WIN;
    if (captureTarget_ == CaptureTarget::Translate) {
        config_.translateHotkey = {modifiers, virtualKey};
    } else {
        config_.ocrHotkey = {modifiers, virtualKey};
    }
    StopHotkeyCapture();
    RefreshHotkeysPage();
}

void SettingsWindow::Layout() {
    if (!hwnd_) {
        return;
    }

    RECT client{};
    GetClientRect(hwnd_, &client);
    MoveWindow(tab_.hwnd(), 24, 24, client.right - 48, client.bottom - 112, TRUE);
    MoveWindow(saveButton_.hwnd(), client.right - 226, client.bottom - 64, 96, 36, TRUE);
    MoveWindow(closeButton_.hwnd(), client.right - 118, client.bottom - 64, 96, 36, TRUE);

    const RECT content = tab_.GetContentRect();
    const int pageLeft = content.left;
    const int pageWidth = (content.right - content.left);
    const int pageHeight = content.bottom - content.top;

    MoveWindow(providerPage_->root.hwnd(), pageLeft, content.top, pageWidth, pageHeight, TRUE);
    MoveWindow(defaultsPage_->root.hwnd(), pageLeft, content.top, pageWidth, pageHeight, TRUE);
    MoveWindow(displayPage_->root.hwnd(), pageLeft, content.top, pageWidth, pageHeight, TRUE);
    MoveWindow(hotkeyPage_->root.hwnd(), pageLeft, content.top, pageWidth, pageHeight, TRUE);

    const int leftColumn = 28;
    const int leftWidth = 252;
    MoveWindow(providerPage_->providerList.hwnd(), leftColumn, 32, leftWidth, pageHeight - 132, TRUE);
    MoveWindow(providerPage_->addProvider.hwnd(), leftColumn, pageHeight - 72, leftWidth, 38, TRUE);

    const int formLeft = 314;
    const int rightInset = 18;
    const int labelWidth = 100;
    const int fieldX = formLeft + 108;
    const int fieldWidth = pageWidth - fieldX - rightInset;
    MoveWindow(providerPage_->titleText.hwnd(), formLeft, 22, 240, 34, TRUE);
    MoveWindow(providerPage_->providerDelete.hwnd(), pageWidth - 120, 22, 92, 34, TRUE);
    MoveWindow(providerPage_->providerToggle.hwnd(), pageWidth - 224, 22, 96, 34, TRUE);

    int y = 72;
    auto placeField = [&](HWND label, HWND field) {
        MoveWindow(label, formLeft, y, labelWidth, 26, TRUE);
        MoveWindow(field, fieldX, y - 4, fieldWidth, 34, TRUE);
        y += 48;
    };
    placeField(providerPage_->nameLabel.hwnd(), providerPage_->nameEdit.hwnd());
    placeField(providerPage_->keyLabel.hwnd(), providerPage_->keyEdit.hwnd());
    placeField(providerPage_->baseLabel.hwnd(), providerPage_->baseEdit.hwnd());
    placeField(providerPage_->pathLabel.hwnd(), providerPage_->pathEdit.hwnd());
    MoveWindow(providerPage_->streamCheck.hwnd(), fieldX, y - 4, 180, 30, TRUE);
    y += 44;
    placeField(providerPage_->modelSearchLabel.hwnd(), providerPage_->modelSearchEdit.hwnd());
    MoveWindow(providerPage_->modelListLabel.hwnd(), formLeft, y, 160, 26, TRUE);
    const int actionWidth = 112;
    const int actionGap = 8;
    const int actionY = y - 6;
    const int fetchX = fieldX + fieldWidth - actionWidth;
    const int testX = fetchX - actionGap - actionWidth;
    const int toggleX = testX - actionGap - actionWidth;
    const int removeX = toggleX - actionGap - actionWidth;
    const int editX = removeX - actionGap - actionWidth;
    const int addX = editX - actionGap - 152;
    MoveWindow(providerPage_->modelAdd.hwnd(), addX, actionY, 152, 32, TRUE);
    MoveWindow(providerPage_->modelEdit.hwnd(), editX, actionY, actionWidth, 32, TRUE);
    MoveWindow(providerPage_->modelRemove.hwnd(), removeX, actionY, actionWidth, 32, TRUE);
    MoveWindow(providerPage_->modelToggle.hwnd(), toggleX, actionY, actionWidth, 32, TRUE);
    MoveWindow(providerPage_->testModel.hwnd(), testX, actionY, actionWidth, 32, TRUE);
    MoveWindow(providerPage_->fetchModels.hwnd(), fetchX, actionY, actionWidth, 32, TRUE);
    y += 38;
    MoveWindow(providerPage_->modelList.hwnd(), fieldX, y, fieldWidth, pageHeight - y - 34, TRUE);

    ShowWindow(defaultsPage_->providerLabel.hwnd(), SW_HIDE);
    ShowWindow(defaultsPage_->providerCombo.hwnd(), SW_HIDE);
    MoveWindow(defaultsPage_->ocrLabel.hwnd(), 28, 40, 160, 26, TRUE);
    MoveWindow(defaultsPage_->ocrCombo.hwnd(), 220, 34, 520, 34, TRUE);
    MoveWindow(defaultsPage_->translateLabel.hwnd(), 28, 104, 160, 26, TRUE);
    MoveWindow(defaultsPage_->translateCombo.hwnd(), 220, 98, 520, 34, TRUE);
    MoveWindow(defaultsPage_->ocrTimeoutLabel.hwnd(), 28, 168, 180, 26, TRUE);
    MoveWindow(defaultsPage_->ocrTimeoutEdit.hwnd(), 220, 162, 160, 34, TRUE);
    MoveWindow(defaultsPage_->ocrPromptLabel.hwnd(), 28, 230, 180, 26, TRUE);
    MoveWindow(defaultsPage_->ocrPromptEdit.hwnd(), 220, 224, 520, 110, TRUE);
    MoveWindow(defaultsPage_->translatePromptLabel.hwnd(), 28, 356, 180, 26, TRUE);
    MoveWindow(defaultsPage_->translatePromptEdit.hwnd(), 220, 350, 520, 110, TRUE);

    MoveWindow(displayPage_->themeLabel.hwnd(), 28, 40, 160, 26, TRUE);
    MoveWindow(displayPage_->themeCombo.hwnd(), 220, 34, 260, 34, TRUE);
    MoveWindow(displayPage_->fontLabel.hwnd(), 28, 104, 160, 26, TRUE);
    MoveWindow(displayPage_->fontCombo.hwnd(), 220, 98, 260, 34, TRUE);
    MoveWindow(displayPage_->startTrayCheck.hwnd(), 220, 154, 240, 30, TRUE);
    MoveWindow(displayPage_->copyAfterHotkeyOcrCheck.hwnd(), 220, 194, 320, 30, TRUE);
    MoveWindow(displayPage_->ocrResultFilterLabel.hwnd(), 28, 252, 160, 26, TRUE);
    MoveWindow(displayPage_->ocrResultFilterEdit.hwnd(), 220, 246, 520, 34, TRUE);
    MoveWindow(displayPage_->translateResultFilterLabel.hwnd(), 28, 316, 160, 26, TRUE);
    MoveWindow(displayPage_->translateResultFilterEdit.hwnd(), 220, 310, 520, 34, TRUE);

    MoveWindow(hotkeyPage_->help.hwnd(), 28, 24, 520, 30, TRUE);
    MoveWindow(hotkeyPage_->ocrLabel.hwnd(), 28, 90, 160, 26, TRUE);
    MoveWindow(hotkeyPage_->ocrValue.hwnd(), 220, 86, 240, 32, TRUE);
    MoveWindow(hotkeyPage_->ocrCapture.hwnd(), 480, 84, 120, 34, TRUE);
    MoveWindow(hotkeyPage_->ocrClear.hwnd(), 612, 84, 88, 34, TRUE);
    MoveWindow(hotkeyPage_->translateLabel.hwnd(), 28, 150, 160, 26, TRUE);
    MoveWindow(hotkeyPage_->translateValue.hwnd(), 220, 146, 240, 32, TRUE);
    MoveWindow(hotkeyPage_->translateCapture.hwnd(), 480, 144, 120, 34, TRUE);
    MoveWindow(hotkeyPage_->translateClear.hwnd(), 612, 144, 88, 34, TRUE);

    const int selectedTab = tab_.GetSelection();
    HWND visiblePage = nullptr;
    switch (selectedTab) {
    case 0: visiblePage = providerPage_->root.hwnd(); break;
    case 1: visiblePage = defaultsPage_->root.hwnd(); break;
    case 2: visiblePage = displayPage_->root.hwnd(); break;
    case 3: visiblePage = hotkeyPage_->root.hwnd(); break;
    default: break;
    }
    if (visiblePage) {
        InvalidateRect(visiblePage, nullptr, FALSE);
    }
}

void SettingsWindow::PositionWindow() {
    if (!hwnd_) {
        return;
    }

    RECT workArea{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    const int width = std::max(960, static_cast<int>((workArea.right - workArea.left) * 0.8));
    const int height = std::max(700, static_cast<int>((workArea.bottom - workArea.top) * 0.8));
    const int left = workArea.left + ((workArea.right - workArea.left) - width) / 2;
    const int top = workArea.top + ((workArea.bottom - workArea.top) - height) / 2;
    SetWindowPos(hwnd_, nullptr, left, top, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
}

void SettingsWindow::RefreshAll() {
    RefreshProviderTable();
    RefreshProviderDetails();
    RefreshModelList();
    RefreshDefaultsPage();
    RefreshDisplayPage();
    RefreshHotkeysPage();
    RefreshModelActions();
    SetStatus(LoadS(IDS_STATUS_READY));
}

std::wstring TrimGroupHeaderText(const std::wstring& text) {
    if (text.rfind(L"> ", 0) == 0 || text.rfind(L"+ ", 0) == 0 || text.rfind(L"- ", 0) == 0) {
        const std::size_t pos = text.rfind(L"  ");
        if (pos != std::wstring::npos && pos > 2) {
            return text.substr(2, pos - 2);
        }
        return text.substr(2);
    }
    return text;
}

void SettingsWindow::RefreshProviderTable() {
    std::vector<darkui::ListBoxItem> rows;
    int enabledCount = 0;
    std::vector<std::size_t> order(config_.providers.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        order[i] = i;
    }
    std::stable_sort(order.begin(), order.end(), [&](std::size_t lhs, std::size_t rhs) {
        const auto& a = config_.providers[lhs];
        const auto& b = config_.providers[rhs];
        if (a.enabled != b.enabled) {
            return a.enabled > b.enabled;
        }
        return a.name < b.name;
    });
    for (std::size_t index : order) {
        const auto& provider = config_.providers[index];
        darkui::ListBoxItem item;
        item.text = provider.name + L" | " + (provider.enabled ? LoadS(IDS_PROVIDER_ENABLED) : LoadS(IDS_PROVIDER_DISABLED));
        item.userData = static_cast<std::uintptr_t>(index);
        if (provider.enabled) {
            item.textColor = kEnabledGreen;
            ++enabledCount;
        }
        rows.push_back(std::move(item));
    }    providerPage_->providerList.SetItems(rows);
    for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
        if (static_cast<int>(rows[i].userData) == providerSelection_) {
            providerPage_->providerList.SetSelection(i, false);
            break;
        }
    }
}

void SettingsWindow::RefreshProviderDetails() {
    const ProviderConfig* provider = CurrentProvider();
    if (!provider) return;
    providerPage_->titleText.SetText(provider->name);
    providerPage_->providerToggle.SetText(provider->enabled ? LoadS(IDS_PROVIDER_DISABLE_ACTION) : LoadS(IDS_PROVIDER_ENABLE_ACTION));
    EnableWindow(providerPage_->providerDelete.hwnd(), !provider->builtIn);
    providerPage_->providerToggle.SetCornerRadius(18);
    providerPage_->providerDelete.SetCornerRadius(18);
    providerPage_->nameEdit.SetText(provider->name);
    providerPage_->keyEdit.SetText(provider->apiKey);
    providerPage_->baseEdit.SetText(provider->baseUrl);
    providerPage_->pathEdit.SetText(provider->apiPath);
    providerPage_->streamCheck.SetChecked(provider->streamResponse);
}

void SettingsWindow::RefreshModelList() {
    auto* provider = CurrentProvider();
    if (!provider) return;
    const std::wstring filter = TrimCopy(providerPage_->modelSearchEdit.GetText());
    const int oldSelection = providerPage_->modelList.GetSelection();
    int selectedModelIndex = -1;
    std::wstring selectedGroupName;
    if (oldSelection >= 0 && oldSelection < static_cast<int>(providerPage_->modelList.GetCount())) {
        const darkui::ListBoxItem selectedItem = providerPage_->modelList.GetItem(oldSelection);
        if (IsGroupHeaderItem(selectedItem)) {
            selectedGroupName = TrimGroupHeaderText(selectedItem.text);
        } else {
            selectedModelIndex = static_cast<int>(selectedItem.userData);
        }
    }
    std::vector<darkui::ListBoxItem> items;
    std::vector<std::pair<std::wstring, std::vector<std::pair<std::size_t, std::wstring>>>> groups;
    std::vector<std::size_t> order(provider->models.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        order[i] = i;
    }
    std::stable_sort(order.begin(), order.end(), [&](std::size_t lhs, std::size_t rhs) {
        const ProviderModel& a = provider->models[lhs];
        const ProviderModel& b = provider->models[rhs];
        if (a.enabled != b.enabled) {
            return a.enabled > b.enabled;
        }
        if (a.custom != b.custom) {
            return a.custom > b.custom;
        }
        const std::wstring ga = GroupNameForModel(a);
        const std::wstring gb = GroupNameForModel(b);
        if (ga != gb) {
            return ga < gb;
        }
        return a.id < b.id;
    });
    for (std::size_t orderedIndex : order) {
        const std::wstring text = FilterModelText(provider->models[orderedIndex]);
        if (!filter.empty() && text.find(filter) == std::wstring::npos) {
            continue;
        }
        const std::wstring groupName = GroupNameForModel(provider->models[orderedIndex]);
        auto it = std::find_if(groups.begin(), groups.end(), [&](const auto& entry) {
            return entry.first == groupName;
        });
        if (it == groups.end()) {
            groups.push_back({groupName, {}});
            it = std::prev(groups.end());
        }
        it->second.push_back({orderedIndex, text});
    }

    std::size_t visibleModelCount = 0;
    std::size_t enabledVisibleCount = 0;
    for (const auto& group : groups) {
        const bool collapsed = std::find(collapsedModelGroups_.begin(), collapsedModelGroups_.end(), group.first) != collapsedModelGroups_.end();
        darkui::ListBoxItem groupItem;
        groupItem.text = std::wstring(collapsed ? L"+ " : L"- ") + group.first + L"  " + std::to_wstring(group.second.size());
        groupItem.userData = kGroupHeaderFlag;
        groupItem.textColor = RGB(130, 170, 220);
        groupItem.backgroundColor = RGB(30, 45, 72);
        items.push_back(std::move(groupItem));
        if (collapsed) {
            continue;
        }
        for (const auto& model : group.second) {
            darkui::ListBoxItem item;
            item.text = L"    " + model.second;
            item.userData = static_cast<std::uintptr_t>(model.first);
            if (provider->models[model.first].enabled) {
                item.textColor = kEnabledGreen;
                ++enabledVisibleCount;
            } else if (provider->models[model.first].custom) {
                item.textColor = kCustomAmber;
            }
            items.push_back(std::move(item));
            ++visibleModelCount;
        }
    }    providerPage_->modelList.SetItems(items);
    providerPage_->modelListLabel.SetText(LoadS(IDS_PROVIDER_MODEL_LIST) + L" " + std::to_wstring(visibleModelCount));
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        const darkui::ListBoxItem& item = items[i];
        if (!selectedGroupName.empty() && IsGroupHeaderItem(item) && TrimGroupHeaderText(item.text) == selectedGroupName) {
            providerPage_->modelList.SetSelection(i, false);
            break;
        }
        if (selectedModelIndex >= 0 && !IsGroupHeaderItem(item) && static_cast<int>(item.userData) == selectedModelIndex) {
            providerPage_->modelList.SetSelection(i, false);
            break;
        }
    }
}

void SettingsWindow::RefreshDefaultsPage() {
    defaultsPage_->providerCombo.SetItems({});
    defaultsPage_->providerCombo.SetSelection(-1, false);

    defaultsPage_->modelChoices.clear();
    std::vector<darkui::ComboItem> modelItems;
    bool accent = false;
    for (const auto& provider : config_.providers) {
        if (!provider.enabled) {
            continue;
        }
        bool providerHasEnabledModel = false;
        for (const auto& model : provider.models) {
            if (!model.enabled) {
                continue;
            }
            providerHasEnabledModel = true;
            const std::size_t choiceIndex = defaultsPage_->modelChoices.size();
            defaultsPage_->modelChoices.push_back({provider.id, model.id});
            darkui::ComboItem item;
            item.text = DefaultModelDisplayText(provider, model);
            item.userData = choiceIndex;
            item.accent = accent;
            modelItems.push_back(std::move(item));
        }
        if (providerHasEnabledModel) {
            accent = !accent;
        }
    }
    defaultsPage_->ocrCombo.SetItems(modelItems);
    defaultsPage_->translateCombo.SetItems(modelItems);

    int ocrSelection = -1;
    int translateSelection = -1;
    for (std::size_t i = 0; i < defaultsPage_->modelChoices.size(); ++i) {
        const auto& choice = defaultsPage_->modelChoices[i];
        if (choice.providerId == config_.defaultOcrProviderId && choice.modelId == config_.defaultOcrModel) {
            ocrSelection = static_cast<int>(i);
        }
        if (choice.providerId == config_.defaultTranslateProviderId && choice.modelId == config_.defaultTranslateModel) {
            translateSelection = static_cast<int>(i);
        }
    }

    if (!defaultsPage_->modelChoices.empty()) {
        if (ocrSelection < 0) {
            ocrSelection = 0;
            config_.defaultOcrProviderId = defaultsPage_->modelChoices[0].providerId;
            config_.defaultOcrModel = defaultsPage_->modelChoices[0].modelId;
        }
        if (translateSelection < 0) {
            translateSelection = 0;
            config_.defaultTranslateProviderId = defaultsPage_->modelChoices[0].providerId;
            config_.defaultTranslateModel = defaultsPage_->modelChoices[0].modelId;
        }
    } else {
        config_.defaultOcrProviderId.clear();
        config_.defaultTranslateProviderId.clear();
        config_.defaultOcrModel.clear();
        config_.defaultTranslateModel.clear();
    }
    defaultsPage_->ocrCombo.SetSelection(ocrSelection, false);
    defaultsPage_->translateCombo.SetSelection(translateSelection, false);
    defaultsPage_->ocrTimeoutEdit.SetText(std::to_wstring(std::clamp(config_.ocrTimeoutSeconds, 1, 300)));
    defaultsPage_->ocrPromptEdit.SetText(config_.ocrPrompt);
    defaultsPage_->translatePromptEdit.SetText(config_.translateTextPrompt);
}

void SettingsWindow::RefreshDisplayPage() {
    displayPage_->themeCombo.SetItems(ToComboItems({LoadS(IDS_THEME_GRAPHITE), LoadS(IDS_THEME_MOSS), LoadS(IDS_THEME_MONO)}));
    displayPage_->themeCombo.SetSelection(ThemeSelection(config_.themeName), false);
    displayPage_->fontCombo.SetItems(ToComboItems({L"16", L"18", L"20", L"22"}));
    const std::wstring fontText = std::to_wstring(config_.fontSize);
    for (int i = 0; i < static_cast<int>(displayPage_->fontCombo.GetCount()); ++i) {
        if (displayPage_->fontCombo.GetItem(i).text == fontText) {
            displayPage_->fontCombo.SetSelection(i, false);
            break;
        }
    }
    displayPage_->startTrayCheck.SetChecked(config_.startInTray);
    displayPage_->copyAfterHotkeyOcrCheck.SetChecked(config_.copyAfterHotkeyOcr);
    displayPage_->ocrResultFilterEdit.SetText(config_.ocrResultFilter);
    displayPage_->translateResultFilterEdit.SetText(config_.translateResultFilter);
}

void SettingsWindow::RefreshHotkeysPage() {
    hotkeyPage_->ocrValue.SetText((captureTarget_ == CaptureTarget::Ocr) ? LoadS(IDS_HOTKEY_WAITING) : HotkeyToText(config_.ocrHotkey));
    hotkeyPage_->translateValue.SetText((captureTarget_ == CaptureTarget::Translate) ? LoadS(IDS_HOTKEY_WAITING) : HotkeyToText(config_.translateHotkey));
}

void SettingsWindow::RefreshModelActions() {
    auto* provider = CurrentProvider();
    const int modelIndex = SelectedModelIndex();
    const bool hasModel = provider && modelIndex >= 0 && modelIndex < static_cast<int>(provider->models.size());
    const bool hasGroup = !SelectedModelGroup().empty();

    EnableWindow(providerPage_->modelAdd.hwnd(), provider != nullptr);
    EnableWindow(providerPage_->modelEdit.hwnd(), hasModel);
    EnableWindow(providerPage_->modelRemove.hwnd(), hasModel || hasGroup);
    EnableWindow(providerPage_->modelToggle.hwnd(), hasModel);
    EnableWindow(providerPage_->fetchModels.hwnd(), provider != nullptr);
    EnableWindow(providerPage_->testModel.hwnd(), hasModel && provider != nullptr);

    if (hasModel && provider->models[modelIndex].enabled) {
        providerPage_->modelToggle.SetText(LoadS(IDS_PROVIDER_DISABLE_ACTION));
    } else {
        providerPage_->modelToggle.SetText(LoadS(IDS_PROVIDER_ENABLE_ACTION));
    }
}

void SettingsWindow::ApplyCurrentTheme() {
    host_.ApplyTheme(MakeAppTheme(config_.themeName, config_.fontSize));
    host_.theme_manager().Apply();
    RefreshDisplayPage();
    Layout();
}

void SettingsWindow::ToggleCurrentProviderEnabled() {
    auto* provider = CurrentProvider();
    if (!provider) {
        return;
    }

    provider->enabled = !provider->enabled;
    if (!provider->enabled && provider->id == config_.defaultProviderId) {
        config_.defaultProviderId.clear();
        for (const auto& item : config_.providers) {
            if (item.enabled) {
                config_.defaultProviderId = item.id;
                break;
            }
        }
        if (config_.defaultProviderId.empty()) {
            config_.defaultOcrModel.clear();
            config_.defaultTranslateModel.clear();
        }
    } else if (provider->enabled && config_.defaultProviderId.empty()) {
        config_.defaultProviderId = provider->id;
    }

    RefreshProviderTable();
    RefreshProviderDetails();
    RefreshDefaultsPage();
    RefreshModelActions();
    SetStatus(LoadS(IDS_STATUS_PROVIDER_UPDATED));
}

void SettingsWindow::DeleteCurrentProvider() {
    auto* provider = CurrentProvider();
    if (!provider || provider->builtIn) {
        return;
    }

    const std::wstring removedId = provider->id;
    config_.providers.erase(config_.providers.begin() + providerSelection_);
    if (config_.providers.empty()) {
        providerSelection_ = -1;
        config_.defaultProviderId.clear();
        config_.defaultOcrModel.clear();
        config_.defaultTranslateModel.clear();
    } else {
        providerSelection_ = std::clamp(providerSelection_, 0, static_cast<int>(config_.providers.size()) - 1);
        if (config_.defaultProviderId == removedId) {
            config_.defaultProviderId.clear();
            for (const auto& item : config_.providers) {
                if (item.enabled) {
                    config_.defaultProviderId = item.id;
                    break;
                }
            }
        }
    }
    RefreshProviderTable();
    RefreshProviderDetails();
    RefreshModelList();
    RefreshDefaultsPage();
    RefreshModelActions();
    SetStatus(LoadS(IDS_STATUS_PROVIDER_UPDATED));
}

void SettingsWindow::CommitCurrentProvider() {
    auto* provider = CurrentProvider();
    if (!provider) return;
    provider->name = providerPage_->nameEdit.GetText();
    provider->apiKey = providerPage_->keyEdit.GetText();
    provider->baseUrl = providerPage_->baseEdit.GetText();
    provider->apiPath = providerPage_->pathEdit.GetText();
    provider->streamResponse = providerPage_->streamCheck.GetChecked();
}

void SettingsWindow::SaveConfig() {
    CommitCurrentProvider();
    const std::wstring ocrPrompt = TrimCopy(defaultsPage_ ? defaultsPage_->ocrPromptEdit.GetText() : config_.ocrPrompt);
    const std::wstring translatePrompt = TrimCopy(defaultsPage_ ? defaultsPage_->translatePromptEdit.GetText() : config_.translateTextPrompt);
    if (ocrPrompt.empty()) {
        MessageBoxW(hwnd_, L"OCR提示词不能为空。", LoadS(IDS_APP_TITLE).c_str(), MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (translatePrompt.empty()) {
        MessageBoxW(hwnd_, L"文本翻译提示词不能为空。", LoadS(IDS_APP_TITLE).c_str(), MB_OK | MB_ICONINFORMATION);
        return;
    }
    config_.ocrPrompt = ocrPrompt;
    config_.translateTextPrompt = translatePrompt;
    const bool priorStartInTray = config_.startInTray;
    bool uiStartInTray = priorStartInTray;
    if (displayPage_) {
        // Read back directly from controls at save time so UI state is persisted
        // even if a control click notification is missed.
        uiStartInTray = displayPage_->startTrayCheck.GetChecked();
        config_.startInTray = uiStartInTray;
        config_.copyAfterHotkeyOcr = displayPage_->copyAfterHotkeyOcrCheck.GetChecked();
        config_.ocrResultFilter = displayPage_->ocrResultFilterEdit.GetText();
        config_.translateResultFilter = displayPage_->translateResultFilterEdit.GetText();
        config_.themeName = ThemeStorageName(displayPage_->themeCombo.GetSelection());
        config_.fontSize = _wtoi(displayPage_->fontCombo.GetText().c_str());
    }
    if (defaultsPage_) {
        const int providerSelection = defaultsPage_->providerCombo.GetSelection();
        if (providerSelection >= 0 && providerSelection < static_cast<int>(defaultsPage_->providerCombo.GetCount())) {
            const int providerIndex = static_cast<int>(defaultsPage_->providerCombo.GetItem(providerSelection).userData);
            if (providerIndex >= 0 && providerIndex < static_cast<int>(config_.providers.size())) {
                config_.defaultProviderId = config_.providers[providerIndex].id;
            }
        }
        const int ocrSelection = defaultsPage_->ocrCombo.GetSelection();
        if (ocrSelection >= 0 && ocrSelection < static_cast<int>(defaultsPage_->modelChoices.size())) {
            config_.defaultOcrProviderId = defaultsPage_->modelChoices[ocrSelection].providerId;
            config_.defaultOcrModel = defaultsPage_->modelChoices[ocrSelection].modelId;
        }
        const int translateSelection = defaultsPage_->translateCombo.GetSelection();
        if (translateSelection >= 0 && translateSelection < static_cast<int>(defaultsPage_->modelChoices.size())) {
            config_.defaultTranslateProviderId = defaultsPage_->modelChoices[translateSelection].providerId;
            config_.defaultTranslateModel = defaultsPage_->modelChoices[translateSelection].modelId;
        }
        int timeoutSeconds = _wtoi(defaultsPage_->ocrTimeoutEdit.GetText().c_str());
        if (timeoutSeconds <= 0) {
            timeoutSeconds = 6;
        }
        config_.ocrTimeoutSeconds = std::clamp(timeoutSeconds, 1, 300);
    }
    std::wstring error;    if (!store_.Save(config_, error)) {        SetStatus(error);
        return;
    }    SetStatus(LoadS(IDS_STATUS_SETTINGS_SAVED));
    if (onConfigSaved_) {
        onConfigSaved_();
    }
}

void SettingsWindow::AddCustomProvider() {
    ProviderConfig provider;
    provider.id = L"custom-" + std::to_wstring(config_.providers.size() + 1);
    provider.name = LoadS(IDS_CUSTOM_PROVIDER_PREFIX) + L" " + std::to_wstring(config_.providers.size() + 1);
    provider.baseUrl = L"https://api.openai.com";
    provider.apiPath = L"/v1/chat/completions";
    provider.enabled = true;
    provider.builtIn = false;
    config_.providers.push_back(provider);
    providerSelection_ = static_cast<int>(config_.providers.size() - 1);
    RefreshProviderTable();
    RefreshProviderDetails();
    RefreshModelList();
    RefreshDefaultsPage();
    RefreshModelActions();
    SetStatus(LoadS(IDS_STATUS_PROVIDER_ADDED));
}

void SettingsWindow::AddCustomModel() {
    auto* provider = CurrentProvider();
    if (!provider) {
        return;
    }

    std::wstring modelId;
    std::wstring modelName;
    bool modelReasoning = false;
    if (!ShowModelDialog(modelId, modelName, modelReasoning, false)) {
        return;
    }

    const auto exists = std::any_of(provider->models.begin(), provider->models.end(), [&](const ProviderModel& model) {
        return model.id == modelId;
    });
    if (exists) {
        SetStatus(LoadS(IDS_STATUS_PROVIDER_UPDATED));
        return;
    }

    ProviderModel model;
    model.id = modelId;
    model.displayName = modelName;
    model.enabled = true;
    model.custom = true;
    model.reasoning = modelReasoning;
    provider->models.push_back(model);
    RefreshModelList();
    RefreshDefaultsPage();
    RefreshModelActions();
    SetStatus(LoadS(IDS_STATUS_PROVIDER_UPDATED));
}

bool SettingsWindow::ShowModelDialog(std::wstring& modelId, std::wstring& modelName, bool& modelReasoning, bool editing) {
    AddModelDialogSession session;
    darkui::Dialog::Options dialogOptions;
    dialogOptions.title = editing ? LoadS(IDS_DIALOG_EDIT_MODEL_TITLE) : LoadS(IDS_DIALOG_ADD_MODEL_TITLE);
    dialogOptions.confirmText = editing ? LoadS(IDS_PROVIDER_MODEL_EDIT) : LoadS(IDS_PROVIDER_MODEL_ADD);
    dialogOptions.cancelText = LoadS(IDS_CLOSE);
    dialogOptions.width = 560;
    dialogOptions.height = 360;
    dialogOptions.messageVisible = false;
    if (!session.dialog.Create(hwnd_, 9200, host_.theme(), dialogOptions)) {
        return false;
    }

    darkui::Panel::Options panelOptions;
    panelOptions.role = darkui::SurfaceRole::Panel;
    panelOptions.cornerRadius = 18;
    darkui::Static::Options labelOptions;
    labelOptions.variant = darkui::StaticVariant::PanelTitle;
    darkui::Edit::Options editOptions;
    editOptions.variant = darkui::FieldVariant::Panel;
    editOptions.cueBanner = LoadS(IDS_PROVIDER_MODEL_ID_HINT);
    darkui::CheckBox::Options checkOptions;
    checkOptions.variant = darkui::SelectionVariant::Accent;
    checkOptions.text = LoadS(IDS_PROVIDER_MODEL_REASONING);

    labelOptions.text = LoadS(IDS_PROVIDER_MODEL_ID);
    darkui::Static::Options nameLabelOptions = labelOptions;
    nameLabelOptions.text = LoadS(IDS_PROVIDER_MODEL_NAME);

    if (!session.formPanel.Create(session.dialog.content_hwnd(), ID_DIALOG_MODEL_PANEL, host_.theme(), panelOptions) ||
        !session.idLabel.Create(session.formPanel.hwnd(), ID_DIALOG_MODEL_ID_LABEL, host_.theme(), labelOptions) ||
        !session.idEdit.Create(session.formPanel.hwnd(), ID_DIALOG_MODEL_ID_EDIT, host_.theme(), editOptions) ||
        !session.nameLabel.Create(session.formPanel.hwnd(), ID_DIALOG_MODEL_NAME_LABEL, host_.theme(), nameLabelOptions) ||
        !session.nameEdit.Create(session.formPanel.hwnd(), ID_DIALOG_MODEL_NAME_EDIT, host_.theme(), editOptions) ||
        !session.reasoningCheck.Create(session.formPanel.hwnd(), ID_DIALOG_MODEL_REASONING_CHECK, host_.theme(), checkOptions)) {
        return false;
    }

    session.idEdit.SetText(modelId);
    session.nameEdit.SetText(modelName);
    session.reasoningCheck.SetChecked(modelReasoning);
    if (editing) {
        EnableWindow(session.idEdit.hwnd(), FALSE);
    }

    session.themeManager.SetTheme(host_.theme());
    session.themeManager.Bind(
        session.dialog, session.formPanel, session.idLabel, session.idEdit, session.nameLabel, session.nameEdit, session.reasoningCheck);

    MoveWindow(session.formPanel.hwnd(), 16, 12, 496, 238, TRUE);
    MoveWindow(session.idLabel.hwnd(), 18, 16, 140, 28, TRUE);
    MoveWindow(session.idEdit.hwnd(), 18, 48, 460, 38, TRUE);
    MoveWindow(session.nameLabel.hwnd(), 18, 98, 140, 28, TRUE);
    MoveWindow(session.nameEdit.hwnd(), 18, 130, 460, 38, TRUE);
    MoveWindow(session.reasoningCheck.hwnd(), 18, 184, 220, 30, TRUE);

    const darkui::Dialog::Result result = session.dialog.ShowModal();
    if (result != darkui::Dialog::Result::Confirm) {
        return false;
    }

    modelId = TrimCopy(session.idEdit.GetText());
    modelName = TrimCopy(session.nameEdit.GetText());
    modelReasoning = session.reasoningCheck.GetChecked();
    if (modelId.empty()) {
        MessageBoxW(hwnd_, LoadS(IDS_MSG_INPUT_MODEL_ID).c_str(), LoadS(IDS_APP_TITLE).c_str(), MB_OK | MB_ICONINFORMATION);
        return false;
    }
    return true;
}

void SettingsWindow::EditSelectedModel() {
    auto* provider = CurrentProvider();
    const int modelIndex = SelectedModelIndex();
    if (!provider || modelIndex < 0 || modelIndex >= static_cast<int>(provider->models.size())) {
        return;
    }

    std::wstring modelId = provider->models[modelIndex].id;
    std::wstring modelName = provider->models[modelIndex].displayName;
    bool modelReasoning = provider->models[modelIndex].reasoning;
    if (!ShowModelDialog(modelId, modelName, modelReasoning, true)) {
        return;
    }

    provider->models[modelIndex].displayName = modelName;
    provider->models[modelIndex].reasoning = modelReasoning;
    RefreshModelList();
    RefreshDefaultsPage();
    RefreshModelActions();
    SetStatus(LoadS(IDS_STATUS_PROVIDER_UPDATED));
}

void SettingsWindow::OpenModelTestDialog() {
    auto* provider = CurrentProvider();
    const int modelIndex = SelectedModelIndex();
    if (!provider || modelIndex < 0 || modelIndex >= static_cast<int>(provider->models.size())) {
        return;
    }

    testDialog_ = std::make_unique<TestDialogSession>();
    testDialog_->modelId = provider->models[modelIndex].id;
    darkui::Dialog::Options dialogOptions;
    dialogOptions.title = provider->models[modelIndex].id;
    dialogOptions.confirmText = LoadS(IDS_CLOSE);
    dialogOptions.cancelVisible = false;
    dialogOptions.width = 780;
    dialogOptions.height = 620;
    dialogOptions.messageVisible = false;
    if (!testDialog_->dialog.Create(hwnd_, 9300, host_.theme(), dialogOptions)) {
        testDialog_.reset();
        return;
    }

    darkui::Static::Options labelOptions;
    labelOptions.variant = darkui::StaticVariant::PanelTitle;

    darkui::Edit::Options promptOptions;
    promptOptions.variant = darkui::FieldVariant::Panel;
    promptOptions.style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL;

    darkui::Edit::Options outputOptions = promptOptions;

    darkui::Button::Options sendOptions;
    sendOptions.variant = darkui::ButtonVariant::Primary;
    sendOptions.text = L"Send";

    labelOptions.text = L"Prompt";
    darkui::Static::Options responseLabelOptions = labelOptions;
    responseLabelOptions.text = L"Streaming Output";

    if (!testDialog_->promptLabel.Create(testDialog_->dialog.content_hwnd(), ID_DIALOG_TEST_PROMPT_LABEL, host_.theme(), labelOptions) ||
        !testDialog_->promptEdit.Create(testDialog_->dialog.content_hwnd(), ID_DIALOG_TEST_PROMPT_EDIT, host_.theme(), promptOptions) ||
        !testDialog_->sendButton.Create(testDialog_->dialog.content_hwnd(), ID_DIALOG_TEST_SEND_BUTTON, host_.theme(), sendOptions) ||
        !testDialog_->responseLabel.Create(testDialog_->dialog.content_hwnd(), ID_DIALOG_TEST_RESPONSE_LABEL, host_.theme(), responseLabelOptions) ||
        !testDialog_->responseEdit.Create(testDialog_->dialog.content_hwnd(), ID_DIALOG_TEST_RESPONSE_EDIT, host_.theme(), outputOptions)) {
        testDialog_.reset();
        return;
    }

    testDialog_->promptEdit.SetText(L"今日天气");
    testDialog_->responseEdit.SetText(L"");
    testDialog_->themeManager.SetTheme(host_.theme());
    testDialog_->themeManager.Bind(testDialog_->dialog,
                                   testDialog_->promptLabel,
                                   testDialog_->promptEdit,
                                   testDialog_->sendButton,
                                   testDialog_->responseLabel,
                                   testDialog_->responseEdit);

    MoveWindow(testDialog_->promptLabel.hwnd(), 34, 28, 160, 28, TRUE);
    MoveWindow(testDialog_->promptEdit.hwnd(), 34, 60, 524, 100, TRUE);
    MoveWindow(testDialog_->sendButton.hwnd(), 576, 72, 120, 40, TRUE);
    MoveWindow(testDialog_->responseLabel.hwnd(), 34, 180, 160, 28, TRUE);
    MoveWindow(testDialog_->responseEdit.hwnd(), 34, 212, 662, 260, TRUE);

    testDialog_->dialog.ShowModal();
    testDialog_.reset();
}

void SettingsWindow::StartModelTestRequest() {
    auto* provider = CurrentProvider();
    if (!provider || !testDialog_ || testDialog_->busy) {        return;
    }

    const std::wstring prompt = TrimCopy(testDialog_->promptEdit.GetText());
    if (prompt.empty()) {
        testDialog_->promptEdit.SetText(L"今日天气");
    }
    const std::wstring finalPrompt = TrimCopy(testDialog_->promptEdit.GetText());
    testDialog_->busy = true;
    EnableWindow(testDialog_->sendButton.hwnd(), FALSE);
    testDialog_->responseEdit.SetText(L"");
    testDialog_->requestId = ++testRequestId_;    SetStatus(L"Testing " + testDialog_->modelId + L" ...");

    ProviderConfig providerCopy = *provider;
    const std::wstring modelId = testDialog_->modelId;
    const unsigned long long requestId = testDialog_->requestId;
    HWND targetWindow = hwnd_;
    std::thread([this, providerCopy, modelId, finalPrompt, requestId, targetWindow]() {
        RequestOptions options;
        options.stream = true;
        bool modelReasoning = false;
        bool found = false;
        for (const auto& model : providerCopy.models) {
            if (model.id == modelId) {
                modelReasoning = model.reasoning;
                found = true;
                break;
            }
        }
        options.enableReasoning = modelReasoning;
        options.includeReasoningOption = found && modelReasoning;

        auto postChunk = [targetWindow, requestId](const std::wstring& chunk) {
            auto* payload = new TestChunkPayload();
            payload->requestId = requestId;
            payload->text = chunk;
            if (!PostMessageW(targetWindow, WM_APP_TEST_STREAM_CHUNK, 0, reinterpret_cast<LPARAM>(payload))) {
                delete payload;
            }
        };

        ServiceResult result = service_.TestModel(
            providerCopy,
            modelId,
            finalPrompt,
            options,
            OcrService::StreamCallback(postChunk));
        auto* done = new TestDonePayload();
        done->requestId = requestId;
        done->result = std::move(result);
        if (!PostMessageW(targetWindow, WM_APP_TEST_DONE, 0, reinterpret_cast<LPARAM>(done))) {
            delete done;
        }
    }).detach();
}

int SettingsWindow::SelectedModelIndex() const {
    if (!providerPage_) {
        return -1;
    }
    const int selection = providerPage_->modelList.GetSelection();
    if (selection < 0 || selection >= static_cast<int>(providerPage_->modelList.GetCount())) {
        return -1;
    }
    const darkui::ListBoxItem item = providerPage_->modelList.GetItem(selection);
    if (IsGroupHeaderItem(item)) {
        return -1;
    }
    return static_cast<int>(item.userData);
}

std::wstring SettingsWindow::SelectedModelGroup() const {
    if (!providerPage_) {
        return L"";
    }
    const int selection = providerPage_->modelList.GetSelection();
    if (selection < 0 || selection >= static_cast<int>(providerPage_->modelList.GetCount())) {
        return L"";
    }
    const darkui::ListBoxItem item = providerPage_->modelList.GetItem(selection);
    if (!IsGroupHeaderItem(item)) {
        return L"";
    }
    return TrimGroupHeaderText(item.text);
}

void SettingsWindow::SetStatus(const std::wstring& text) {
    statusText_ = text;
    SetWindowTextW(hwnd_, (LoadS(IDS_SETTINGS_TITLE) + L" - " + statusText_).c_str());
}

ProviderConfig* SettingsWindow::CurrentProvider() {
    if (providerSelection_ < 0 || providerSelection_ >= static_cast<int>(config_.providers.size())) {
        return nullptr;
    }
    return &config_.providers[providerSelection_];
}

const ProviderConfig* SettingsWindow::CurrentProvider() const {
    if (providerSelection_ < 0 || providerSelection_ >= static_cast<int>(config_.providers.size())) {
        return nullptr;
    }
    return &config_.providers[providerSelection_];
}

}  // namespace app


