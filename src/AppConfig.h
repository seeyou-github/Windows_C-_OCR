#pragma once

#include <windows.h>

#include <string>
#include <vector>

namespace app {

struct ProviderModel {
    std::wstring id;
    bool enabled = true;
};

struct ProviderConfig {
    std::wstring id;
    std::wstring name;
    std::wstring apiKey;
    std::wstring baseUrl;
    std::wstring apiPath;
    bool enabled = true;
    bool builtIn = false;
    std::vector<ProviderModel> models;
};

struct HotkeyConfig {
    UINT modifiers = 0;
    UINT virtualKey = 0;
};

struct AppConfig {
    std::vector<ProviderConfig> providers;
    std::wstring defaultProviderId;
    std::wstring defaultOcrModel;
    std::wstring defaultTranslateModel;
    std::wstring themeName = L"graphite";
    int fontSize = 18;
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
