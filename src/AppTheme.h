#pragma once

#include "darkui/darkui.h"

namespace app {

darkui::Theme MakeAppTheme(const std::wstring& themeName, int fontSize);

}  // namespace app
