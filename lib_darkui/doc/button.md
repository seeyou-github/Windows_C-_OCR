# Button

## Overview

`darkui::Button` is a custom dark owner-draw button for Win32. It preserves the standard Win32 button notification flow while giving application code control over semantic button roles, rounded geometry, and host surface blending.

## Files

- `include/darkui/button.h`
- `src/button.cpp`

## Suitable Scenarios

- Primary and secondary buttons in dark windows
- Rounded buttons placed on cards or panels
- Win32 applications that still want standard `BN_CLICKED` behavior

## Main Features

- Normal, hover, pressed, and disabled states
- Semantic variants for common button roles
- Custom border and text colors
- Configurable corner radius
- Host surface color support to avoid visible seams around rounded corners

## Recommended Usage

```cpp
#include "darkui/button.h"

darkui::Theme theme;
theme.button = RGB(52, 61, 72);
theme.buttonHover = RGB(64, 75, 88);
theme.buttonHot = RGB(72, 86, 104);
theme.buttonDisabled = RGB(58, 62, 70);
theme.buttonDisabledText = RGB(138, 144, 152);
theme.border = RGB(84, 96, 112);
theme.text = RGB(232, 236, 241);

darkui::Button button;
darkui::Button::Options options;
options.text = L"Run";
options.variant = darkui::ButtonVariant::Secondary;

darkui::Panel card;
darkui::Panel::Options cardOptions;
card.Create(hwnd, 3000, theme, cardOptions);

button.Create(card.hwnd(), 3001, theme, options);

darkui::ThemedWindowHost host;
darkui::ThemedWindowHost::Options hostOptions;
hostOptions.theme = theme;
host.Attach(hwnd, hostOptions);
auto& themeManager = host.theme_manager();
themeManager.Bind(card, button);
themeManager.Apply();

MoveWindow(card.hwnd(), 20, 20, 220, 90, TRUE);
MoveWindow(button.hwnd(), 20, 20, 140, 40, TRUE);
```

## Parent Message Handling

```cpp
case WM_COMMAND:
    if (LOWORD(wParam) == 3001 && HIWORD(wParam) == BN_CLICKED) {
        MessageBoxW(hwnd, L"clicked", L"info", MB_OK);
        return 0;
    }
    break;
```

## Common API

### `Create`

```cpp
bool Create(HWND parent, int controlId, const Theme& theme, const Button::Options& options);
```

### `Options`

```cpp
darkui::Button::Options options;
options.text = L"Run";
options.variant = darkui::ButtonVariant::Primary;
```

Available variants:

```cpp
darkui::ButtonVariant::Primary
darkui::ButtonVariant::Secondary
darkui::ButtonVariant::Subtle
darkui::ButtonVariant::Ghost
darkui::ButtonVariant::Danger
```

### `SetText` / `GetText`

```cpp
button.SetText(L"Processing...");
std::wstring text = button.GetText();
```

### `SetCornerRadius`

```cpp
button.SetCornerRadius(18);
```

### `ThemedWindowHost \+ ThemeManager`

```cpp
darkui::ThemedWindowHost host;
darkui::ThemedWindowHost::Options hostOptions;
hostOptions.theme = theme;
host.Attach(hwnd, hostOptions);
auto& themeManager = host.theme_manager();
themeManager.Bind(button);
themeManager.Apply();
```

Purpose:

- Sets the background color behind the rounded button corners
- Recommended when the button sits on a card or panel instead of the main window background

## Theme Fields Used

- `button`
- `buttonHover`
- `buttonHot`
- `buttonDisabled`
- `buttonDisabledText`
- `border`
- `text`
- `textPadding`
- `uiFont`
- `background`

## Usage Notes

- For plain window backgrounds, the default surface fallback is usually enough
- For cards and panels, prefer parenting the button to `darkui::Panel` and keeping `surfaceRole = Auto`
- Prefer `variant` for common roles and only override `cornerRadius` or `surfaceColor` when the preset is not enough
- Use `ThemedWindowHost + theme_manager()` when the same page will switch themes repeatedly
- To disable the control, use `EnableWindow(button.hwnd(), FALSE)`

## Demo Reference

For complete examples, see:

- `../demo/src/demo_button.cpp`
- `../demo/src/demo_edit.cpp`
- `../demo/src/demo_progress.cpp`
- `../demo/src/demo_showcase.cpp`
- `../demo/src/demo_slider.cpp`

## Current Limitations

- No built-in icon support
- No drop-down arrow support
- No default-button glow behavior
- No button-group management

