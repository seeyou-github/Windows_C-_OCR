#pragma once

#include "AppConfig.h"
#include "OcrService.h"

#include "darkui/darkui.h"

#include <functional>
#include <memory>
#include <string>

namespace app {

class SettingsWindow {
public:
    SettingsWindow(AppConfig& config, const ConfigStore& store, const OcrService& service, std::function<void()> onConfigSaved);
    ~SettingsWindow();

    bool Create(HINSTANCE instance, HWND owner);
    void Show();
    void Close();
    bool IsOpen() const;

private:
    struct DefaultModelChoice {
        std::wstring providerId;
        std::wstring modelId;
    };
    enum class CaptureTarget { None, Ocr, Translate };
    struct ProviderPage;
    struct DefaultsPage;
    struct DisplayPage;
    struct HotkeyPage;
    struct TestDialogSession {
        darkui::ThemeManager themeManager;
        darkui::Dialog dialog;
        darkui::Panel formPanel;
        darkui::Static promptLabel;
        darkui::Edit promptEdit;
        darkui::Button sendButton;
        darkui::Static responseLabel;
        darkui::Edit responseEdit;
        std::wstring modelId;
        bool busy = false;
        unsigned long long requestId = 0;
    };

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    bool OnCreate(HWND hwnd);
    void OnDestroy();
    void OnSize();
    void OnCommand(WPARAM wParam, LPARAM lParam);
    void OnNotify(LPARAM lParam);
    void OnKeyDown(UINT virtualKey);

    void Layout();
    void PositionWindow();
    void RefreshAll();
    void RefreshProviderTable();
    void RefreshProviderDetails();
    void RefreshModelList();
    void RefreshDefaultsPage();
    void RefreshDisplayPage();
    void RefreshHotkeysPage();
    void RefreshModelActions();
    void ApplyCurrentTheme();
    void StartHotkeyCapture(CaptureTarget target);
    void StopHotkeyCapture();
    void ToggleCurrentProviderEnabled();
    void DeleteCurrentProvider();
    void CommitCurrentProvider();
    void SaveConfig();
    void AddCustomProvider();
    void AddCustomModel();
    void EditSelectedModel();
    bool ShowModelDialog(std::wstring& modelId, std::wstring& modelName, bool& modelReasoning, bool editing);
    void OpenModelTestDialog();
    void StartModelTestRequest();
    int SelectedModelIndex() const;
    std::wstring SelectedModelGroup() const;
    void SetStatus(const std::wstring& text);
    ProviderConfig* CurrentProvider();
    const ProviderConfig* CurrentProvider() const;

    HWND hwnd_ = nullptr;
    HWND owner_ = nullptr;
    HINSTANCE instance_ = nullptr;
    AppConfig& config_;
    const ConfigStore& store_;
    const OcrService& service_;
    std::function<void()> onConfigSaved_;
    darkui::ThemedWindowHost host_;
    darkui::Tab tab_;
    darkui::Button saveButton_;
    darkui::Button closeButton_;
    std::wstring statusText_;
    int providerSelection_ = 0;
    std::vector<std::wstring> collapsedModelGroups_;
    CaptureTarget captureTarget_ = CaptureTarget::None;

    ProviderPage* providerPage_ = nullptr;
    DefaultsPage* defaultsPage_ = nullptr;
    DisplayPage* displayPage_ = nullptr;
    HotkeyPage* hotkeyPage_ = nullptr;
    std::unique_ptr<TestDialogSession> testDialog_;
    unsigned long long testRequestId_ = 0;
    HHOOK keyboardHook_ = nullptr;
    bool firstShowPending_ = true;
    HICON appIconLarge_ = nullptr;
    HICON appIconSmall_ = nullptr;
};

}  // namespace app
