#include "OcrService.h"

#include "OpenAiCompatibleProviderService.h"
#include "SiliconFlowProviderService.h"
#include "ZhipuProviderService.h"

namespace app {

namespace {

const ProviderServiceBase& ResolveService(const ProviderConfig& provider) {
    static const SiliconFlowProviderService siliconFlowService;
    static const ZhipuProviderService zhipuService;
    static const OpenAiCompatibleProviderService genericService;

    if (provider.id == L"siliconflow") {
        return siliconFlowService;
    }
    if (provider.id == L"zhipu") {
        return zhipuService;
    }
    return genericService;
}

}  // namespace

ServiceResult OcrService::RecognizeImage(const ProviderConfig& provider,
                                         const std::wstring& modelId,
                                         const std::wstring& imagePath,
                                         const RequestOptions& options,
                                         const StreamCallback& onChunk) const {
    return ResolveService(provider).RecognizeImage(provider, modelId, imagePath, options, onChunk);
}

ServiceResult OcrService::RecognizeImageDataUrl(const ProviderConfig& provider,
                                                const std::wstring& modelId,
                                                const std::string& imageDataUrl,
                                                const RequestOptions& options,
                                                const StreamCallback& onChunk) const {
    return ResolveService(provider).RecognizeImageDataUrl(provider, modelId, imageDataUrl, options, onChunk);
}

ServiceResult OcrService::TranslateText(const ProviderConfig& provider,
                                        const std::wstring& modelId,
                                        const std::wstring& input,
                                        const RequestOptions& options,
                                        const StreamCallback& onChunk) const {
    return ResolveService(provider).TranslateText(provider, modelId, input, options, onChunk);
}

ServiceResult OcrService::TranslateImageDataUrl(const ProviderConfig& provider,
                                                const std::wstring& modelId,
                                                const std::string& imageDataUrl,
                                                const RequestOptions& options,
                                                const StreamCallback& onChunk) const {
    return ResolveService(provider).TranslateImageDataUrl(provider, modelId, imageDataUrl, options, onChunk);
}

ServiceResult OcrService::FetchModels(const ProviderConfig& provider) const {
    return ResolveService(provider).FetchModels(provider);
}

ServiceResult OcrService::TestModel(const ProviderConfig& provider,
                                    const std::wstring& modelId,
                                    const std::wstring& prompt,
                                    const RequestOptions& options,
                                    const StreamCallback& onChunk) const {
    return ResolveService(provider).TestModel(provider, modelId, prompt, options, onChunk);
}

}  // namespace app
