#pragma once

#include "ProviderServiceBase.h"

namespace app {

class ZhipuProviderService : public ProviderServiceBase {
protected:
    std::string BuildProviderOptionsJson(const ProviderConfig& provider,
                                         const std::wstring& modelId,
                                         const RequestOptions& options) const override;
};

}  // namespace app
