# Tab

## Overview

`darkui::Tab` is a custom dark tab control. It draws its own tab strip and allows application code to attach page windows to each tab item.

## Files

- `include/darkui/tab.h`
- `src/tab.cpp`

## Suitable Scenarios

- Settings windows
- Multi-page work areas
- Dark admin or dashboard interfaces

## Main Features

- Horizontal or vertical tab layout
- Dynamic tab item management
- Attached child-page windows
- Standard `WM_NOTIFY + TCN_SELCHANGE` notification flow

## Core Type

```cpp
struct TabItem {
    std::wstring text;
    std::uintptr_t userData = 0;
};
```

## Recommended Usage

```cpp
#include "darkui/tab.h"

darkui::Theme theme;
theme.tabBackground = RGB(24, 27, 31);
theme.tabItem = RGB(48, 53, 60);
theme.tabItemActive = RGB(78, 120, 184);
theme.tabText = RGB(206, 211, 218);
theme.tabTextActive = RGB(245, 247, 250);
theme.tabHeight = 38;

darkui::Panel card;
darkui::Panel::Options cardOptions;
cardOptions.cornerRadius = 20;
card.Create(hwnd, 7000, theme, cardOptions);

darkui::Tab tab;
darkui::Tab::Options options;
options.items = {
    {L"Overview", 1},
    {L"Metrics", 2},
    {L"Notes", 3},
};
options.selection = 0;
options.variant = darkui::TabVariant::Accent;

tab.Create(card.hwnd(), 7001, theme, options);

darkui::ThemedWindowHost host;
darkui::ThemedWindowHost::Options hostOptions;
hostOptions.theme = theme;
host.Attach(hwnd, hostOptions);
auto& themeManager = host.theme_manager();
themeManager.Bind(card, tab);
themeManager.Apply();

MoveWindow(card.hwnd(), 20, 20, 680, 460, TRUE);
MoveWindow(tab.hwnd(), 20, 20, 640, 420, TRUE);
```

## Attaching Pages

```cpp
HWND pageOne = CreateWindowExW(0, L"STATIC", L"Overview", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, tab.hwnd(), nullptr, instance, nullptr);
HWND pageTwo = CreateWindowExW(0, L"STATIC", L"Metrics", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, tab.hwnd(), nullptr, instance, nullptr);
HWND pageThree = CreateWindowExW(0, L"STATIC", L"Notes", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, tab.hwnd(), nullptr, instance, nullptr);

tab.AttachPage(0, pageOne);
tab.AttachPage(1, pageTwo);
tab.AttachPage(2, pageThree);
tab.SetSelection(0);
```

## Parent Message Handling

```cpp
case WM_NOTIFY: {
    auto* hdr = reinterpret_cast<NMHDR*>(lParam);
    if (hdr && hdr->hwndFrom == tab.hwnd() && hdr->code == TCN_SELCHANGE) {
        int index = tab.GetSelection();
        return 0;
    }
    break;
}
```

## Common API

### `SetVertical`

```cpp
tab.SetVertical(true);
```

### `SetItems` / `AddItem` / `ClearItems`

```cpp
tab.SetItems({{L"A", 1}, {L"B", 2}});
tab.AddItem({L"C", 3});
tab.ClearItems();
```

### `AttachPage`

```cpp
tab.AttachPage(0, pageOne);
```

### `SetSelection` / `GetSelection`

```cpp
tab.SetSelection(1, true);
int index = tab.GetSelection();
```

### `GetContentRect`

```cpp
RECT contentRect = tab.GetContentRect();
```

### `variant`

Prefer semantic presets before hand-tuning strip emphasis:

```cpp
options.variant = darkui::TabVariant::Default;
options.variant = darkui::TabVariant::Panel;
options.variant = darkui::TabVariant::Accent;
```

### `ThemedWindowHost \+ ThemeManager`

```cpp
darkui::ThemedWindowHost host;
darkui::ThemedWindowHost::Options hostOptions;
hostOptions.theme = theme;
host.Attach(hwnd, hostOptions);
auto& themeManager = host.theme_manager();
themeManager.Bind(tab);
themeManager.Apply();
```

## Theme Fields Used

- `tabBackground`
- `tabItem`
- `tabItemActive`
- `tabText`
- `tabTextActive`
- `tabHeight`
- `tabWidth`
- `background`
- `textPadding`
- `uiFont`

## Usage Notes

- Page creation and page layout stay in application code
- When the tab area sits on a shared card surface, prefer parenting it to `darkui::Panel`
- Vertical layout works well for left navigation
- Horizontal layout works well for classic tabbed windows
- `TabVariant::Panel` is the preferred preset when the whole tab control lives inside a card surface
- `TabVariant::Accent` is the preferred preset when the selected tab should carry stronger navigation emphasis

## Demo Reference

For complete examples, see:

- `../demo/src/demo_tab.cpp`
- `../demo/src/demo_showcase.cpp`
- `../demo/src/demo_toolbar.cpp`
- `../demo/src/demo_toolbar_menubar.cpp`

## Current Limitations

- No close buttons
- No drag reordering
- No icon tabs
- No disabled tabs

