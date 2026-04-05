#pragma once

#include "ProviderServiceBase.h"

namespace app {

class SiliconFlowProviderService : public ProviderServiceBase {
protected:
    std::string BuildProviderOptionsJson(const ProviderConfig& provider,
                                         const std::wstring& modelId,
                                         const RequestOptions& options) const override;
    std::string BuildOcrPrompt(const ProviderConfig& provider, const std::wstring& modelId) const override;
};

}  // namespace app
