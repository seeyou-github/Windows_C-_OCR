#pragma once

#include "AppConfig.h"

#include <functional>
#include <string>
#include <vector>

namespace app {

struct RequestOptions {
    bool stream = false;
    bool enableReasoning = false;
    bool includeReasoningOption = false;
    DWORD timeoutMs = 30000;
    std::wstring ocrPrompt;
    std::wstring translateTextPrompt;
};

struct ServiceResult {
    bool success = false;
    std::wstring error;
    std::wstring text;
    std::vector<std::wstring> items;
    std::wstring requestText;
    std::wstring responseText;
};

class OcrService {
public:
    using StreamCallback = std::function<void(const std::wstring&)>;

    ServiceResult RecognizeImage(const ProviderConfig& provider,
                                 const std::wstring& modelId,
                                 const std::wstring& imagePath,
                                 const RequestOptions& options = {},
                                 const StreamCallback& onChunk = {}) const;
    ServiceResult RecognizeImageDataUrl(const ProviderConfig& provider,
                                        const std::wstring& modelId,
                                        const std::string& imageDataUrl,
                                        const RequestOptions& options = {},
                                        const StreamCallback& onChunk = {}) const;
    ServiceResult TranslateText(const ProviderConfig& provider,
                                const std::wstring& modelId,
                                const std::wstring& input,
                                const RequestOptions& options = {},
                                const StreamCallback& onChunk = {}) const;
    ServiceResult TranslateImageDataUrl(const ProviderConfig& provider,
                                        const std::wstring& modelId,
                                        const std::string& imageDataUrl,
                                        const RequestOptions& options = {},
                                        const StreamCallback& onChunk = {}) const;
    ServiceResult FetchModels(const ProviderConfig& provider) const;
    ServiceResult TestModel(const ProviderConfig& provider,
                            const std::wstring& modelId,
                            const std::wstring& prompt,
                            const RequestOptions& options = {},
                            const StreamCallback& onChunk = {}) const;
};

}  // namespace app
