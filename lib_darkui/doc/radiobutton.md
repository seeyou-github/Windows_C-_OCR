# RadioButton

## Overview

`darkui::RadioButton` is a dark owner-drawn radio button for Win32. It preserves native auto-radio grouping while replacing the light system glyph with a themed dark circle and selection dot.

## Files

- `include/darkui/radiobutton.h`
- `src/radiobutton.cpp`

## Suitable Scenarios

- Mutually exclusive display modes
- Theme or profile selection panels
- Compact settings groups in dark tools

## Main Features

- Dark circle, dot, and text rendering
- Hover and disabled states
- Semantic selection variants for default, panel, and accent emphasis
- Keeps normal `WM_COMMAND + BN_CLICKED`
- Uses native auto-radio grouping semantics

## Recommended Usage

```cpp
#include "darkui/radiobutton.h"

darkui::Theme theme;
theme.radioBackground = RGB(43, 47, 54);
theme.radioBackgroundHot = RGB(55, 60, 68);
theme.radioAccent = RGB(82, 132, 204);
theme.radioText = RGB(236, 239, 244);

darkui::RadioButton compact;
darkui::RadioButton detailed;
darkui::Panel card;
darkui::Panel::Options cardOptions;
card.Create(hwnd, 4400, theme, cardOptions);
darkui::RadioButton::Options compactOptions;
compactOptions.text = L"Compact view";
compactOptions.checked = true;
compactOptions.style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_GROUP;
compactOptions.variant = darkui::SelectionVariant::Accent;
darkui::RadioButton::Options detailedOptions;
detailedOptions.text = L"Detailed view";
detailedOptions.variant = darkui::SelectionVariant::Panel;

compact.Create(card.hwnd(), 4401, theme, compactOptions);
detailed.Create(card.hwnd(), 4402, theme, detailedOptions);

darkui::ThemedWindowHost host;
darkui::ThemedWindowHost::Options hostOptions;
hostOptions.theme = theme;
host.Attach(hwnd, hostOptions);
auto& themeManager = host.theme_manager();
themeManager.Bind(card, compact, detailed);
themeManager.Apply();

MoveWindow(card.hwnd(), 20, 20, 300, 110, TRUE);
MoveWindow(compact.hwnd(), 20, 20, 260, 28, TRUE);
MoveWindow(detailed.hwnd(), 20, 56, 260, 28, TRUE);
```

## Parent Message Handling

```cpp
case WM_COMMAND:
    if (HIWORD(wParam) == BN_CLICKED) {
        if (LOWORD(wParam) == 4401 || LOWORD(wParam) == 4402) {
            return 0;
        }
    }
    break;
```

## Common API

### `Create`

```cpp
bool Create(HWND parent, int controlId, const Theme& theme, const RadioButton::Options& options);
```

### `SetChecked` / `GetChecked`

```cpp
compact.SetChecked(true);
bool selected = compact.GetChecked();
```

### `Options`

```cpp
darkui::RadioButton::Options options;
options.checked = true;
options.variant = darkui::SelectionVariant::Default;
```

Available variants:

```cpp
darkui::SelectionVariant::Default
darkui::SelectionVariant::Panel
darkui::SelectionVariant::Accent
```

## Theme Fields Used

- `radioBackground`
- `radioBackgroundHot`
- `radioAccent`
- `radioBorder`
- `radioText`
- `radioDisabledText`
- `uiFont`
- `background`

## Usage Notes

- Use `WS_GROUP` on the first radio button in a group
- Native auto-radio behavior clears the previously selected sibling
- Prefer parenting the radio buttons to `darkui::Panel` when they sit on a shared card surface
- Prefer `variant` for standard selection emphasis and only override surface values for one-off cases

## Demo Reference

For a complete example, see:

- `../demo/src/demo_radiobutton.cpp`

## Current Limitations

- No custom group container helper
- No per-item subtitle support
