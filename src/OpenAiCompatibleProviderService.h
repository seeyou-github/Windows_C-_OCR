#pragma once

#include "ProviderServiceBase.h"

namespace app {

class OpenAiCompatibleProviderService : public ProviderServiceBase {
protected:
    std::string BuildProviderOptionsJson(const ProviderConfig& provider,
                                         const std::wstring& modelId,
                                         const RequestOptions& options) const override;
};

}  // namespace app
