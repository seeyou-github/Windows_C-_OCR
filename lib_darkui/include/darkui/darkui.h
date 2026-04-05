#pragma once

// Unified public entry point for the darkui control set.
// Recommended caller path:
// - 1. Create a Theme with MakePresetTheme(...) or MakeSemanticTheme(...).
// - 2. Create a top-level ThemedWindowHost in WM_CREATE and attach it to the window.
// - 3. Fill per-control Options, call Create(parent, id, host.theme(), options), then bind controls through host.theme_manager().
// Notes:
// - Layout and message routing remain standard Win32 responsibilities.
// - Include this header when the caller wants the full recommended darkui surface instead of per-control headers.

#include "darkui/checkbox.h"
#include "darkui/button.h"
#include "darkui/combobox.h"
#include "darkui/dialog.h"
#include "darkui/edit.h"
#include "darkui/listbox.h"
#include "darkui/listview.h"
#include "darkui/panel.h"
#include "darkui/progress.h"
#include "darkui/quick.h"
#include "darkui/radiobutton.h"
#include "darkui/slider.h"
#include "darkui/static.h"
#include "darkui/tab.h"
#include "darkui/themed_window_host.h"
#include "darkui/toolbar.h"
