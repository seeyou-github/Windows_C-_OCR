#include "OpenAiCompatibleProviderService.h"

namespace app {

std::string OpenAiCompatibleProviderService::BuildProviderOptionsJson(const ProviderConfig&,
                                                                      const std::wstring&,
                                                                      const RequestOptions& options) const {
    if (!options.includeReasoningOption) {
        return "";
    }
    if (options.enableReasoning) {
        return "\"reasoning_effort\":\"medium\",";
    }
    return "";
}

}  // namespace app
