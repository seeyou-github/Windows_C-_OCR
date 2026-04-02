#pragma once

#include "AppConfig.h"
#include "OcrService.h"
#include "SettingsWindow.h"

#include "darkui/darkui.h"

#include <string>

namespace app {

class MainWindow {
public:
    MainWindow();
    ~MainWindow();

    bool Create(HINSTANCE instance);

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    bool OnCreate();
    void OnDestroy();
    void OnSize();
    void OnCommand(WPARAM wParam, LPARAM lParam);
    void OnHotkey(UINT id);

    void Layout();
    void ApplyConfigTheme();
    void RefreshModelCombo();
    void RefreshResultText(const std::wstring& text);
    void SetStatus(const std::wstring& text);
    void ShowFromTray();
    void HideToTray();
    void CenterWindow();
    void AddTrayIcon();
    void RemoveTrayIcon();
    void ShowTrayMenu();
    void OpenImage();
    void RunOcr();
    void TranslateResult();
    void CopyResult();
    void OpenSettings();
    void RegisterAppHotkeys();
    const ProviderConfig* DefaultProvider() const;

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    ConfigStore configStore_;
    AppConfig config_;
    OcrService service_;
    SettingsWindow settingsWindow_;
    darkui::ThemedWindowHost host_;
    darkui::Button openButton_;
    darkui::Button translateButton_;
    darkui::Button copyButton_;
    darkui::Button settingsButton_;
    darkui::ComboBox modelCombo_;
    darkui::Edit resultEdit_;
    std::wstring selectedImagePath_;
    std::wstring statusText_;
    bool trayAdded_ = false;
};

}  // namespace app
