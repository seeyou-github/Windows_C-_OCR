#include "OcrService.h"

#include <windows.h>
#include <wininet.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace app {

namespace {

struct InternetHandle {
    HINTERNET handle = nullptr;
    explicit InternetHandle(HINTERNET value = nullptr) : handle(value) {}
    ~InternetHandle() {
        if (handle) {
            InternetCloseHandle(handle);
        }
    }
    InternetHandle(const InternetHandle&) = delete;
    InternetHandle& operator=(const InternetHandle&) = delete;
    explicit operator bool() const { return handle != nullptr; }
};

struct ParsedUrl {
    std::wstring host;
    std::wstring path;
    INTERNET_PORT port = INTERNET_DEFAULT_HTTP_PORT;
    bool secure = false;
};

std::wstring Utf8ToWide(const std::string& input) {
    if (input.empty()) {
        return L"";
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, nullptr, 0);
    std::wstring output(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, output.data(), size);
    output.resize(size - 1);
    return output;
}

std::string WideToUtf8(const std::wstring& input) {
    if (input.empty()) {
        return "";
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string output(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, output.data(), size, nullptr, nullptr);
    output.resize(size - 1);
    return output;
}

bool ParseUrl(const std::wstring& baseUrl, const std::wstring& path, ParsedUrl& parsed, std::wstring& error) {
    std::wstring fullUrl = baseUrl;
    if (!path.empty()) {
        if (!fullUrl.empty() && fullUrl.back() == L'/' && path.front() == L'/') {
            fullUrl.pop_back();
        }
        fullUrl += path;
    }

    URL_COMPONENTSW components{};
    wchar_t host[256] = {};
    wchar_t urlPath[1024] = {};
    components.dwStructSize = sizeof(components);
    components.lpszHostName = host;
    components.dwHostNameLength = static_cast<DWORD>(std::size(host));
    components.lpszUrlPath = urlPath;
    components.dwUrlPathLength = static_cast<DWORD>(std::size(urlPath));
    if (!InternetCrackUrlW(fullUrl.c_str(), 0, 0, &components)) {
        error = L"Invalid provider URL.";
        return false;
    }

    parsed.host.assign(components.lpszHostName, components.dwHostNameLength);
    parsed.path.assign(components.lpszUrlPath, components.dwUrlPathLength);
    parsed.port = components.nPort;
    parsed.secure = (components.nScheme == INTERNET_SCHEME_HTTPS);
    return true;
}

bool ReadBinaryFile(const std::wstring& path, std::vector<char>& data) {
    std::ifstream file(std::filesystem::path(path), std::ios::binary);
    if (!file) {
        return false;
    }
    data.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return true;
}

std::string GuessMimeType(const std::wstring& path) {
    std::wstring lower = path;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](wchar_t ch) { return static_cast<wchar_t>(towlower(ch)); });
    const auto endsWith = [&](const wchar_t* suffix) {
        const std::size_t suffixLength = wcslen(suffix);
        return lower.size() >= suffixLength && lower.compare(lower.size() - suffixLength, suffixLength, suffix) == 0;
    };
    if (endsWith(L".png")) return "image/png";
    if (endsWith(L".jpg") || endsWith(L".jpeg")) return "image/jpeg";
    if (endsWith(L".bmp")) return "image/bmp";
    if (endsWith(L".gif")) return "image/gif";
    if (endsWith(L".webp")) return "image/webp";
    return "application/octet-stream";
}

std::string Base64Encode(const std::vector<char>& data) {
    static const char kTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string output;
    output.reserve(((data.size() + 2) / 3) * 4);
    for (std::size_t i = 0; i < data.size(); i += 3) {
        const unsigned int b0 = static_cast<unsigned char>(data[i]);
        const unsigned int b1 = (i + 1 < data.size()) ? static_cast<unsigned char>(data[i + 1]) : 0;
        const unsigned int b2 = (i + 2 < data.size()) ? static_cast<unsigned char>(data[i + 2]) : 0;
        const unsigned int triple = (b0 << 16) | (b1 << 8) | b2;
        output.push_back(kTable[(triple >> 18) & 0x3F]);
        output.push_back(kTable[(triple >> 12) & 0x3F]);
        output.push_back((i + 1 < data.size()) ? kTable[(triple >> 6) & 0x3F] : '=');
        output.push_back((i + 2 < data.size()) ? kTable[triple & 0x3F] : '=');
    }
    return output;
}

std::string JsonEscape(const std::string& input) {
    std::string output;
    for (const char ch : input) {
        switch (ch) {
        case '\\': output += "\\\\"; break;
        case '"': output += "\\\""; break;
        case '\r': output += "\\r"; break;
        case '\n': output += "\\n"; break;
        case '\t': output += "\\t"; break;
        default: output.push_back(ch); break;
        }
    }
    return output;
}

std::string JsonUnescape(const std::string& input) {
    std::string output;
    bool escaping = false;
    for (char ch : input) {
        if (escaping) {
            switch (ch) {
            case 'n': output.push_back('\n'); break;
            case 'r': break;
            case 't': output.push_back('\t'); break;
            default: output.push_back(ch); break;
            }
            escaping = false;
        } else if (ch == '\\') {
            escaping = true;
        } else {
            output.push_back(ch);
        }
    }
    return output;
}

bool HttpRequest(const ProviderConfig& provider,
                 const std::wstring& method,
                 const std::wstring& path,
                 const std::string& headers,
                 const std::vector<char>& body,
                 std::string& response,
                 std::wstring& error) {
    ParsedUrl parsed;
    if (!ParseUrl(provider.baseUrl, path, parsed, error)) {
        return false;
    }

    InternetHandle internet(InternetOpenW(L"Win32OCR/2.0", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0));
    InternetHandle connection;
    InternetHandle request;
    if (!internet) {
        error = L"InternetOpenW failed.";
        return false;
    }

    connection.handle = InternetConnectW(internet.handle, parsed.host.c_str(), parsed.port, nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0);
    if (!connection) {
        error = L"InternetConnectW failed.";
        return false;
    }

    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
    if (parsed.secure) {
        flags |= INTERNET_FLAG_SECURE;
    }
    request.handle = HttpOpenRequestW(connection.handle, method.c_str(), parsed.path.c_str(), nullptr, nullptr, nullptr, flags, 0);
    if (!request) {
        error = L"HttpOpenRequestW failed.";
        return false;
    }

    if (!HttpSendRequestA(request.handle,
                          headers.empty() ? nullptr : headers.c_str(),
                          static_cast<DWORD>(headers.size()),
                          body.empty() ? nullptr : const_cast<char*>(body.data()),
                          static_cast<DWORD>(body.size()))) {
        error = L"HttpSendRequestA failed.";
        return false;
    }

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    HttpQueryInfoW(request.handle, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &statusCodeSize, nullptr);

    char buffer[4096];
    DWORD read = 0;
    response.clear();
    while (InternetReadFile(request.handle, buffer, sizeof(buffer), &read) && read > 0) {
        response.append(buffer, buffer + read);
    }

    if (statusCode >= 400) {
        error = L"HTTP " + std::to_wstring(statusCode) + L": " + Utf8ToWide(response);
        return false;
    }
    return true;
}

std::string ExtractContentText(const std::string& json) {
    std::size_t pos = json.find("\"content\"");
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos);
    pos = json.find('"', pos);
    if (pos == std::string::npos) return "";
    ++pos;
    std::string raw;
    bool escaping = false;
    while (pos < json.size()) {
        const char ch = json[pos++];
        if (escaping) {
            raw.push_back('\\');
            raw.push_back(ch);
            escaping = false;
            continue;
        }
        if (ch == '\\') {
            escaping = true;
            continue;
        }
        if (ch == '"') {
            break;
        }
        raw.push_back(ch);
    }
    return JsonUnescape(raw);
}

std::vector<std::wstring> ExtractModelIds(const std::string& json) {
    std::vector<std::wstring> models;
    std::size_t pos = 0;
    while ((pos = json.find("\"id\"", pos)) != std::string::npos) {
        pos = json.find(':', pos);
        pos = json.find('"', pos);
        if (pos == std::string::npos) break;
        ++pos;
        std::string raw;
        while (pos < json.size() && json[pos] != '"') {
            raw.push_back(json[pos++]);
        }
        if (!raw.empty()) {
            models.push_back(Utf8ToWide(raw));
        }
    }
    std::sort(models.begin(), models.end());
    models.erase(std::unique(models.begin(), models.end()), models.end());
    return models;
}

std::string BuildHeaders(const ProviderConfig& provider) {
    return "Authorization: Bearer " + WideToUtf8(provider.apiKey) + "\r\n"
           "Content-Type: application/json\r\n";
}

}  // namespace

ServiceResult OcrService::RecognizeImage(const ProviderConfig& provider, const std::wstring& modelId, const std::wstring& imagePath) const {
    ServiceResult result;
    std::vector<char> imageData;
    if (!ReadBinaryFile(imagePath, imageData)) {
        result.error = L"Failed to read image file.";
        return result;
    }

    const std::string dataUrl = "data:" + GuessMimeType(imagePath) + ";base64," + Base64Encode(imageData);
    const std::string prompt =
        "Please perform OCR on this image and return only the recognized text. Preserve line breaks where possible. "
        "If there is no readable text, respond with No readable text.";

    const std::string body =
        "{"
        "\"model\":\"" + JsonEscape(WideToUtf8(modelId)) + "\","
        "\"stream\":false,"
        "\"messages\":[{"
            "\"role\":\"user\","
            "\"content\":["
                "{\"type\":\"image_url\",\"image_url\":{\"url\":\"" + JsonEscape(dataUrl) + "\",\"detail\":\"high\"}},"
                "{\"type\":\"text\",\"text\":\"" + JsonEscape(prompt) + "\"}"
            "]"
        "}]"
        "}";

    std::string response;
    if (!HttpRequest(provider, L"POST", provider.apiPath, BuildHeaders(provider), {body.begin(), body.end()}, response, result.error)) {
        return result;
    }
    result.text = Utf8ToWide(ExtractContentText(response));
    result.success = !result.text.empty();
    if (!result.success) {
        result.error = L"Failed to parse OCR response.";
    }
    return result;
}

ServiceResult OcrService::TranslateText(const ProviderConfig& provider, const std::wstring& modelId, const std::wstring& input) const {
    ServiceResult result;
    const std::string prompt = "Translate the following text into Chinese and return only the translated text:\n" + WideToUtf8(input);
    const std::string body =
        "{"
        "\"model\":\"" + JsonEscape(WideToUtf8(modelId)) + "\","
        "\"stream\":false,"
        "\"messages\":[{\"role\":\"user\",\"content\":\"" + JsonEscape(prompt) + "\"}]"
        "}";

    std::string response;
    if (!HttpRequest(provider, L"POST", provider.apiPath, BuildHeaders(provider), {body.begin(), body.end()}, response, result.error)) {
        return result;
    }
    result.text = Utf8ToWide(ExtractContentText(response));
    result.success = !result.text.empty();
    if (!result.success) {
        result.error = L"Failed to parse translate response.";
    }
    return result;
}

ServiceResult OcrService::FetchModels(const ProviderConfig& provider) const {
    ServiceResult result;
    const std::wstring path = provider.baseUrl.find(L"/v1") != std::wstring::npos ? L"/models" : L"/v1/models";
    std::string response;
    if (!HttpRequest(provider, L"GET", path, BuildHeaders(provider), {}, response, result.error)) {
        return result;
    }
    result.items = ExtractModelIds(response);
    result.success = !result.items.empty();
    if (!result.success) {
        result.error = L"Failed to fetch model list.";
    }
    return result;
}

ServiceResult OcrService::TestModel(const ProviderConfig& provider, const std::wstring& modelId) const {
    ServiceResult result;
    const std::string body =
        "{"
        "\"model\":\"" + JsonEscape(WideToUtf8(modelId)) + "\","
        "\"stream\":false,"
        "\"messages\":[{\"role\":\"user\",\"content\":\"Reply with OK.\"}]"
        "}";
    std::string response;
    if (!HttpRequest(provider, L"POST", provider.apiPath, BuildHeaders(provider), {body.begin(), body.end()}, response, result.error)) {
        return result;
    }
    result.text = Utf8ToWide(ExtractContentText(response));
    result.success = !result.text.empty();
    if (!result.success) {
        result.error = L"Failed to parse model test response.";
    }
    return result;
}

}  // namespace app
