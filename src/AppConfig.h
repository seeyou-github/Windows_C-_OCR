#pragma once

#include <windows.h>

#include <string>
#include <vector>

namespace app {

struct ProviderModel {
    std::wstring id;
    std::wstring displayName;
    bool enabled = true;
    bool custom = false;
    bool reasoning = false;
};

struct ProviderConfig {
    std::wstring id;
    std::wstring name;
    std::wstring apiKey;
    std::wstring baseUrl;
    std::wstring apiPath;
    bool enabled = true;
    bool builtIn = false;
    bool streamResponse = false;
    std::vector<ProviderModel> models;
};

struct HotkeyConfig {
    UINT modifiers = 0;
    UINT virtualKey = 0;
};

struct AppConfig {
    std::vector<ProviderConfig> providers;
    std::wstring defaultProviderId;
    std::wstring defaultOcrProviderId;
    std::wstring defaultTranslateProviderId;
    std::wstring defaultOcrModel;
    std::wstring defaultTranslateModel;
    std::wstring ocrPrompt;
    std::wstring translateTextPrompt;
    std::wstring ocrResultFilter;
    std::wstring translateResultFilter;
    bool startInTray = true;
    bool copyAfterHotkeyOcr = false;
    std::wstring themeName = L"graphite";
    int fontSize = 18;
    int mainWindowWidth = 860;
    int mainWindowHeight = 860;
    HotkeyConfig ocrHotkey{};
    HotkeyConfig translateHotkey{};
};

class ConfigStore {
public:
    bool Load(AppConfig& config, std::wstring& error) const;
    bool Save(const AppConfig& config, std::wstring& error) const;
    std::wstring ConfigPath() const;

    static AppConfig CreateDefaultConfig();
};

std::wstring HotkeyToText(const HotkeyConfig& hotkey);

}  // namespace app
