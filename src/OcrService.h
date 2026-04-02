#pragma once

#include "AppConfig.h"

#include <string>
#include <vector>

namespace app {

struct ServiceResult {
    bool success = false;
    std::wstring error;
    std::wstring text;
    std::vector<std::wstring> items;
};

class OcrService {
public:
    ServiceResult RecognizeImage(const ProviderConfig& provider, const std::wstring& modelId, const std::wstring& imagePath) const;
    ServiceResult TranslateText(const ProviderConfig& provider, const std::wstring& modelId, const std::wstring& input) const;
    ServiceResult FetchModels(const ProviderConfig& provider) const;
    ServiceResult TestModel(const ProviderConfig& provider, const std::wstring& modelId) const;
};

}  // namespace app
