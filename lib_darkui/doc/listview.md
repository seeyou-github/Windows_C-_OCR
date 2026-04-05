# ListView

`darkui::ListView` is a native Win32 `SysListView32` wrapper with a dark header and restrained dark report styling. It keeps native scrolling, native selection visuals, column resizing, and keyboard behavior while adding a dark header and dark grid overlay similar to the reference Android log viewer project.

## Files

- `include/darkui/listview.h`
- `src/listview.cpp`
- `doc/native-dark-scrollbar.md`

## Good Fit

- Dark log viewers, result tables, and data panes
- Pages that want native column resize and native scrollbars
- Pages that want native multi-select plus a lightweight copy/context-menu layer
- Cases where a restrained dark wrapper is preferred over a fully custom control

## Characteristics

- Native `SysListView32`
- Dark `DarkMode_Explorer` theme setup
- Custom-painted header
- Native column resize, native scrollbars, native keyboard navigation
- Native selection visuals are preserved
- Dark grid overlay that tracks runtime header resizing
- `Ctrl+C` support for selected rows
- Right-click menu with `Copy` and `Select All`
- Runtime theme updates through `SetTheme(...)` and `ThemeManager`

## Basic Example

```cpp
#include "darkui/listview.h"

darkui::ListView listView;
darkui::ListView::Options options;
options.columns = {
    {L"Time", 180},
    {L"Level", 80},
    {L"Tag", 160},
    {L"Message", 520},
};
options.rows = {
    {L"12:30:11.120", L"Info", L"Renderer", L"Pipeline ready"},
    {L"12:30:11.422", L"Warn", L"Network", L"Reconnect scheduled"},
};

listView.Create(hwnd, 9001, darkui::MakePresetTheme(), options);
MoveWindow(listView.hwnd(), 24, 24, 720, 320, TRUE);
```

## Notes

- `hwnd()` returns the outer host window.
- `list_hwnd()` returns the inner native `SysListView32`.
- `header_hwnd()` returns the native header control.
- Column widths are initialized in pixels, but the user can still resize them at runtime.
- The wrapper syncs live native header widths back into its dark grid overlay.
- `SetSelection(...)` is a convenience single-row setter; the underlying control still supports multi-select.
- Row copy uses tab-separated columns and CRLF-separated rows.

## Related Guide

- Native dark scrollbar guide: [native-dark-scrollbar.md](D:/Code/Windows_C++_lib_darkui/doc/native-dark-scrollbar.md)
