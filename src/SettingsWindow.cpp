#include "SettingsWindow.h"

#include "AppIds.h"
#include "AppTheme.h"
#include "UiText.h"

#include <commctrl.h>

namespace app {

struct SettingsWindow::ProviderPage {
    darkui::Panel root;
    darkui::ListBox providerList;
    darkui::Button addProvider;
    darkui::Static nameLabel;
    darkui::Static keyLabel;
    darkui::Static baseLabel;
    darkui::Static pathLabel;
    darkui::Edit nameEdit;
    darkui::Edit keyEdit;
    darkui::Edit baseEdit;
    darkui::Edit pathEdit;
    darkui::Static modelInputLabel;
    darkui::Edit modelInputEdit;
    darkui::Static modelSearchLabel;
    darkui::Edit modelSearchEdit;
    darkui::Static modelListLabel;
    darkui::ListBox modelList;
    darkui::Button modelAdd;
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
    darkui::ComboBox providerCombo;
    darkui::ComboBox ocrCombo;
    darkui::ComboBox translateCombo;
};

struct SettingsWindow::DisplayPage {
    darkui::Panel root;
    darkui::Static themeLabel;
    darkui::Static fontLabel;
    darkui::ComboBox themeCombo;
    darkui::ComboBox fontCombo;
};

struct SettingsWindow::HotkeyPage {
    darkui::Panel root;
    darkui::Static help;
    darkui::Static ocrLabel;
    darkui::Static ocrValue;
    darkui::Button ocrCapture;
    darkui::Button ocrClear;
    darkui::Static translateLabel;
    darkui::Static translateValue;
    darkui::Button translateCapture;
    darkui::Button translateClear;
};

namespace {

std::wstring LoadS(UINT id) {
    return LoadStringResource(id);
}

std::vector<darkui::ComboItem> ToComboItems(const std::vector<std::wstring>& values) {
    std::vector<darkui::ComboItem> items;
    for (std::size_t i = 0; i < values.size(); ++i) {
        items.push_back({values[i], i, false});
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
    return model.id + (model.enabled ? L" [" + LoadS(IDS_PROVIDER_ENABLED) + L"]"
                                     : L" [" + LoadS(IDS_PROVIDER_DISABLED) + L"]");
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
}

bool SettingsWindow::Create(HINSTANCE instance, HWND owner) {
    instance_ = instance;
    owner_ = owner;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance_;
    wc.lpszClassName = L"Win32OcrSettingsWindow";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(18, 20, 24));
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
    return hwnd_ != nullptr;
}

void SettingsWindow::Show() {
    if (hwnd_) {
        PositionWindow();
        ShowWindow(hwnd_, SW_SHOW);
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
    case WM_COMMAND:
        OnCommand(wParam, lParam);
        return 0;
    case WM_NOTIFY:
        OnNotify(lParam);
        return 0;
    case WM_KEYDOWN:
        OnKeyDown(static_cast<UINT>(wParam));
        return 0;
    case WM_ERASEBKGND:
        if (host_.HandleEraseBackground(reinterpret_cast<HDC>(wParam))) {
            return 1;
        }
        break;
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
        {LoadS(IDS_TAB_DEFAULTS), 1},
        {LoadS(IDS_TAB_DISPLAY), 2},
        {LoadS(IDS_TAB_HOTKEY), 3}
    };
    tabOptions.selection = 0;
    tabOptions.variant = darkui::TabVariant::Accent;
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
    labelOptions.text = LoadS(IDS_PROVIDER_MODEL_INPUT);
    providerPage_->modelInputLabel.Create(providerPage_->root.hwnd(), 8114, host_.theme(), labelOptions);
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
    providerPage_->modelInputEdit.Create(providerPage_->root.hwnd(), ID_PROVIDER_MODEL_INPUT_EDIT, host_.theme(), fieldOptions);
    providerPage_->modelSearchEdit.Create(providerPage_->root.hwnd(), ID_PROVIDER_MODEL_SEARCH_EDIT, host_.theme(), fieldOptions);

    darkui::ListBox::Options listOptions;
    listOptions.variant = darkui::FieldVariant::Default;
    providerPage_->modelList.Create(providerPage_->root.hwnd(), ID_PROVIDER_MODEL_LIST, host_.theme(), listOptions);

    darkui::Button::Options smallButton;
    smallButton.variant = darkui::ButtonVariant::Secondary;
    smallButton.text = LoadS(IDS_PROVIDER_MODEL_ADD);
    providerPage_->modelAdd.Create(providerPage_->root.hwnd(), ID_PROVIDER_MODEL_ADD, host_.theme(), smallButton);
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

    darkui::ComboBox::Options comboOptions;
    comboOptions.variant = darkui::FieldVariant::Panel;
    defaultsPage_->providerCombo.Create(defaultsPage_->root.hwnd(), ID_DEFAULT_PROVIDER_COMBO, host_.theme(), comboOptions);
    defaultsPage_->ocrCombo.Create(defaultsPage_->root.hwnd(), ID_DEFAULT_OCR_MODEL_COMBO, host_.theme(), comboOptions);
    defaultsPage_->translateCombo.Create(defaultsPage_->root.hwnd(), ID_DEFAULT_TRANSLATE_MODEL_COMBO, host_.theme(), comboOptions);

    labelOptions.text = LoadS(IDS_DISPLAY_THEME);
    displayPage_->themeLabel.Create(displayPage_->root.hwnd(), 8300, host_.theme(), labelOptions);
    labelOptions.text = LoadS(IDS_DISPLAY_FONT_SIZE);
    displayPage_->fontLabel.Create(displayPage_->root.hwnd(), 8301, host_.theme(), labelOptions);
    displayPage_->themeCombo.Create(displayPage_->root.hwnd(), ID_DISPLAY_THEME_COMBO, host_.theme(), comboOptions);
    displayPage_->fontCombo.Create(displayPage_->root.hwnd(), ID_DISPLAY_FONT_COMBO, host_.theme(), comboOptions);

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
    hotkeyPage_->translateCapture.Create(hotkeyPage_->root.hwnd(), ID_HOTKEY_CAPTURE_TRANSLATE, host_.theme(), smallButton);
    smallButton.text = LoadS(IDS_HOTKEY_CLEAR);
    hotkeyPage_->ocrClear.Create(hotkeyPage_->root.hwnd(), ID_HOTKEY_CLEAR_OCR, host_.theme(), smallButton);
    hotkeyPage_->translateClear.Create(hotkeyPage_->root.hwnd(), ID_HOTKEY_CLEAR_TRANSLATE, host_.theme(), smallButton);

    host_.theme_manager().Bind(tab_, saveButton_, closeButton_);
    RefreshAll();
    Layout();
    return true;
}

void SettingsWindow::OnDestroy() {
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
    case ID_PROVIDER_TABLE:
        if (HIWORD(wParam) == LBN_SELCHANGE) {
            const int selected = providerPage_->providerList.GetSelection();
            if (selected >= 0 && selected < static_cast<int>(config_.providers.size()) && selected != providerSelection_) {
                CommitCurrentProvider();
                providerSelection_ = selected;
                RefreshProviderDetails();
                RefreshModelList();
            }
        }
        return;
    case ID_PROVIDER_MODEL_ADD: {
        auto* provider = CurrentProvider();
        if (!provider) return;
        const std::wstring value = providerPage_->modelInputEdit.GetText();
        if (value.empty()) return;
        provider->models.push_back({value, true});
        providerPage_->modelInputEdit.SetText(L"");
        RefreshModelList();
        RefreshDefaultsPage();
        SetStatus(LoadS(IDS_STATUS_PROVIDER_UPDATED));
        return;
    }
    case ID_PROVIDER_MODEL_REMOVE: {
        auto* provider = CurrentProvider();
        if (!provider) return;
        const int selection = providerPage_->modelList.GetSelection();
        if (selection >= 0 && selection < static_cast<int>(provider->models.size())) {
            provider->models.erase(provider->models.begin() + selection);
            RefreshModelList();
            RefreshDefaultsPage();
        }
        return;
    }
    case ID_PROVIDER_MODEL_TOGGLE: {
        auto* provider = CurrentProvider();
        if (!provider) return;
        const int selection = providerPage_->modelList.GetSelection();
        if (selection >= 0 && selection < static_cast<int>(provider->models.size())) {
            provider->models[selection].enabled = !provider->models[selection].enabled;
            RefreshModelList();
            RefreshDefaultsPage();
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
        provider->models.clear();
        for (const auto& model : result.items) {
            provider->models.push_back({model, true});
        }
        RefreshModelList();
        RefreshDefaultsPage();
        SetStatus(LoadS(IDS_STATUS_FETCH_MODELS_DONE));
        return;
    }
    case ID_PROVIDER_TEST_MODEL: {
        auto* provider = CurrentProvider();
        if (!provider || provider->apiKey.empty()) {
            MessageBoxW(hwnd_, LoadS(IDS_MSG_INPUT_API_KEY).c_str(), LoadS(IDS_APP_TITLE).c_str(), MB_OK | MB_ICONINFORMATION);
            return;
        }
        const int selection = providerPage_->modelList.GetSelection();
        if (selection < 0 || selection >= static_cast<int>(provider->models.size())) {
            MessageBoxW(hwnd_, LoadS(IDS_MSG_INPUT_MODEL_NAME).c_str(), LoadS(IDS_APP_TITLE).c_str(), MB_OK | MB_ICONINFORMATION);
            return;
        }
        const ServiceResult result = service_.TestModel(*provider, provider->models[selection].id);
        SetStatus(result.success ? LoadS(IDS_STATUS_TEST_MODEL_DONE)
                                 : FormatStatus(LoadS(IDS_STATUS_TEST_MODEL_FAILED), result.error));
        return;
    }
    case ID_SETTINGS_SAVE:
        SaveConfig();
        return;
    case ID_SETTINGS_CLOSE:
        DestroyWindow(hwnd_);
        return;
    case ID_HOTKEY_CAPTURE_OCR:
        captureTarget_ = CaptureTarget::Ocr;
        RefreshHotkeysPage();
        return;
    case ID_HOTKEY_CAPTURE_TRANSLATE:
        captureTarget_ = CaptureTarget::Translate;
        RefreshHotkeysPage();
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
            if (selection >= 0 && selection < static_cast<int>(config_.providers.size())) {
                config_.defaultProviderId = config_.providers[selection].id;
                RefreshDefaultsPage();
            }
        }
        return;
    case ID_DEFAULT_OCR_MODEL_COMBO:
        if (HIWORD(wParam) == CBN_SELCHANGE) {
            config_.defaultOcrModel = defaultsPage_->ocrCombo.GetText();
        }
        return;
    case ID_DEFAULT_TRANSLATE_MODEL_COMBO:
        if (HIWORD(wParam) == CBN_SELCHANGE) {
            config_.defaultTranslateModel = defaultsPage_->translateCombo.GetText();
        }
        return;
    case ID_DISPLAY_THEME_COMBO:
        if (HIWORD(wParam) == CBN_SELCHANGE) {
            config_.themeName = ThemeStorageName(displayPage_->themeCombo.GetSelection());
        }
        return;
    case ID_DISPLAY_FONT_COMBO:
        if (HIWORD(wParam) == CBN_SELCHANGE) {
            config_.fontSize = _wtoi(displayPage_->fontCombo.GetText().c_str());
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
        Layout();
        return;
    }
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
    if (captureTarget_ == CaptureTarget::Ocr) {
        config_.ocrHotkey = {modifiers, virtualKey};
    } else {
        config_.translateHotkey = {modifiers, virtualKey};
    }
    captureTarget_ = CaptureTarget::None;
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
    const int contentGap = 18;
    const int pageLeft = content.left + contentGap;
    const int pageWidth = (content.right - content.left) - contentGap;
    const int pageHeight = content.bottom - content.top;

    MoveWindow(providerPage_->root.hwnd(), pageLeft, content.top, pageWidth, pageHeight, TRUE);
    MoveWindow(defaultsPage_->root.hwnd(), pageLeft, content.top, pageWidth, pageHeight, TRUE);
    MoveWindow(displayPage_->root.hwnd(), pageLeft, content.top, pageWidth, pageHeight, TRUE);
    MoveWindow(hotkeyPage_->root.hwnd(), pageLeft, content.top, pageWidth, pageHeight, TRUE);

    MoveWindow(providerPage_->providerList.hwnd(), 40, 40, 240, pageHeight - 138, TRUE);
    MoveWindow(providerPage_->addProvider.hwnd(), 40, pageHeight - 78, 240, 36, TRUE);

    int y = 24;
    const int labelWidth = 96;
    const int formLeft = 304;
    const int fieldX = formLeft + 108;
    const int fieldWidth = pageWidth - fieldX - 16;
    auto placeField = [&](HWND label, HWND field) {
        MoveWindow(label, formLeft, y, labelWidth, 26, TRUE);
        MoveWindow(field, fieldX, y - 4, fieldWidth, 34, TRUE);
        y += 48;
    };
    placeField(providerPage_->nameLabel.hwnd(), providerPage_->nameEdit.hwnd());
    placeField(providerPage_->keyLabel.hwnd(), providerPage_->keyEdit.hwnd());
    placeField(providerPage_->baseLabel.hwnd(), providerPage_->baseEdit.hwnd());
    placeField(providerPage_->pathLabel.hwnd(), providerPage_->pathEdit.hwnd());
    placeField(providerPage_->modelInputLabel.hwnd(), providerPage_->modelInputEdit.hwnd());
    placeField(providerPage_->modelSearchLabel.hwnd(), providerPage_->modelSearchEdit.hwnd());
    MoveWindow(providerPage_->modelListLabel.hwnd(), formLeft, y, 120, 26, TRUE);
    MoveWindow(providerPage_->modelList.hwnd(), fieldX, y - 4, fieldWidth, 190, TRUE);
    y += 210;
    const int buttonWidth = (fieldWidth - 16) / 3;
    MoveWindow(providerPage_->modelAdd.hwnd(), fieldX, y, buttonWidth, 34, TRUE);
    MoveWindow(providerPage_->modelRemove.hwnd(), fieldX + buttonWidth + 8, y, buttonWidth, 34, TRUE);
    MoveWindow(providerPage_->modelToggle.hwnd(), fieldX + (buttonWidth + 8) * 2, y, buttonWidth, 34, TRUE);
    y += 44;
    MoveWindow(providerPage_->fetchModels.hwnd(), fieldX, y, buttonWidth + 20, 34, TRUE);
    MoveWindow(providerPage_->testModel.hwnd(), fieldX + buttonWidth + 28, y, buttonWidth + 20, 34, TRUE);

    MoveWindow(defaultsPage_->providerLabel.hwnd(), 28, 40, 160, 26, TRUE);
    MoveWindow(defaultsPage_->providerCombo.hwnd(), 220, 34, 380, 34, TRUE);
    MoveWindow(defaultsPage_->ocrLabel.hwnd(), 28, 104, 160, 26, TRUE);
    MoveWindow(defaultsPage_->ocrCombo.hwnd(), 220, 98, 380, 34, TRUE);
    MoveWindow(defaultsPage_->translateLabel.hwnd(), 28, 168, 160, 26, TRUE);
    MoveWindow(defaultsPage_->translateCombo.hwnd(), 220, 162, 380, 34, TRUE);

    MoveWindow(displayPage_->themeLabel.hwnd(), 28, 40, 160, 26, TRUE);
    MoveWindow(displayPage_->themeCombo.hwnd(), 220, 34, 260, 34, TRUE);
    MoveWindow(displayPage_->fontLabel.hwnd(), 28, 104, 160, 26, TRUE);
    MoveWindow(displayPage_->fontCombo.hwnd(), 220, 98, 260, 34, TRUE);

    MoveWindow(hotkeyPage_->help.hwnd(), 28, 24, 520, 30, TRUE);
    MoveWindow(hotkeyPage_->ocrLabel.hwnd(), 28, 90, 160, 26, TRUE);
    MoveWindow(hotkeyPage_->ocrValue.hwnd(), 220, 86, 240, 32, TRUE);
    MoveWindow(hotkeyPage_->ocrCapture.hwnd(), 480, 84, 120, 34, TRUE);
    MoveWindow(hotkeyPage_->ocrClear.hwnd(), 612, 84, 88, 34, TRUE);
    MoveWindow(hotkeyPage_->translateLabel.hwnd(), 28, 154, 160, 26, TRUE);
    MoveWindow(hotkeyPage_->translateValue.hwnd(), 220, 150, 240, 32, TRUE);
    MoveWindow(hotkeyPage_->translateCapture.hwnd(), 480, 148, 120, 34, TRUE);
    MoveWindow(hotkeyPage_->translateClear.hwnd(), 612, 148, 88, 34, TRUE);

    RedrawWindow(providerPage_->modelList.hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE | RDW_ALLCHILDREN);
    RedrawWindow(providerPage_->root.hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE | RDW_ALLCHILDREN);
    RedrawWindow(defaultsPage_->root.hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE | RDW_ALLCHILDREN);
    RedrawWindow(displayPage_->root.hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE | RDW_ALLCHILDREN);
    RedrawWindow(hotkeyPage_->root.hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE | RDW_ALLCHILDREN);
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
    SetStatus(LoadS(IDS_STATUS_READY));
}

void SettingsWindow::RefreshProviderTable() {
    std::vector<darkui::ListBoxItem> rows;
    for (const auto& provider : config_.providers) {
        rows.push_back({provider.name + L"  " + (provider.enabled ? LoadS(IDS_PROVIDER_ENABLED) : LoadS(IDS_PROVIDER_DISABLED)), 0});
    }
    providerPage_->providerList.SetItems(rows);
    providerPage_->providerList.SetSelection(providerSelection_, false);
}

void SettingsWindow::RefreshProviderDetails() {
    const ProviderConfig* provider = CurrentProvider();
    if (!provider) return;
    providerPage_->nameEdit.SetText(provider->name);
    providerPage_->keyEdit.SetText(provider->apiKey);
    providerPage_->baseEdit.SetText(provider->baseUrl);
    providerPage_->pathEdit.SetText(provider->apiPath);
}

void SettingsWindow::RefreshModelList() {
    auto* provider = CurrentProvider();
    if (!provider) return;
    const std::wstring filter = providerPage_->modelSearchEdit.GetText();
    std::vector<darkui::ListBoxItem> items;
    for (std::size_t i = 0; i < provider->models.size(); ++i) {
        const std::wstring text = FilterModelText(provider->models[i]);
        if (!filter.empty() && text.find(filter) == std::wstring::npos) {
            continue;
        }
        items.push_back({text, i});
    }
    providerPage_->modelList.SetItems(items);
}

void SettingsWindow::RefreshDefaultsPage() {
    std::vector<std::wstring> providerNames;
    int providerIndex = 0;
    for (std::size_t i = 0; i < config_.providers.size(); ++i) {
        providerNames.push_back(config_.providers[i].name);
        if (config_.providers[i].id == config_.defaultProviderId) {
            providerIndex = static_cast<int>(i);
        }
    }
    defaultsPage_->providerCombo.SetItems(ToComboItems(providerNames));
    defaultsPage_->providerCombo.SetSelection(providerIndex, false);

    const ProviderConfig* defaultProvider = nullptr;
    for (const auto& provider : config_.providers) {
        if (provider.id == config_.defaultProviderId) {
            defaultProvider = &provider;
            break;
        }
    }
    std::vector<std::wstring> modelNames;
    if (defaultProvider) {
        for (const auto& model : defaultProvider->models) {
            if (model.enabled) modelNames.push_back(model.id);
        }
    }
    defaultsPage_->ocrCombo.SetItems(ToComboItems(modelNames));
    defaultsPage_->translateCombo.SetItems(ToComboItems(modelNames));
    for (std::size_t i = 0; i < modelNames.size(); ++i) {
        if (modelNames[i] == config_.defaultOcrModel) defaultsPage_->ocrCombo.SetSelection(static_cast<int>(i), false);
        if (modelNames[i] == config_.defaultTranslateModel) defaultsPage_->translateCombo.SetSelection(static_cast<int>(i), false);
    }
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
}

void SettingsWindow::RefreshHotkeysPage() {
    hotkeyPage_->ocrValue.SetText((captureTarget_ == CaptureTarget::Ocr) ? LoadS(IDS_HOTKEY_WAITING) : HotkeyToText(config_.ocrHotkey));
    hotkeyPage_->translateValue.SetText((captureTarget_ == CaptureTarget::Translate) ? LoadS(IDS_HOTKEY_WAITING) : HotkeyToText(config_.translateHotkey));
}

void SettingsWindow::CommitCurrentProvider() {
    auto* provider = CurrentProvider();
    if (!provider) return;
    provider->name = providerPage_->nameEdit.GetText();
    provider->apiKey = providerPage_->keyEdit.GetText();
    provider->baseUrl = providerPage_->baseEdit.GetText();
    provider->apiPath = providerPage_->pathEdit.GetText();
}

void SettingsWindow::SaveConfig() {
    CommitCurrentProvider();
    std::wstring error;
    if (!store_.Save(config_, error)) {
        SetStatus(error);
        return;
    }
    SetStatus(LoadS(IDS_STATUS_SETTINGS_SAVED));
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
    provider.models.push_back({L"gpt-4.1-mini", true});
    config_.providers.push_back(provider);
    providerSelection_ = static_cast<int>(config_.providers.size() - 1);
    RefreshProviderTable();
    RefreshProviderDetails();
    RefreshModelList();
    RefreshDefaultsPage();
    SetStatus(LoadS(IDS_STATUS_PROVIDER_ADDED));
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
