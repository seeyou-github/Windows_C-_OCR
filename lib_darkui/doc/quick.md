# Quick Helpers

## Overview

`include/darkui/quick.h` adds a thin convenience layer on top of the existing control wrappers. It does not replace the normal Win32-style API. Instead, it reduces repeated setup code for the most common tasks:

- creating a ready-made semantic theme
- picking a built-in dark theme preset
- creating card/panel containers with inherited surfaces
- binding one theme to multiple controls
- opening a dark message dialog with one function call

This helper layer is the recommended companion for the new per-control `Options` structs and for `ThemedWindowHost`.

- `Button::Options`
- `CheckBox::Options`
- `ComboBox::Options`
- `Dialog::Options`
- `Edit::Options`
- `ListBox::Options`
- `ProgressBar::Options`
- `RadioButton::Options`
- `ScrollBar::Options`
- `Slider::Options`
- `Static::Options`
- `Tab::Options`
- `Table::Options`
- `Toolbar::Options`

## Files

- `include/darkui/quick.h`

## Theme Presets

Use `ThemePreset` when you want a ready-to-use palette without filling a long `Theme` manually.

```cpp
#include "darkui/darkui.h"

darkui::Theme theme = darkui::MakePresetTheme(darkui::ThemePreset::Graphite);
```

## Options-Based Control Creation

```cpp
darkui::Button button;
darkui::Button::Options options;
options.text = L"Apply";
options.variant = darkui::ButtonVariant::Primary;

darkui::Panel card;
darkui::Panel::Options cardOptions;
card.Create(hwnd, 4100, theme, cardOptions);

button.Create(card.hwnd(), 4101, theme, options);
```

If you need an explicit color, `surfaceColor` still overrides `surfaceRole`.
If several controls share one card surface, prefer `darkui::Panel` and let children inherit it.
For common styles, prefer semantic variants before hand-tuning radius or density:

- `ButtonVariant::Primary / Secondary / Subtle / Ghost / Danger`
- `FieldVariant::Default / Panel / Dense`
- `StaticVariant::Title / Body / Muted / PanelTitle / PanelBody`
- `SelectionVariant::Default / Panel / Accent`
- `ProgressVariant::Default / Panel / Emphasis`
- `SliderVariant::Default / Dense / Emphasis`
- `TabVariant::Default / Panel / Accent`
- `ToolbarVariant::Default / Dense / Accent`

## Panel And Surface Inheritance

```cpp
darkui::Panel card;
darkui::Panel::Options cardOptions;
card.Create(hwnd, 4200, theme, cardOptions);

darkui::Button button;
darkui::Button::Options buttonOptions;
buttonOptions.text = L"Apply";

button.Create(card.hwnd(), 4201, theme, buttonOptions);
```

With this structure, `buttonOptions.surfaceRole` can usually stay at the default `Auto`.

Available presets:

- `Graphite`
- `Ember`
- `Glacier`
- `Moss`
- `Mono`

## Compact Semantic Theme Creation

If you want a custom palette but still prefer semantic theme entry, use `MakeSemanticTheme(...)`:

```cpp
darkui::Theme theme = darkui::MakeSemanticTheme(
    RGB(20, 22, 26),
    RGB(32, 36, 42),
    RGB(228, 232, 238),
    RGB(248, 250, 252),
    RGB(82, 132, 204),
    RGB(48, 86, 148));
```

## ThemedWindowHost

Use `ThemedWindowHost` as the preferred top-level window theme shell:

```cpp
darkui::ThemedWindowHost host;
darkui::ThemedWindowHost::Options options;
options.theme = darkui::MakePresetTheme();
options.titleBarStyle = darkui::TitleBarStyle::Black;

host.Attach(hwnd, options);
```

Typical benefits:

- stores the current resolved theme for the page
- exposes `theme_manager()` for control binding
- rebuilds standard window title/body fonts
- owns the background brush for uniform dark fill
- applies a dark or theme-based title bar

## ThemedWindowHost + ThemeManager

Use `ThemedWindowHost` together with `theme_manager()` as the preferred repeated theme-switch entry:

```cpp
darkui::ThemedWindowHost host;
darkui::ThemedWindowHost::Options options;
options.theme = darkui::MakePresetTheme();
host.Attach(hwnd, options);

host.theme_manager().Bind(buttonA, buttonB, editA, listBoxA);
host.theme_manager().Apply();

host.ApplyTheme(darkui::MakePresetTheme(darkui::ThemePreset::Moss));
```

Use `ThemeBinder` when you only want the remembered binding list without storing the current theme separately.

## One-Shot Message Dialog

For a simple popup, you do not need to manually construct and configure `darkui::Dialog`.

```cpp
darkui::DialogOptions options;
options.title = L"Delete File";
options.message = L"Delete the selected file permanently?";
options.confirmText = L"Delete";
options.cancelText = L"Cancel";

darkui::Dialog::Result result =
    darkui::ShowMessageDialog(hwnd, 5001, theme, options);
```

## Confirm Dialog Shortcut

```cpp
darkui::Dialog::Result result = darkui::ShowConfirmDialog(
    hwnd,
    5002,
    theme,
    L"Publish Session",
    L"Apply the current settings to every workstation?",
    L"Publish",
    L"Cancel");
```

## Info Dialog Shortcut

Use `ShowInfoDialog(...)` when you only need one acknowledge button:

```cpp
darkui::ShowInfoDialog(hwnd, 5003, theme, L"Completed", L"Export finished.");
```

## Notes

- These helpers are additive. `Options + ThemedWindowHost + ThemeManager` is the intended caller-facing path.
- Theme distribution and layout are still separate responsibilities. `theme_manager()` only updates bound controls, while `ThemedWindowHost` manages top-level window resources.
- `ShowMessageDialog(...)` is intended for simple text dialogs. For custom forms, continue using `darkui::Dialog` directly.

