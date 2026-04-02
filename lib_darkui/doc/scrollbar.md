# ScrollBar

## Overview

`darkui::ScrollBar` is a custom dark scrollbar that supports both horizontal and vertical modes while preserving standard Win32 scroll notification behavior.

## Files

- `include/darkui/scrollbar.h`
- `src/scrollbar.cpp`

## Suitable Scenarios

- Scrolling custom-drawn content
- Dark side panels
- Scrollable regions paired with custom controls or custom canvases

## Main Features

- Horizontal and vertical modes
- Custom logical range
- Configurable page size
- Thumb dragging
- Track click paging
- Keyboard arrows, `Home`, and `End`

## Recommended Usage

```cpp
#include "darkui/scrollbar.h"

darkui::Theme theme;
theme.scrollBarBackground = RGB(24, 27, 31);
theme.scrollBarTrack = RGB(48, 53, 60);
theme.scrollBarThumb = RGB(120, 128, 140);
theme.scrollBarThumbHot = RGB(160, 170, 184);
theme.scrollBarThickness = 14;
theme.scrollBarMinThumbSize = 28;

darkui::ScrollBar verticalBar;
darkui::ScrollBar::Options options;
options.vertical = true;
options.minimum = 0;
options.maximum = 100;
options.pageSize = 24;
options.value = 56;

verticalBar.Create(hwnd, 6001, theme, options);

darkui::ThemedWindowHost host;
darkui::ThemedWindowHost::Options hostOptions;
hostOptions.theme = theme;
host.Attach(hwnd, hostOptions);
auto& themeManager = host.theme_manager();
themeManager.Bind(verticalBar);
themeManager.Apply();

MoveWindow(verticalBar.hwnd(), 20, 20, 16, 220, TRUE);
```

## Parent Message Handling

### Vertical

```cpp
case WM_VSCROLL:
    if (reinterpret_cast<HWND>(lParam) == verticalBar.hwnd()) {
        int value = verticalBar.GetValue();
        return 0;
    }
    break;
```

### Horizontal

```cpp
case WM_HSCROLL:
    if (reinterpret_cast<HWND>(lParam) == horizontalBar.hwnd()) {
        int value = horizontalBar.GetValue();
        return 0;
    }
    break;
```

## Common API

### `SetRange`

```cpp
scrollBar.SetRange(0, 100);
```

### `SetPageSize`

```cpp
scrollBar.SetPageSize(20);
```

### `SetValue`

```cpp
scrollBar.SetValue(40);
scrollBar.SetValue(48, true);
```

### `ThemedWindowHost \+ ThemeManager`

```cpp
darkui::ThemedWindowHost host;
darkui::ThemedWindowHost::Options hostOptions;
hostOptions.theme = theme;
host.Attach(hwnd, hostOptions);
auto& themeManager = host.theme_manager();
themeManager.Bind(scrollBar);
themeManager.Apply();
```

## Reading State

```cpp
int value = scrollBar.GetValue();
int minValue = scrollBar.GetMinimum();
int maxValue = scrollBar.GetMaximum();
int pageSize = scrollBar.GetPageSize();
bool isVertical = scrollBar.vertical();
```

## Theme Fields Used

- `scrollBarBackground`
- `scrollBarTrack`
- `scrollBarThumb`
- `scrollBarThumbHot`
- `scrollBarThickness`
- `scrollBarMinThumbSize`

## Usage Notes

- The control only represents the scrollbar itself
- The parent window is responsible for applying the resulting content offset
- `pageSize` affects both thumb size and reachable scroll range

## Demo Reference

For complete examples, see:

- `../demo/src/demo_scrollbar.cpp`
- `../demo/src/demo_showcase.cpp`
- `../demo/src/demo_tab.cpp`
- `../demo/src/demo_toolbar.cpp`
- `../demo/src/demo_toolbar_menubar.cpp`

## Current Limitations

- No arrow buttons
- No auto-repeat on hold
- No scroll-container helper

