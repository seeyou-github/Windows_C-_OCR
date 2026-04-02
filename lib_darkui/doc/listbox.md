# ListBox

## Overview

`darkui::ListBox` is a dark list box built from a themed host window plus an inner native `LISTBOX`. It keeps native keyboard navigation, scrolling, and selection behavior while removing the default light border and light item colors.

## Files

- `include/darkui/listbox.h`
- `src/listbox.cpp`

## Suitable Scenarios

- Option lists in dark settings panels
- Log category filters
- Single-select or multi-select lists in tools and inspectors

## Main Features

- Rounded dark host surface
- Native list box keyboard and scroll behavior
- Owner-drawn dark item rendering
- Semantic field variants for default, panel, and dense forms
- Supports both single-selection and multi-selection styles

## Recommended Usage

```cpp
#include "darkui/listbox.h"

darkui::Theme theme;
theme.listBoxBackground = RGB(18, 20, 24);
theme.listBoxPanel = RGB(38, 42, 48);
theme.listBoxText = RGB(232, 236, 241);
theme.listBoxItemSelected = RGB(78, 120, 184);

darkui::Panel card;
darkui::Panel::Options cardOptions;
cardOptions.cornerRadius = 18;
card.Create(hwnd, 2200, theme, cardOptions);

darkui::ListBox listBox;
darkui::ListBox::Options options;
options.variant = darkui::FieldVariant::Panel;
options.items = {
    {L"All projects", 1},
    {L"Queued exports", 2},
    {L"Archived snapshots", 3}
};
options.selection = 0;

listBox.Create(card.hwnd(), 2201, theme, options);

darkui::ThemedWindowHost host;
darkui::ThemedWindowHost::Options hostOptions;
hostOptions.theme = theme;
host.Attach(hwnd, hostOptions);
auto& themeManager = host.theme_manager();
themeManager.Bind(card, listBox);
themeManager.Apply();

MoveWindow(card.hwnd(), 20, 20, 320, 220, TRUE);
MoveWindow(listBox.hwnd(), 20, 20, 280, 180, TRUE);
```

## Multi-Selection Example

```cpp
darkui::ListBox multiList;
darkui::ListBox::Options multiOptions;
multiOptions.variant = darkui::FieldVariant::Dense;
multiOptions.style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_VSCROLL | LBS_EXTENDEDSEL;
multiList.Create(card.hwnd(), 2202, theme, multiOptions);
```

## Parent Message Handling

```cpp
case WM_COMMAND:
    if (LOWORD(wParam) == 2201 && HIWORD(wParam) == LBN_SELCHANGE) {
        int index = listBox.GetSelection();
        return 0;
    }
    break;
```

## Common API

### `Create`

```cpp
bool Create(HWND parent, int controlId, const Theme& theme, const ListBox::Options& options);
```

### `SetItems` / `AddItem` / `ClearItems`

```cpp
listBox.SetItems({{L"One", 1}, {L"Two", 2}});
listBox.AddItem({L"Three", 3});
listBox.ClearItems();
```

### `GetSelection` / `GetSelections`

```cpp
int selected = listBox.GetSelection();
std::vector<int> selectedIndices = listBox.GetSelections();
```

### `SetSelection`

```cpp
listBox.SetSelection(2, true);
```

### `SetCornerRadius`

```cpp
listBox.SetCornerRadius(14);
```

### `FieldVariant`

```cpp
darkui::FieldVariant::Default
darkui::FieldVariant::Panel
darkui::FieldVariant::Dense
```

## Theme Fields Used

- `listBoxBackground`
- `listBoxPanel`
- `listBoxText`
- `listBoxItemSelected`
- `listBoxItemSelectedText`
- `border`
- `textPadding`
- `listBoxItemHeight`
- `uiFont`

## Usage Notes

- Move and size the host returned by `listBox.hwnd()`
- When the list box sits on a grouped card, prefer parenting it to `darkui::Panel`
- Prefer `variant` for standard field styles and only override `cornerRadius` when you need a one-off shape
- Use `listBox.list_hwnd()` when you need direct access to the inner native `LISTBOX`
- For multi-select mode, prefer `GetSelections()` instead of `GetSelection()`

## Demo Reference

For a complete example, see:

- `../demo/src/demo_listbox.cpp`

## Current Limitations

- No per-item accent color yet
- No drag reordering
- No custom column layout
