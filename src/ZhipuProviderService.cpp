#include "ZhipuProviderService.h"

namespace app {

std::string ZhipuProviderService::BuildProviderOptionsJson(const ProviderConfig&,
                                                           const std::wstring&,
                                                           const RequestOptions& options) const {
    if (!options.includeReasoningOption) {
        return "";
    }
    if (options.enableReasoning) {
        return "\"thinking\":{\"type\":\"enabled\"},";
    }
    return "\"thinking\":{\"type\":\"disabled\"},";
}

}  // namespace app
