#pragma once

#include "OcrService.h"

#include <string>

namespace app {

class ProviderServiceBase {
public:
    virtual ~ProviderServiceBase() = default;

    ServiceResult RecognizeImage(const ProviderConfig& provider,
                                 const std::wstring& modelId,
                                 const std::wstring& imagePath,
                                 const RequestOptions& options,
                                 const OcrService::StreamCallback& onChunk) const;
    ServiceResult RecognizeImageDataUrl(const ProviderConfig& provider,
                                        const std::wstring& modelId,
                                        const std::string& imageDataUrl,
                                        const RequestOptions& options,
                                        const OcrService::StreamCallback& onChunk) const;
    ServiceResult TranslateText(const ProviderConfig& provider,
                                const std::wstring& modelId,
                                const std::wstring& input,
                                const RequestOptions& options,
                                const OcrService::StreamCallback& onChunk) const;
    ServiceResult TranslateImageDataUrl(const ProviderConfig& provider,
                                        const std::wstring& modelId,
                                        const std::string& imageDataUrl,
                                        const RequestOptions& options,
                                        const OcrService::StreamCallback& onChunk) const;
    ServiceResult FetchModels(const ProviderConfig& provider) const;
    ServiceResult TestModel(const ProviderConfig& provider,
                            const std::wstring& modelId,
                            const std::wstring& prompt,
                            const RequestOptions& options,
                            const OcrService::StreamCallback& onChunk) const;

protected:
    virtual std::string BuildProviderOptionsJson(const ProviderConfig& provider,
                                                 const std::wstring& modelId,
                                                 const RequestOptions& options) const;
    virtual std::string BuildHeaders(const ProviderConfig& provider, bool stream) const;
    virtual std::string BuildOcrPrompt(const ProviderConfig& provider, const std::wstring& modelId) const;

private:
    ServiceResult SendChatRequest(const ProviderConfig& provider,
                                  const std::wstring& modelId,
                                  const std::string& messagesJson,
                                  const RequestOptions& options,
                                  const OcrService::StreamCallback& onChunk,
                                  const std::wstring& requestText) const;
};

}  // namespace app
