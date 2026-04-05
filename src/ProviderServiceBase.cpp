#include "ProviderServiceBase.h"

#include <windows.h>
#include <wininet.h>

#include <algorithm>
#include <cwctype>
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
    if (endsWith(L".pdf")) return "application/pdf";
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

std::string SanitizeRequestBodyForDebug(std::string body) {
    // Redact large inline data-url payloads to prevent UI freeze when showing debug text.
    const std::string key = "\"url\":\"data:";
    std::size_t pos = 0;
    while ((pos = body.find(key, pos)) != std::string::npos) {
        std::size_t valueStart = pos + 7;  // points at first char after "\"url\":\""
        std::size_t quoteEnd = valueStart;
        bool escaped = false;
        while (quoteEnd < body.size()) {
            const char ch = body[quoteEnd];
            if (!escaped && ch == '"') {
                break;
            }
            escaped = (!escaped && ch == '\\');
            if (ch != '\\') {
                escaped = false;
            }
            ++quoteEnd;
        }
        if (quoteEnd <= valueStart || quoteEnd >= body.size()) {
            break;
        }
        body.replace(valueStart, quoteEnd - valueStart, "[omitted-data-url]");
        pos = valueStart + 18;
    }

    constexpr std::size_t kMaxDebugChars = 24000;
    if (body.size() > kMaxDebugChars) {
        body.resize(kMaxDebugChars);
        body += "...(truncated)";
    }
    return body;
}

bool IsBlankText(const std::wstring& text) {
    for (wchar_t ch : text) {
        if (!iswspace(ch)) {
            return false;
        }
    }
    return true;
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

std::size_t FindMatchingObjectEnd(const std::string& json, std::size_t objectStart) {
    if (objectStart >= json.size() || json[objectStart] != '{') {
        return std::string::npos;
    }
    int depth = 0;
    bool inString = false;
    bool escaping = false;
    for (std::size_t i = objectStart; i < json.size(); ++i) {
        const char ch = json[i];
        if (inString) {
            if (escaping) {
                escaping = false;
                continue;
            }
            if (ch == '\\') {
                escaping = true;
                continue;
            }
            if (ch == '"') {
                inString = false;
            }
            continue;
        }
        if (ch == '"') {
            inString = true;
            continue;
        }
        if (ch == '{') {
            ++depth;
            continue;
        }
        if (ch == '}') {
            --depth;
            if (depth == 0) {
                return i;
            }
        }
    }
    return std::string::npos;
}

std::string ExtractJsonStringByExactKey(const std::string& objectJson, const char* key) {
    const std::string keyToken = std::string("\"") + key + "\"";
    std::size_t keyPos = objectJson.find(keyToken);
    while (keyPos != std::string::npos) {
        std::size_t colon = objectJson.find(':', keyPos + keyToken.size());
        if (colon == std::string::npos) {
            return "";
        }
        std::size_t pos = colon + 1;
        while (pos < objectJson.size() &&
               (objectJson[pos] == ' ' || objectJson[pos] == '\t' || objectJson[pos] == '\r' || objectJson[pos] == '\n')) {
            ++pos;
        }
        if (pos < objectJson.size() && objectJson[pos] == '"') {
            ++pos;
            std::string raw;
            bool escaping = false;
            while (pos < objectJson.size()) {
                const char ch = objectJson[pos++];
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
                    return JsonUnescape(raw);
                }
                raw.push_back(ch);
            }
            return "";
        }
        keyPos = objectJson.find(keyToken, keyPos + keyToken.size());
    }
    return "";
}

std::string ExtractJsonStringAfter(const std::string& json, std::size_t startPos, const char* key) {
    std::size_t pos = json.find(key, startPos);
    if (pos == std::string::npos) {
        return "";
    }
    pos = json.find(':', pos);
    if (pos == std::string::npos) {
        return "";
    }
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\r' || json[pos] == '\n')) {
        ++pos;
    }
    // Only parse plain JSON string values. If the value is an object/array, ignore it.
    if (pos >= json.size() || json[pos] != '"') {
        return "";
    }
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

std::string ExtractContentText(const std::string& json) {
    return ExtractJsonStringAfter(json, 0, "\"content\"");
}

std::string ExtractStreamDeltaText(const std::string& json) {
    const std::size_t deltaPos = json.find("\"delta\"");
    if (deltaPos == std::string::npos) {
        return "";
    }
    std::size_t colon = json.find(':', deltaPos);
    if (colon == std::string::npos) {
        return "";
    }
    std::size_t objectStart = json.find('{', colon + 1);
    if (objectStart == std::string::npos) {
        return "";
    }
    const std::size_t objectEnd = FindMatchingObjectEnd(json, objectStart);
    if (objectEnd == std::string::npos || objectEnd <= objectStart) {
        return "";
    }
    const std::string deltaObject = json.substr(objectStart, objectEnd - objectStart + 1);
    return ExtractJsonStringByExactKey(deltaObject, "content");
}

std::vector<std::wstring> ExtractModelIds(const std::string& json) {
    std::vector<std::wstring> models;
    std::size_t pos = 0;
    while ((pos = json.find("\"id\"", pos)) != std::string::npos) {
        pos = json.find(':', pos);
        pos = json.find('"', pos);
        if (pos == std::string::npos) {
            break;
        }
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

std::wstring DeriveModelsPath(const ProviderConfig& provider) {
    if (!provider.apiPath.empty()) {
        std::wstring path = provider.apiPath;
        const std::wstring chatCompletions = L"/chat/completions";
        const std::wstring completions = L"/completions";
        if (path.size() >= chatCompletions.size() &&
            path.compare(path.size() - chatCompletions.size(), chatCompletions.size(), chatCompletions) == 0) {
            path.erase(path.size() - chatCompletions.size());
            return path + L"/models";
        }
        if (path.size() >= completions.size() &&
            path.compare(path.size() - completions.size(), completions.size(), completions) == 0) {
            path.erase(path.size() - completions.size());
            return path + L"/models";
        }
        const std::size_t lastSlash = path.find_last_of(L'/');
        if (lastSlash != std::wstring::npos && lastSlash > 0) {
            return path.substr(0, lastSlash) + L"/models";
        }
    }
    return L"/v1/models";
}

bool HttpRequest(const ProviderConfig& provider,
                 const std::wstring& method,
                 const std::wstring& path,
                 const std::string& headers,
                 const std::vector<char>& body,
                 DWORD timeoutMs,
                 bool stream,
                 const OcrService::StreamCallback& onChunk,
                 std::string& response,
                 std::wstring& error,
                 std::string* aggregatedText = nullptr) {
    ParsedUrl parsed;
    if (!ParseUrl(provider.baseUrl, path, parsed, error)) {
        return false;
    }

    InternetHandle internet(InternetOpenW(L"Win32OCR/2.1", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0));
    if (!internet) {
        error = L"InternetOpenW failed.";
        return false;
    }
    if (timeoutMs == 0) {
        timeoutMs = 30000;
    }
    InternetSetOptionW(internet.handle, INTERNET_OPTION_CONNECT_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
    InternetSetOptionW(internet.handle, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
    InternetSetOptionW(internet.handle, INTERNET_OPTION_SEND_TIMEOUT, &timeoutMs, sizeof(timeoutMs));

    InternetHandle connection(InternetConnectW(internet.handle, parsed.host.c_str(), parsed.port, nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0));
    if (!connection) {
        error = L"InternetConnectW failed.";
        return false;
    }
    InternetSetOptionW(connection.handle, INTERNET_OPTION_CONNECT_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
    InternetSetOptionW(connection.handle, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
    InternetSetOptionW(connection.handle, INTERNET_OPTION_SEND_TIMEOUT, &timeoutMs, sizeof(timeoutMs));

    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
    if (parsed.secure) {
        flags |= INTERNET_FLAG_SECURE;
    }
    InternetHandle request(HttpOpenRequestW(connection.handle, method.c_str(), parsed.path.c_str(), nullptr, nullptr, nullptr, flags, 0));
    if (!request) {
        error = L"HttpOpenRequestW failed.";
        return false;
    }
    InternetSetOptionW(request.handle, INTERNET_OPTION_CONNECT_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
    InternetSetOptionW(request.handle, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
    InternetSetOptionW(request.handle, INTERNET_OPTION_SEND_TIMEOUT, &timeoutMs, sizeof(timeoutMs));

    if (!HttpSendRequestA(request.handle,
                          headers.empty() ? nullptr : headers.c_str(),
                          static_cast<DWORD>(headers.size()),
                          body.empty() ? nullptr : const_cast<char*>(body.data()),
                          static_cast<DWORD>(body.size()))) {
        const DWORD winErr = GetLastError();
        if (winErr == ERROR_INTERNET_TIMEOUT) {
            error = L"Request timed out (" + std::to_wstring(timeoutMs / 1000) + L"s).";
        } else {
            error = L"HttpSendRequestA failed.";
        }
        return false;
    }

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    HttpQueryInfoW(request.handle, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &statusCodeSize, nullptr);

    char buffer[4096];
    DWORD read = 0;
    response.clear();
    std::string sseBuffer;
    std::string fullText;
    bool readOk = true;
    while ((readOk = (InternetReadFile(request.handle, buffer, sizeof(buffer), &read) != FALSE)) && read > 0) {
        response.append(buffer, buffer + read);
        if (!stream) {
            continue;
        }
        sseBuffer.append(buffer, buffer + read);
        std::size_t lineEnd = 0;
        while ((lineEnd = sseBuffer.find('\n')) != std::string::npos) {
            std::string line = sseBuffer.substr(0, lineEnd);
            sseBuffer.erase(0, lineEnd + 1);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (line.rfind("data:", 0) != 0) {
                continue;
            }
            std::string payload = line.substr(5);
            while (!payload.empty() && payload.front() == ' ') {
                payload.erase(payload.begin());
            }
            if (payload == "[DONE]") {
                continue;
            }
            const std::string delta = ExtractStreamDeltaText(payload);
            if (!delta.empty()) {
                fullText += delta;
                if (onChunk) {
                    onChunk(Utf8ToWide(delta));
                }
            }
        }
    }
    if (!readOk) {
        const DWORD winErr = GetLastError();
        if (winErr == ERROR_INTERNET_TIMEOUT) {
            error = L"Request timed out (" + std::to_wstring(timeoutMs / 1000) + L"s).";
        } else {
            error = L"InternetReadFile failed.";
        }
        return false;
    }

    if (aggregatedText) {
        *aggregatedText = fullText;
    }
    if (statusCode >= 400) {
        error = L"HTTP " + std::to_wstring(statusCode) + L": " + Utf8ToWide(response);
        return false;
    }
    return true;
}

}  // namespace

ServiceResult ProviderServiceBase::RecognizeImage(const ProviderConfig& provider,
                                                  const std::wstring& modelId,
                                                  const std::wstring& imagePath,
                                                  const RequestOptions& options,
                                                  const OcrService::StreamCallback& onChunk) const {
    ServiceResult result;
    std::vector<char> imageData;
    if (!ReadBinaryFile(imagePath, imageData)) {
        result.error = L"Failed to read image file.";
        return result;
    }

    const std::string dataUrl = "data:" + GuessMimeType(imagePath) + ";base64," + Base64Encode(imageData);
    return RecognizeImageDataUrl(provider, modelId, dataUrl, options, onChunk);
}

ServiceResult ProviderServiceBase::RecognizeImageDataUrl(const ProviderConfig& provider,
                                                         const std::wstring& modelId,
                                                         const std::string& imageDataUrl,
                                                         const RequestOptions& options,
                                                         const OcrService::StreamCallback& onChunk) const {
    const std::string prompt = options.ocrPrompt.empty() ? BuildOcrPrompt(provider, modelId) : WideToUtf8(options.ocrPrompt);
    const std::string messagesJson =
        "[{"
            "\"role\":\"user\","
            "\"content\":["
                "{\"type\":\"image_url\",\"image_url\":{\"url\":\"" + JsonEscape(imageDataUrl) + "\",\"detail\":\"high\"}},"
                "{\"type\":\"text\",\"text\":\"" + JsonEscape(prompt) + "\"}"
            "]"
        "}]";
    return SendChatRequest(provider, modelId, messagesJson, options, onChunk, Utf8ToWide(prompt));
}

ServiceResult ProviderServiceBase::TranslateText(const ProviderConfig& provider,
                                                 const std::wstring& modelId,
                                                 const std::wstring& input,
                                                 const RequestOptions& options,
                                                 const OcrService::StreamCallback& onChunk) const {
    const std::string promptPrefix = options.translateTextPrompt.empty()
        ? "Translate the following text into Chinese and return only the translated text:"
        : WideToUtf8(options.translateTextPrompt);
    const std::string prompt = promptPrefix + "\n" + WideToUtf8(input);
    const std::string messagesJson = "[{\"role\":\"user\",\"content\":\"" + JsonEscape(prompt) + "\"}]";
    return SendChatRequest(provider, modelId, messagesJson, options, onChunk, Utf8ToWide(prompt));
}

ServiceResult ProviderServiceBase::TranslateImageDataUrl(const ProviderConfig& provider,
                                                         const std::wstring& modelId,
                                                         const std::string& imageDataUrl,
                                                         const RequestOptions& options,
                                                         const OcrService::StreamCallback& onChunk) const {
    const std::string prompt = "Translate all text in this image into Chinese. Return only translated text. If no readable text exists, reply with No readable text.";
    const std::string messagesJson =
        "[{"
            "\"role\":\"user\","
            "\"content\":["
                "{\"type\":\"image_url\",\"image_url\":{\"url\":\"" + JsonEscape(imageDataUrl) + "\",\"detail\":\"high\"}},"
                "{\"type\":\"text\",\"text\":\"" + JsonEscape(prompt) + "\"}"
            "]"
        "}]";
    return SendChatRequest(provider, modelId, messagesJson, options, onChunk, Utf8ToWide(prompt));
}

ServiceResult ProviderServiceBase::FetchModels(const ProviderConfig& provider) const {
    ServiceResult result;
    const std::wstring path = DeriveModelsPath(provider);
    std::string response;
    if (!HttpRequest(provider, L"GET", path, BuildHeaders(provider, false), {}, 30000, false, {}, response, result.error)) {
        return result;
    }
    result.responseText = Utf8ToWide(response);
    result.items = ExtractModelIds(response);
    result.success = !result.items.empty();
    if (!result.success) {
        result.error = L"Failed to fetch model list.";
    }
    return result;
}

ServiceResult ProviderServiceBase::TestModel(const ProviderConfig& provider,
                                             const std::wstring& modelId,
                                             const std::wstring& prompt,
                                             const RequestOptions& options,
                                             const OcrService::StreamCallback& onChunk) const {
    const std::wstring safePrompt = prompt.empty() ? L"今日天气" : prompt;
    const std::string messagesJson =
        "[{\"role\":\"user\",\"content\":\"" + JsonEscape(WideToUtf8(safePrompt)) + "\"}]";
    return SendChatRequest(provider, modelId, messagesJson, options, onChunk, safePrompt);
}

std::string ProviderServiceBase::BuildProviderOptionsJson(const ProviderConfig&, const std::wstring&, const RequestOptions&) const {
    return "";
}

std::string ProviderServiceBase::BuildHeaders(const ProviderConfig& provider, bool stream) const {
    return "Authorization: Bearer " + WideToUtf8(provider.apiKey) + "\r\n"
           "Content-Type: application/json\r\n"
           "Accept: " + std::string(stream ? "text/event-stream" : "application/json") + "\r\n";
}

std::string ProviderServiceBase::BuildOcrPrompt(const ProviderConfig&, const std::wstring&) const {
    return "Perform OCR on this image and return only the recognized text. Preserve line breaks. If no readable text exists, reply with No readable text.";
}

ServiceResult ProviderServiceBase::SendChatRequest(const ProviderConfig& provider,
                                                   const std::wstring& modelId,
                                                   const std::string& messagesJson,
                                                   const RequestOptions& options,
                                                   const OcrService::StreamCallback& onChunk,
                                                   const std::wstring& requestText) const {
    ServiceResult result;
    result.requestText = requestText;

    std::string body =
        "{"
        "\"model\":\"" + JsonEscape(WideToUtf8(modelId)) + "\","
        "\"stream\":" + std::string(options.stream ? "true" : "false") + ",";
    body += BuildProviderOptionsJson(provider, modelId, options);
    body += "\"messages\":" + messagesJson + "}";
    result.requestText =
        L"model=" + modelId +
        L"\r\nstream=" + std::wstring(options.stream ? L"true" : L"false") +
        L"\r\nenableReasoning=" + std::wstring(options.enableReasoning ? L"true" : L"false") +
        L"\r\nincludeReasoningOption=" + std::wstring(options.includeReasoningOption ? L"true" : L"false") +
        L"\r\npayload=\r\n" + Utf8ToWide(SanitizeRequestBodyForDebug(body));

    std::string response;
    std::string streamedText;
    if (!HttpRequest(provider,
                     L"POST",
                     provider.apiPath,
                     BuildHeaders(provider, options.stream),
                     {body.begin(), body.end()},
                     options.timeoutMs,
                     options.stream,
                     onChunk,
                     response,
                     result.error,
                     &streamedText)) {
        result.responseText = Utf8ToWide(response);
        return result;
    }

    result.responseText = Utf8ToWide(response);
    result.text = options.stream ? Utf8ToWide(streamedText) : Utf8ToWide(ExtractContentText(response));
    result.success = !result.text.empty() && !IsBlankText(result.text);
    if (!result.success) {
        result.error = L"Failed to parse response content.";
    }
    return result;
}

}  // namespace app
