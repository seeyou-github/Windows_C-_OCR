# Panel

## Overview

`darkui::Panel` is a lightweight themed container for card-style sections. It draws its own dark surface and border, and child controls created under `panel.hwnd()` can inherit that surface automatically when their `surfaceRole` stays at the default `Auto`.

## Files

- `include/darkui/panel.h`
- `src/panel.cpp`

## Suitable Scenarios

- Card sections inside dashboards and settings pages
- Dialog body groups that should share one background surface
- Reducing repeated `surfaceRole = darkui::SurfaceRole::Panel` assignments

## Main Features

- Rounded dark panel background and border
- Surface-role inheritance for child controls
- Normal Win32 parent-child window structure
- Runtime theme updates through `ThemeManager`

## Recommended Usage

```cpp
#include "darkui/darkui.h"

darkui::ThemedWindowHost host;
darkui::ThemedWindowHost::Options hostOptions;
hostOptions.theme = darkui::MakePresetTheme(darkui::ThemePreset::Graphite);
host.Attach(hwnd, hostOptions);

darkui::Panel card;
darkui::Panel::Options cardOptions;
cardOptions.role = darkui::SurfaceRole::Panel;
cardOptions.cornerRadius = 22;
card.Create(hwnd, 1100, host.theme(), cardOptions);

darkui::Button button;
darkui::Button::Options buttonOptions;
buttonOptions.text = L"Apply";
buttonOptions.variant = darkui::ButtonVariant::Primary;

button.Create(card.hwnd(), 1101, host.theme(), buttonOptions);

host.theme_manager().Bind(card, button);
host.theme_manager().Apply();

MoveWindow(card.hwnd(), 20, 20, 320, 180, TRUE);
MoveWindow(button.hwnd(), 20, 20, 140, 38, TRUE);
```

## Common API

### `Create`

```cpp
bool Create(HWND parent, int controlId, const Theme& theme, const Panel::Options& options);
```

### `Options`

```cpp
darkui::Panel::Options options;
options.role = darkui::SurfaceRole::Panel;
options.cornerRadius = 24;
```

### `surface_role`

```cpp
darkui::SurfaceRole role = card.surface_role();
```

### `SetCornerRadius`

```cpp
card.SetCornerRadius(28);
```

## Theme Fields Used

- `background`
- `panel`
- `border`

## Usage Notes

- The default `Panel::Options::role` is already `SurfaceRole::Panel`
- Child controls created under `panel.hwnd()` can usually keep their default `surfaceRole = Auto`
- Prefer child control `variant` presets before manually tuning radius or low-level styling
- Bind the panel itself into `ThemeManager` so its background updates with the rest of the page

## Demo Reference

For complete examples, see:

- `../demo/src/demo_showcase.cpp`
- `../demo/src/demo_static.cpp`
- `../demo/src/demo_dialog.cpp`
