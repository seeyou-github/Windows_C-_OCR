#pragma once

#include <string>
#include <windows.h>

namespace app {

void SetResourceInstance(HINSTANCE instance);
HINSTANCE GetResourceInstance();
std::wstring LoadStringResource(UINT id);
std::wstring FormatStatus(const std::wstring& left, const std::wstring& right);

}  // namespace app
