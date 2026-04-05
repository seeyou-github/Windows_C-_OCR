#include "SiliconFlowProviderService.h"

#include <algorithm>

namespace app {

namespace {

bool ContainsNoCase(const std::wstring& text, const wchar_t* needle) {
    if (!needle || !*needle) {
        return false;
    }
    std::wstring lowerText = text;
    std::wstring lowerNeedle = needle;
    std::transform(lowerText.begin(), lowerText.end(), lowerText.begin(), [](wchar_t ch) { return static_cast<wchar_t>(towlower(ch)); });
    std::transform(lowerNeedle.begin(), lowerNeedle.end(), lowerNeedle.begin(), [](wchar_t ch) { return static_cast<wchar_t>(towlower(ch)); });
    return lowerText.find(lowerNeedle) != std::wstring::npos;
}

}  // namespace

std::string SiliconFlowProviderService::BuildProviderOptionsJson(const ProviderConfig&,
                                                                const std::wstring&,
                                                                const RequestOptions& options) const {
    if (!options.includeReasoningOption) {
        return "";
    }
    if (options.enableReasoning) {
        return "\"thinking_budget\":1024,";
    }
    return "\"enable_thinking\":false,";
}

std::string SiliconFlowProviderService::BuildOcrPrompt(const ProviderConfig&, const std::wstring& modelId) const {
    if (ContainsNoCase(modelId, L"deepseek-ocr")) {
        return "<image>\n<|grounding|>OCR this image.";
    }
    return "Perform OCR on this image and return only the recognized text. Preserve line breaks. If no readable text exists, reply with No readable text.";
}

}  // namespace app
