#pragma once

#include "AppConfig.h"
#include "OcrService.h"
#include "SettingsWindow.h"

#include "darkui/darkui.h"

#include <atomic>
#include <string>
#include <windows.h>

namespace app {

class MainWindow {
public:
    MainWindow();
    ~MainWindow();

    bool Create(HINSTANCE instance);
    static constexpr wchar_t kWindowClassName[] = L"Win32OcrMainWindow";

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    bool OnCreate();
    void OnDestroy();
    void OnSize();
    void SaveWindowSizeIfNeeded();
    void OnCommand(WPARAM wParam, LPARAM lParam);
    void OnHotkey(UINT id);

    void Layout();
    void ApplyConfigTheme();
    void RefreshModelCombo();
    std::wstring ApplyResultFilter(const std::wstring& text, const std::wstring& filterExpression) const;
    std::wstring FilterOcrResult(const std::wstring& text) const;
    std::wstring FilterTranslateResult(const std::wstring& text) const;
    void RefreshResultText(const std::wstring& text);
    void RefreshTranslateText(const std::wstring& text);
    void AppendResultText(const std::wstring& text);
    void AppendTranslateText(const std::wstring& text);
    void UpdateTextEditScrollBars();
    void UpdateEditVerticalScrollBar(darkui::Edit& edit, bool visible);
    bool CopyTextToClipboard(const std::wstring& text, bool updateStatus);
    void ShowRecognitionToast();
    void SetStatus(const std::wstring& text);
    void ShowFromTray();
    void HideToTray();
    void ToggleTrayWindow();
    void CenterWindow();
    void AddTrayIcon();
    void RemoveTrayIcon();
    void ShowTrayMenu();
    void OpenImage();
    void RunOcr();
    void RunHotkeyCapture(bool translateAfterOcr);
    void RunSelectedTextTranslate();
    void RunTranslateImage();
    void TranslateResult();
    void BeginAsyncRequest(bool translateOnly, const std::wstring& inputText, const std::wstring& imagePath);
    bool CaptureSelectionToDataUrl(std::string& imageDataUrl, bool& canceled);
    void CopyResult();
    void OpenSettings();
    void RegisterAppHotkeys();
    void ShowOcrFailureDialog(const ServiceResult& result);
    void ShowServiceFailureDialog(const std::wstring& title, const ServiceResult& result);
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
    darkui::Edit translateEdit_;
    struct MainModelChoice {
        std::wstring providerId;
        std::wstring modelId;
    };
    std::vector<MainModelChoice> modelChoices_;
    std::wstring selectedImagePath_;
    std::string selectedImageDataUrl_;
    std::wstring statusText_;
    bool translationVisible_ = false;
    bool trayAdded_ = false;
    bool requestInFlight_ = false;
    bool activeTranslateRequest_ = false;
    bool activeRequestStream_ = false;
    bool pendingTranslateAfterOcr_ = false;
    bool pendingHotkeySilentOcr_ = false;
    bool activeHotkeySilentOcr_ = false;
    std::atomic<unsigned long long> requestCounter_{0};
    unsigned long long activeRequestId_ = 0;
    HHOOK hotkeyHook_ = nullptr;
    ULONGLONG lastHotkeyTick_ = 0;
    ULONGLONG lastTrayToggleTick_ = 0;
    bool windowSizeDirty_ = false;
    HICON appIconLarge_ = nullptr;
    HICON appIconSmall_ = nullptr;
};

}  // namespace app
