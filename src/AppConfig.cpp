#include "AppConfig.h"

#include <windows.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace app {

namespace {

std::wstring Trim(const std::wstring& input) {
    std::size_t begin = 0;
    while (begin < input.size() && iswspace(input[begin])) {
        ++begin;
    }
    std::size_t end = input.size();
    while (end > begin && iswspace(input[end - 1])) {
        --end;
    }
    return input.substr(begin, end - begin);
}

std::wstring GetExeDirectory() {
    wchar_t modulePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, modulePath, static_cast<DWORD>(std::size(modulePath)));
    std::wstring path = modulePath;
    const std::size_t slash = path.find_last_of(L"\\/");
    return (slash == std::wstring::npos) ? L"." : path.substr(0, slash);
}

std::wstring JoinLines(const std::vector<std::wstring>& lines) {
    std::wstring output;
    for (const auto& line : lines) {
        output += line;
        output += L"\r\n";
    }
    return output;
}

std::wstring BoolToText(bool value) {
    return value ? L"1" : L"0";
}

bool TextToBool(const std::wstring& value, bool fallback) {
    if (value == L"1" || value == L"true" || value == L"TRUE") {
        return true;
    }
    if (value == L"0" || value == L"false" || value == L"FALSE") {
        return false;
    }
    return fallback;
}

int TextToInt(const std::wstring& value, int fallback) {
    if (value.empty()) {
        return fallback;
    }
    return _wtoi(value.c_str());
}

ProviderConfig* FindProviderById(AppConfig& config, const std::wstring& id) {
    for (auto& provider : config.providers) {
        if (provider.id == id) {
            return &provider;
        }
    }
    return nullptr;
}

std::wstring MakeIniSafe(const std::wstring& text) {
    std::wstring output = text;
    std::replace(output.begin(), output.end(), L'\r', L' ');
    std::replace(output.begin(), output.end(), L'\n', L' ');
    return output;
}

}  // namespace

AppConfig ConfigStore::CreateDefaultConfig() {
    AppConfig config;

    ProviderConfig silicon;
    silicon.id = L"siliconflow";
    silicon.name = L"SiliconFlow";
    silicon.baseUrl = L"https://api.siliconflow.cn";
    silicon.apiPath = L"/v1/chat/completions";
    silicon.enabled = true;
    silicon.builtIn = true;
    silicon.models = {
        {L"zai-org/GLM-4.5V", true},
        {L"Qwen/Qwen2.5-VL-72B-Instruct", true},
        {L"Qwen/Qwen2.5-72B-Instruct", true}
    };

    ProviderConfig zhipu;
    zhipu.id = L"zhipu";
    zhipu.name = L"Zhipu";
    zhipu.baseUrl = L"https://open.bigmodel.cn";
    zhipu.apiPath = L"/api/paas/v4/chat/completions";
    zhipu.enabled = true;
    zhipu.builtIn = true;
    zhipu.models = {
        {L"glm-4.1v-thinking-flash", true},
        {L"glm-4.5-air", true},
        {L"glm-4-air", true}
    };

    config.providers = {silicon, zhipu};
    config.defaultProviderId = silicon.id;
    config.defaultOcrModel = silicon.models.front().id;
    config.defaultTranslateModel = silicon.models.back().id;
    config.themeName = L"graphite";
    config.fontSize = 18;
    config.ocrHotkey = {MOD_CONTROL | MOD_SHIFT, 'O'};
    config.translateHotkey = {MOD_CONTROL | MOD_SHIFT, 'T'};
    return config;
}

std::wstring ConfigStore::ConfigPath() const {
    return GetExeDirectory() + L"\\Win32OCR.ini";
}

bool ConfigStore::Load(AppConfig& config, std::wstring& error) const {
    config = CreateDefaultConfig();
    std::wifstream input{std::filesystem::path(ConfigPath())};
    if (!input.is_open()) {
        return true;
    }

    std::wstring line;
    std::wstring currentSection;
    ProviderConfig* currentProvider = nullptr;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == L'\r') {
            line.pop_back();
        }
        line = Trim(line);
        if (line.empty() || line[0] == L'#' || line[0] == L';') {
            continue;
        }
        if (line.front() == L'[' && line.back() == L']') {
            currentSection = line.substr(1, line.size() - 2);
            currentProvider = nullptr;
            if (currentSection.rfind(L"provider:", 0) == 0) {
                const std::wstring providerId = currentSection.substr(9);
                currentProvider = FindProviderById(config, providerId);
                if (!currentProvider) {
                    ProviderConfig provider;
                    provider.id = providerId;
                    provider.name = providerId;
                    config.providers.push_back(provider);
                    currentProvider = &config.providers.back();
                }
            }
            continue;
        }

        const std::size_t equals = line.find(L'=');
        if (equals == std::wstring::npos) {
            continue;
        }

        const std::wstring key = Trim(line.substr(0, equals));
        const std::wstring value = Trim(line.substr(equals + 1));

        if (currentSection == L"ui") {
            if (key == L"default_provider") config.defaultProviderId = value;
            else if (key == L"default_ocr_model") config.defaultOcrModel = value;
            else if (key == L"default_translate_model") config.defaultTranslateModel = value;
            else if (key == L"theme") config.themeName = value;
            else if (key == L"font_size") config.fontSize = TextToInt(value, config.fontSize);
            else if (key == L"ocr_hotkey_mod") config.ocrHotkey.modifiers = static_cast<UINT>(TextToInt(value, config.ocrHotkey.modifiers));
            else if (key == L"ocr_hotkey_vk") config.ocrHotkey.virtualKey = static_cast<UINT>(TextToInt(value, config.ocrHotkey.virtualKey));
            else if (key == L"translate_hotkey_mod") config.translateHotkey.modifiers = static_cast<UINT>(TextToInt(value, config.translateHotkey.modifiers));
            else if (key == L"translate_hotkey_vk") config.translateHotkey.virtualKey = static_cast<UINT>(TextToInt(value, config.translateHotkey.virtualKey));
        } else if (currentProvider) {
            if (key == L"name") currentProvider->name = value;
            else if (key == L"api_key") currentProvider->apiKey = value;
            else if (key == L"base_url") currentProvider->baseUrl = value;
            else if (key == L"api_path") currentProvider->apiPath = value;
            else if (key == L"enabled") currentProvider->enabled = TextToBool(value, currentProvider->enabled);
            else if (key == L"builtin") currentProvider->builtIn = TextToBool(value, currentProvider->builtIn);
            else if (key == L"model") {
                const std::size_t pipe = value.find(L'|');
                ProviderModel model;
                if (pipe == std::wstring::npos) {
                    model.id = value;
                    model.enabled = true;
                } else {
                    model.id = value.substr(0, pipe);
                    model.enabled = TextToBool(value.substr(pipe + 1), true);
                }
                if (!model.id.empty()) {
                    currentProvider->models.push_back(model);
                }
            }
        }
    }

    for (auto& provider : config.providers) {
        if (provider.models.empty()) {
            provider.models.push_back({L"gpt-4.1-mini", true});
        }
    }
    if (config.defaultProviderId.empty() && !config.providers.empty()) {
        config.defaultProviderId = config.providers.front().id;
    }
    if (config.defaultOcrModel.empty() && !config.providers.empty()) {
        config.defaultOcrModel = config.providers.front().models.front().id;
    }
    if (config.defaultTranslateModel.empty() && !config.providers.empty()) {
        config.defaultTranslateModel = config.providers.front().models.front().id;
    }

    error.clear();
    return true;
}

bool ConfigStore::Save(const AppConfig& config, std::wstring& error) const {
    std::vector<std::wstring> lines;
    lines.push_back(L"[ui]");
    lines.push_back(L"default_provider=" + MakeIniSafe(config.defaultProviderId));
    lines.push_back(L"default_ocr_model=" + MakeIniSafe(config.defaultOcrModel));
    lines.push_back(L"default_translate_model=" + MakeIniSafe(config.defaultTranslateModel));
    lines.push_back(L"theme=" + MakeIniSafe(config.themeName));
    lines.push_back(L"font_size=" + std::to_wstring(config.fontSize));
    lines.push_back(L"ocr_hotkey_mod=" + std::to_wstring(config.ocrHotkey.modifiers));
    lines.push_back(L"ocr_hotkey_vk=" + std::to_wstring(config.ocrHotkey.virtualKey));
    lines.push_back(L"translate_hotkey_mod=" + std::to_wstring(config.translateHotkey.modifiers));
    lines.push_back(L"translate_hotkey_vk=" + std::to_wstring(config.translateHotkey.virtualKey));
    lines.push_back(L"");

    for (const auto& provider : config.providers) {
        lines.push_back(L"[provider:" + MakeIniSafe(provider.id) + L"]");
        lines.push_back(L"name=" + MakeIniSafe(provider.name));
        lines.push_back(L"api_key=" + MakeIniSafe(provider.apiKey));
        lines.push_back(L"base_url=" + MakeIniSafe(provider.baseUrl));
        lines.push_back(L"api_path=" + MakeIniSafe(provider.apiPath));
        lines.push_back(L"enabled=" + BoolToText(provider.enabled));
        lines.push_back(L"builtin=" + BoolToText(provider.builtIn));
        for (const auto& model : provider.models) {
            lines.push_back(L"model=" + MakeIniSafe(model.id) + L"|" + BoolToText(model.enabled));
        }
        lines.push_back(L"");
    }

    const std::wstring newContent = JoinLines(lines);
    std::wstring oldContent;
    {
        std::wifstream existing{std::filesystem::path(ConfigPath())};
        if (existing.is_open()) {
            std::wstringstream buffer;
            buffer << existing.rdbuf();
            oldContent = buffer.str();
        }
    }
    if (oldContent == newContent) {
        error.clear();
        return true;
    }

    std::wofstream output(std::filesystem::path(ConfigPath()), std::ios::trunc);
    if (!output.is_open()) {
        error = L"Failed to open config file.";
        return false;
    }
    output << newContent;
    if (!output.good()) {
        error = L"Failed to write config file.";
        return false;
    }
    error.clear();
    return true;
}

std::wstring HotkeyToText(const HotkeyConfig& hotkey) {
    if (hotkey.virtualKey == 0) {
        return L"None";
    }

    std::wstring text;
    if (hotkey.modifiers & MOD_CONTROL) text += L"Ctrl+";
    if (hotkey.modifiers & MOD_SHIFT) text += L"Shift+";
    if (hotkey.modifiers & MOD_ALT) text += L"Alt+";
    if (hotkey.modifiers & MOD_WIN) text += L"Win+";

    wchar_t keyName[64] = {};
    const UINT scanCode = MapVirtualKeyW(hotkey.virtualKey, MAPVK_VK_TO_VSC) << 16;
    if (GetKeyNameTextW(static_cast<LONG>(scanCode), keyName, static_cast<int>(std::size(keyName))) > 0) {
        text += keyName;
    } else {
        text += std::to_wstring(hotkey.virtualKey);
    }
    return text;
}

}  // namespace app
