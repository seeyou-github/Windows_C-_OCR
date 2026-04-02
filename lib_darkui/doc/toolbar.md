# Toolbar

## Overview

`darkui::Toolbar` is a custom dark toolbar control. It supports standard buttons, icon buttons, separators, right-aligned items, drop-down menus, and overflow handling for compact layouts.

## Files

- `include/darkui/toolbar.h`
- `src/toolbar.cpp`

## Suitable Scenarios

- Top application toolbars
- File-manager style action bars
- Menu-bar style drop-down command surfaces

## Main Features

- Standard text buttons
- Icon buttons and icon-only buttons
- Separator items
- Right-aligned item group
- Drop-down menus
- Automatic overflow handling
- Standard `WM_COMMAND` notification flow

## Core Type

```cpp
struct ToolbarItem {
    std::wstring text;
    int commandId = 0;
    HICON icon = nullptr;
    HMENU popupMenu = nullptr;
    std::uintptr_t userData = 0;
    bool separator = false;
    bool checked = false;
    bool disabled = false;
    bool alignRight = false;
    bool iconOnly = false;
    bool dropDown = false;
    int iconScalePercent = 0;
};
```

## Recommended Usage

```cpp
#include "darkui/toolbar.h"

darkui::Theme theme;
theme.toolbarBackground = RGB(25, 28, 32);
theme.toolbarItem = RGB(46, 51, 58);
theme.toolbarItemHot = RGB(64, 71, 82);
theme.toolbarItemActive = RGB(78, 120, 184);
theme.toolbarText = RGB(228, 232, 238);
theme.toolbarTextActive = RGB(248, 250, 252);
theme.toolbarSeparator = RGB(70, 76, 86);
theme.toolbarHeight = 44;
theme.uiFont.height = -19;
theme.textPadding = 12;

darkui::Toolbar toolbar;
darkui::Toolbar::Options options;
options.items = {
    {L"New", 8101, iconNew},
    {L"Open", 8102, iconOpen},
    {L"", 0, nullptr, nullptr, 0, true},
    {L"", 8104, iconSearch, nullptr, 0, false, false, false, false, true},
    {L"Share", 8103, iconShare, nullptr, 0, false, false, false, true},
    {L"Layout", 8105, iconLayout, layoutMenu, 0, false, false, false, true, false, true},
};
options.variant = darkui::ToolbarVariant::Accent;

toolbar.Create(hwnd, 8001, theme, options);

darkui::ThemedWindowHost host;
darkui::ThemedWindowHost::Options hostOptions;
hostOptions.theme = theme;
host.Attach(hwnd, hostOptions);
auto& themeManager = host.theme_manager();
themeManager.Bind(toolbar);
themeManager.Apply();

MoveWindow(toolbar.hwnd(), 0, 0, 900, theme.toolbarHeight + 12, TRUE);
```

## Icon Sizing

`ToolbarItem::iconScalePercent` controls icon size as a percentage of the button's
main dimension.

```cpp
darkui::ToolbarItem openItem{L"Open", 8102, iconOpen};
openItem.iconScalePercent = 72;

darkui::ToolbarItem searchItem{L"Search", 8104, iconSearch, nullptr, 0, false, false, false, false, true};
searchItem.iconScalePercent = 88;

toolbar.SetItems({openItem, searchItem});
```

Notes:

- `0` means use the default `80`
- Values are clamped to `[1, 100]`
- Icons preserve aspect ratio and are never stretched
- The same percentage model is used by both icon-only buttons and icon-plus-text buttons

## Height Model

Toolbar height has three related layers:

- `theme.toolbarHeight`: the intended button height used by layout and sizing logic
- Toolbar window height: controlled by your `MoveWindow()` call
- Actual button height: toolbar window client height minus internal top and bottom padding

Current implementation expects:

```cpp
MoveWindow(toolbar.hwnd(), x, y, width, theme.toolbarHeight + 12, TRUE);
```

That matches the toolbar's internal layout:

- top inset: `6`
- bottom inset: `6`
- button height: `window height - 12`

So when `theme.toolbarHeight = 44`, the typical toolbar window height should be `56`,
which produces buttons that are `44` pixels tall.

## Font And Text Sizing

- Button text size comes from `theme.uiFont`
- Button text does not auto-scale from `toolbarHeight`
- If you increase toolbar height and want larger text, also increase `theme.uiFont.height`
- Button width is measured from the current font metrics, icon draw width, horizontal padding, and drop-down arrow width when present

Typical paired setup:

```cpp
theme.uiFont.height = -19;
theme.toolbarHeight = 44;
MoveWindow(toolbar.hwnd(), 0, 0, 900, theme.toolbarHeight + 12, TRUE);
```

## Parent Message Handling

```cpp
case WM_COMMAND:
    switch (LOWORD(wParam)) {
    case 8101:
        return 0;
    case 8102:
        return 0;
    case 8103:
        return 0;
    }
    break;
```

## Common API

### `SetItems` / `AddItem` / `ClearItems`

```cpp
toolbar.SetItems(items);
toolbar.AddItem({L"Export", 8104});
toolbar.ClearItems();
```

### `SetItem`

```cpp
toolbar.SetItem(1, {L"Open", 8102, iconOpen});
```

### `SetChecked` / `SetDisabled`

```cpp
toolbar.SetChecked(0, true);
toolbar.SetDisabled(3, true);
```

### `ThemedWindowHost \+ ThemeManager`

```cpp
darkui::ThemedWindowHost host;
darkui::ThemedWindowHost::Options hostOptions;
hostOptions.theme = theme;
host.Attach(hwnd, hostOptions);
auto& themeManager = host.theme_manager();
themeManager.Bind(toolbar);
themeManager.Apply();
```

### `variant`

Prefer semantic presets before hand-tuning spacing and emphasis:

```cpp
options.variant = darkui::ToolbarVariant::Default;
options.variant = darkui::ToolbarVariant::Dense;
options.variant = darkui::ToolbarVariant::Accent;
```

### `GetCount` / `GetItem`

```cpp
std::size_t count = toolbar.GetCount();
auto item = toolbar.GetItem(0);
```

## Theme Fields Used

- `toolbarBackground`
- `toolbarItem`
- `toolbarItemHot`
- `toolbarItemActive`
- `toolbarText`
- `toolbarTextActive`
- `toolbarSeparator`
- `toolbarHeight`
- `buttonDisabledText`
- `textPadding`
- `uiFont`

## Usage Notes

- When `iconOnly = true`, still provide `text` so overflow popups can show a readable label
- Toolbar icons default to `80%` of the button's main dimension
- `iconScalePercent` overrides that percentage per item
- Icons always preserve aspect ratio and are never stretched
- Button width is automatic; there is currently no per-item fixed-width field
- Width calculation uses the actual toolbar control height so icon width reservation matches runtime drawing more closely
- When `dropDown = true`, `popupMenu` must be a valid application-owned `HMENU`
- `alignRight = true` is useful for secondary tools or far-edge actions
- The same control can be used as a menu-bar style surface
- `ToolbarVariant::Dense` is the preferred preset for compact utility strips and menu-bar-adjacent tool rows
- `ToolbarVariant::Accent` is the preferred preset when the active command group should read as the primary top action bar

## Drop-Down and Overflow Behavior

- Drop-down menus are rendered through a custom dark popup instead of the native light menu UI
- When space becomes insufficient, hidden items move into an overflow button
- Overflow tries to preserve drop-down hierarchy instead of flattening every submenu entry

## Demo Reference

For complete examples, see:

- `../demo/src/demo_toolbar.cpp`
- `../demo/src/demo_toolbar_menubar.cpp`
- `../demo/src/demo_showcase.cpp`

`demo_toolbar.cpp` currently shows:

- The original toolbar layout with separators and right-aligned items
- A comparison toolbar with the same actions but without separators or `alignRight`
- A slider that changes `theme.uiFont.height`
- A slider that changes `theme.toolbarHeight`
- On-screen debug text for icon extent, measured text width, and button width calculations

## Current Limitations

- No full keyboard navigation model yet
- No drag-based toolbar customization
- No built-in icon resource management

