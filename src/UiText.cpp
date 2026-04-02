#include "UiText.h"

namespace app {

namespace {

HINSTANCE g_resourceInstance = nullptr;

}  // namespace

void SetResourceInstance(HINSTANCE instance) {
    g_resourceInstance = instance;
}

HINSTANCE GetResourceInstance() {
    return g_resourceInstance;
}

std::wstring LoadStringResource(UINT id) {
    wchar_t buffer[512] = {};
    const int length = LoadStringW(g_resourceInstance, id, buffer, static_cast<int>(std::size(buffer)));
    return (length > 0) ? std::wstring(buffer, buffer + length) : L"";
}

std::wstring FormatStatus(const std::wstring& left, const std::wstring& right) {
    if (left.empty()) {
        return right;
    }
    if (right.empty()) {
        return left;
    }
    return left + L" - " + right;
}

}  // namespace app
