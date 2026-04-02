#pragma once

#include "AppConfig.h"
#include "OcrService.h"

#include "darkui/darkui.h"

#include <functional>
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
    struct ProviderPage;
    struct DefaultsPage;
    struct DisplayPage;
    struct HotkeyPage;

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
    void CommitCurrentProvider();
    void SaveConfig();
    void AddCustomProvider();
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
    enum class CaptureTarget { None, Ocr, Translate } captureTarget_ = CaptureTarget::None;

    ProviderPage* providerPage_ = nullptr;
    DefaultsPage* defaultsPage_ = nullptr;
    DisplayPage* displayPage_ = nullptr;
    HotkeyPage* hotkeyPage_ = nullptr;
};

}  // namespace app
