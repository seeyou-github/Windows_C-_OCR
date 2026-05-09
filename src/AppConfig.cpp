#include "AppConfig.h"

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace app {

namespace {

constexpr wchar_t kDefaultOcrPrompt[] =
    L"Perform OCR on this image and return only the recognized text. Preserve line breaks. "
    L"If no readable text exists, reply with No readable text.";

constexpr wchar_t kDefaultTranslateTextPrompt[] =
    L"Translate the following text into Chinese and return only the translated text:";

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

std::wstring DecodeIniToken(const std::wstring& text) {
    std::wstring output;
    output.reserve(text.size());
    bool escaping = false;
    for (wchar_t ch : text) {
        if (escaping) {
            switch (ch) {
            case L'n':
                output += L"\r\n";
                break;
            case L'r':
                break;
            case L'\\':
            case L'|':
            case L'[':
            case L']':
                output.push_back(ch);
                break;
            default:
                output.push_back(ch);
                break;
            }
            escaping = false;
            continue;
        }
        if (ch == L'\\') {
            escaping = true;
            continue;
        }
        output.push_back(ch);
    }
    if (escaping) {
        output.push_back(L'\\');
    }
    return output;
}

std::wstring EncodeIniToken(const std::wstring& text) {
    std::wstring output;
    output.reserve(text.size());
    for (wchar_t ch : text) {
        switch (ch) {
        case L'\\':
            output += L"\\\\";
            break;
        case L'|':
            output += L"\\|";
            break;
        case L'[':
            output += L"\\[";
            break;
        case L']':
            output += L"\\]";
            break;
        case L'\r':
            break;
        case L'\n':
            output += L"\\n";
            break;
        default:
            output.push_back(ch);
            break;
        }
    }
    return output;
}

std::wstring EncodeIniMultiline(const std::wstring& text) {
    std::wstring output;
    output.reserve(text.size());
    for (wchar_t ch : text) {
        switch (ch) {
        case L'\\':
            output += L"\\\\";
            break;
        case L'\r':
            break;
        case L'\n':
            output += L"\\n";
            break;
        default:
            output.push_back(ch);
            break;
        }
    }
    return output;
}

std::wstring DecodeIniMultiline(const std::wstring& text) {
    std::wstring output;
    output.reserve(text.size());
    bool escaping = false;
    for (wchar_t ch : text) {
        if (escaping) {
            switch (ch) {
            case L'n':
                output += L"\r\n";
                break;
            case L'\\':
                output.push_back(L'\\');
                break;
            default:
                output.push_back(L'\\');
                output.push_back(ch);
                break;
            }
            escaping = false;
            continue;
        }
        if (ch == L'\\') {
            escaping = true;
            continue;
        }
        output.push_back(ch);
    }
    if (escaping) {
        output.push_back(L'\\');
    }
    return output;
}

std::vector<std::wstring> SplitEscaped(const std::wstring& text, wchar_t delimiter) {
    std::vector<std::wstring> parts;
    std::wstring current;
    bool escaping = false;
    for (wchar_t ch : text) {
        if (escaping) {
            current.push_back(L'\\');
            current.push_back(ch);
            escaping = false;
            continue;
        }
        if (ch == L'\\') {
            escaping = true;
            continue;
        }
        if (ch == delimiter) {
            parts.push_back(current);
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    if (escaping) {
        current.push_back(L'\\');
    }
    parts.push_back(current);
    return parts;
}

bool IsValidUtf8(const std::string& bytes) {
    if (bytes.empty()) {
        return true;
    }
    const int needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
    return needed > 0;
}

std::wstring DecodeBytes(const std::string& bytes, UINT codePage) {
    if (bytes.empty()) {
        return L"";
    }
    const int needed = MultiByteToWideChar(codePage, 0, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
    if (needed <= 0) {
        return L"";
    }
    std::wstring output(static_cast<std::size_t>(needed), L'\0');
    MultiByteToWideChar(codePage, 0, bytes.data(), static_cast<int>(bytes.size()), output.data(), needed);
    return output;
}

std::string EncodeUtf8(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }
    const int needed = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (needed <= 0) {
        return {};
    }
    std::string output(static_cast<std::size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), output.data(), needed, nullptr, nullptr);
    return output;
}

bool ReadTextFile(const std::wstring& path, std::wstring& content) {
    std::ifstream input(std::filesystem::path(path), std::ios::binary);
    if (!input.is_open()) {
        return false;
    }
    std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (bytes.size() >= 3 &&
        static_cast<unsigned char>(bytes[0]) == 0xEF &&
        static_cast<unsigned char>(bytes[1]) == 0xBB &&
        static_cast<unsigned char>(bytes[2]) == 0xBF) {
        content = DecodeBytes(bytes.substr(3), CP_UTF8);
        return true;
    }
    if (bytes.size() >= 2 &&
        static_cast<unsigned char>(bytes[0]) == 0xFF &&
        static_cast<unsigned char>(bytes[1]) == 0xFE) {
        const wchar_t* wide = reinterpret_cast<const wchar_t*>(bytes.data() + 2);
        const std::size_t count = (bytes.size() - 2) / sizeof(wchar_t);
        content.assign(wide, wide + count);
        return true;
    }
    if (IsValidUtf8(bytes)) {
        content = DecodeBytes(bytes, CP_UTF8);
    } else {
        content = DecodeBytes(bytes, CP_ACP);
    }
    return true;
}

bool WriteTextFileUtf8(const std::wstring& path, const std::wstring& content) {
    const std::string utf8 = EncodeUtf8(content);
    std::ofstream output(std::filesystem::path(path), std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }
    const unsigned char bom[] = {0xEF, 0xBB, 0xBF};
    output.write(reinterpret_cast<const char*>(bom), sizeof(bom));
    if (!utf8.empty()) {
        output.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
    }
    return output.good();
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
    silicon.streamResponse = false;

    ProviderConfig zhipu;
    zhipu.id = L"zhipu";
    zhipu.name = L"Zhipu";
    zhipu.baseUrl = L"https://open.bigmodel.cn";
    zhipu.apiPath = L"/api/paas/v4/chat/completions";
    zhipu.enabled = true;
    zhipu.builtIn = true;
    zhipu.streamResponse = false;

    config.providers = {silicon, zhipu};
    config.defaultProviderId = silicon.id;
    config.defaultOcrProviderId = silicon.id;
    config.defaultTranslateProviderId = silicon.id;
    config.defaultOcrModel.clear();
    config.defaultTranslateModel.clear();
    config.ocrPrompt = kDefaultOcrPrompt;
    config.translateTextPrompt = kDefaultTranslateTextPrompt;
    config.ocrResultFilter.clear();
    config.translateResultFilter.clear();
    config.ocrTimeoutSeconds = 6;
    config.startInTray = true;
    config.copyAfterHotkeyOcr = false;
    config.themeName = L"graphite";
    config.fontSize = 18;
    config.mainWindowWidth = 860;
    config.mainWindowHeight = 860;
    config.ocrHotkey = {MOD_CONTROL | MOD_SHIFT, 'O'};
    config.translateHotkey = {MOD_CONTROL | MOD_SHIFT, 'T'};
    return config;
}

std::wstring ConfigStore::ConfigPath() const {
    return GetExeDirectory() + L"\\Win32OCR.ini";
}

bool ConfigStore::Load(AppConfig& config, std::wstring& error) const {
    config = CreateDefaultConfig();
    std::wstring fileContent;
    if (!ReadTextFile(ConfigPath(), fileContent)) {
        return true;
    }

    std::wstringstream input(fileContent);
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
                const std::wstring providerId = DecodeIniToken(currentSection.substr(9));
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
            else if (key == L"default_ocr_provider") config.defaultOcrProviderId = value;
            else if (key == L"default_translate_provider") config.defaultTranslateProviderId = value;
            else if (key == L"default_ocr_model") config.defaultOcrModel = value;
            else if (key == L"default_translate_model") config.defaultTranslateModel = value;
            else if (key == L"ocr_prompt") config.ocrPrompt = DecodeIniMultiline(value);
            else if (key == L"translate_text_prompt") config.translateTextPrompt = DecodeIniMultiline(value);
            else if (key == L"ocr_result_filter") config.ocrResultFilter = DecodeIniMultiline(value);
            else if (key == L"translate_result_filter") config.translateResultFilter = DecodeIniMultiline(value);
            else if (key == L"ocr_timeout_seconds") config.ocrTimeoutSeconds = std::clamp(TextToInt(value, config.ocrTimeoutSeconds), 1, 300);
            else if (key == L"start_in_tray") config.startInTray = TextToBool(value, config.startInTray);
            else if (key == L"copy_after_hotkey_ocr") config.copyAfterHotkeyOcr = TextToBool(value, config.copyAfterHotkeyOcr);
            else if (key == L"theme") config.themeName = value;
            else if (key == L"font_size") config.fontSize = TextToInt(value, config.fontSize);
            else if (key == L"main_window_width") config.mainWindowWidth = std::max(480, TextToInt(value, config.mainWindowWidth));
            else if (key == L"main_window_height") config.mainWindowHeight = std::max(360, TextToInt(value, config.mainWindowHeight));
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
            else if (key == L"stream_response") currentProvider->streamResponse = TextToBool(value, currentProvider->streamResponse);
            else if (key == L"enable_reasoning") {
                // Keep backward compatibility with legacy provider-level field.
            }
            else if (key == L"model") {
                ProviderModel model;
                const std::vector<std::wstring> parts = SplitEscaped(value, L'|');
                if (parts.empty()) {
                    continue;
                }
                model.id = DecodeIniToken(parts[0]);
                if (parts.size() == 1) {
                    model.enabled = true;
                } else {
                    model.enabled = TextToBool(parts[1], true);
                    if (parts.size() >= 3) model.displayName = DecodeIniToken(parts[2]);
                    if (parts.size() >= 4) model.custom = TextToBool(parts[3], false);
                    if (parts.size() >= 5) model.reasoning = TextToBool(parts[4], false);
                }
                if (!model.id.empty()) {
                    currentProvider->models.push_back(model);
                }
            }
        }
    }

    const auto firstEnabledProviderId = [&]() -> std::wstring {
        for (const auto& provider : config.providers) {
            if (provider.enabled) {
                return provider.id;
            }
        }
        return config.providers.empty() ? L"" : config.providers.front().id;
    };
    const auto isEnabledProvider = [&](const std::wstring& providerId) {
        for (const auto& provider : config.providers) {
            if (provider.id == providerId) {
                return provider.enabled;
            }
        }
        return false;
    };
    if (config.defaultProviderId.empty() || !isEnabledProvider(config.defaultProviderId)) {
        config.defaultProviderId = firstEnabledProviderId();
    }
    if (config.defaultOcrProviderId.empty()) {
        config.defaultOcrProviderId = config.defaultProviderId;
    }
    if (config.defaultTranslateProviderId.empty()) {
        config.defaultTranslateProviderId = config.defaultProviderId;
    }
    if (Trim(config.ocrPrompt).empty()) {
        config.ocrPrompt = kDefaultOcrPrompt;
    }
    if (Trim(config.translateTextPrompt).empty()) {
        config.translateTextPrompt = kDefaultTranslateTextPrompt;
    }

    const auto validateDefaultModel = [&](std::wstring& providerId, std::wstring& modelId) {
        if (providerId.empty()) {
            modelId.clear();
            return;
        }
        const ProviderConfig* provider = nullptr;
        for (const auto& item : config.providers) {
            if (item.enabled && item.id == providerId) {
                provider = &item;
                break;
            }
        }
        if (provider) {
            const auto hasModel = [&](const std::wstring& modelId) {
                for (const auto& model : provider->models) {
                    if (model.enabled && model.id == modelId) {
                        return true;
                    }
                }
                return false;
            };
            if (!hasModel(modelId)) {
                modelId.clear();
            }
        } else {
            providerId.clear();
            modelId.clear();
        }
    };
    validateDefaultModel(config.defaultOcrProviderId, config.defaultOcrModel);
    validateDefaultModel(config.defaultTranslateProviderId, config.defaultTranslateModel);
    if (config.defaultProviderId.empty()) {
        config.defaultProviderId = config.defaultOcrProviderId.empty() ? config.defaultTranslateProviderId : config.defaultOcrProviderId;
    }

    error.clear();
    return true;
}

bool ConfigStore::Save(const AppConfig& config, std::wstring& error) const {
    std::vector<std::wstring> lines;
    lines.push_back(L"[ui]");
    lines.push_back(L"default_provider=" + MakeIniSafe(config.defaultProviderId));
    lines.push_back(L"default_ocr_provider=" + MakeIniSafe(config.defaultOcrProviderId));
    lines.push_back(L"default_translate_provider=" + MakeIniSafe(config.defaultTranslateProviderId));
    lines.push_back(L"default_ocr_model=" + MakeIniSafe(config.defaultOcrModel));
    lines.push_back(L"default_translate_model=" + MakeIniSafe(config.defaultTranslateModel));
    lines.push_back(L"ocr_prompt=" + EncodeIniMultiline(config.ocrPrompt));
    lines.push_back(L"translate_text_prompt=" + EncodeIniMultiline(config.translateTextPrompt));
    lines.push_back(L"ocr_result_filter=" + EncodeIniMultiline(config.ocrResultFilter));
    lines.push_back(L"translate_result_filter=" + EncodeIniMultiline(config.translateResultFilter));
    lines.push_back(L"ocr_timeout_seconds=" + std::to_wstring(std::clamp(config.ocrTimeoutSeconds, 1, 300)));
    lines.push_back(L"start_in_tray=" + BoolToText(config.startInTray));
    lines.push_back(L"copy_after_hotkey_ocr=" + BoolToText(config.copyAfterHotkeyOcr));
    lines.push_back(L"theme=" + MakeIniSafe(config.themeName));
    lines.push_back(L"font_size=" + std::to_wstring(config.fontSize));
    lines.push_back(L"main_window_width=" + std::to_wstring(config.mainWindowWidth));
    lines.push_back(L"main_window_height=" + std::to_wstring(config.mainWindowHeight));
    lines.push_back(L"ocr_hotkey_mod=" + std::to_wstring(config.ocrHotkey.modifiers));
    lines.push_back(L"ocr_hotkey_vk=" + std::to_wstring(config.ocrHotkey.virtualKey));
    lines.push_back(L"translate_hotkey_mod=" + std::to_wstring(config.translateHotkey.modifiers));
    lines.push_back(L"translate_hotkey_vk=" + std::to_wstring(config.translateHotkey.virtualKey));
    lines.push_back(L"");

    for (const auto& provider : config.providers) {
        lines.push_back(L"[provider:" + EncodeIniToken(provider.id) + L"]");
        lines.push_back(L"name=" + MakeIniSafe(provider.name));
        lines.push_back(L"api_key=" + MakeIniSafe(provider.apiKey));
        lines.push_back(L"base_url=" + MakeIniSafe(provider.baseUrl));
        lines.push_back(L"api_path=" + MakeIniSafe(provider.apiPath));
        lines.push_back(L"enabled=" + BoolToText(provider.enabled));
        lines.push_back(L"builtin=" + BoolToText(provider.builtIn));
        lines.push_back(L"stream_response=" + BoolToText(provider.streamResponse));
        for (const auto& model : provider.models) {
            lines.push_back(L"model=" + EncodeIniToken(model.id) + L"|" + BoolToText(model.enabled) + L"|" + EncodeIniToken(model.displayName) + L"|" +
                            BoolToText(model.custom) + L"|" + BoolToText(model.reasoning));
        }
        lines.push_back(L"");
    }

    const std::wstring newContent = JoinLines(lines);
    std::wstring oldContent;
    ReadTextFile(ConfigPath(), oldContent);
    if (oldContent == newContent) {
        error.clear();
        return true;
    }

    const std::wstring finalPath = ConfigPath();
    const std::wstring tempPath = finalPath + L".tmp";
    if (!WriteTextFileUtf8(tempPath, newContent)) {
        error = L"Failed to open config file.";
        return false;
    }
    if (!MoveFileExW(tempPath.c_str(), finalPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tempPath.c_str());
        error = L"Failed to replace config file.";
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


