# ProgressBar

## Overview

`darkui::ProgressBar` is a custom dark progress bar. It separates host background, outer shell, inner track, fill, and percentage text so the control fits naturally into dark cards and panels.

## Files

- `include/darkui/progress.h`
- `src/progress.cpp`

## Suitable Scenarios

- Download progress
- Task execution status
- Stats bars inside dark cards or dashboards

## Main Features

- Configurable logical range
- Configurable current value
- Optional percentage text
- Host surface color support

## Recommended Usage

```cpp
#include "darkui/progress.h"

darkui::Theme theme;
theme.progressBackground = RGB(49, 54, 61);
theme.progressTrack = RGB(36, 40, 46);
theme.progressFill = RGB(78, 120, 184);
theme.progressText = RGB(240, 244, 248);
theme.progressHeight = 18;

darkui::ProgressBar progress;
darkui::ProgressBar::Options options;
options.minimum = 0;
options.maximum = 100;
options.value = 64;
options.variant = darkui::ProgressVariant::Emphasis;

darkui::Panel card;
darkui::Panel::Options cardOptions;
card.Create(hwnd, 5000, theme, cardOptions);

progress.Create(card.hwnd(), 5001, theme, options);

darkui::ThemedWindowHost host;
darkui::ThemedWindowHost::Options hostOptions;
hostOptions.theme = theme;
host.Attach(hwnd, hostOptions);
auto& themeManager = host.theme_manager();
themeManager.Bind(card, progress);
themeManager.Apply();

MoveWindow(card.hwnd(), 20, 20, 340, 90, TRUE);
MoveWindow(progress.hwnd(), 20, 20, 300, 40, TRUE);
```

## Common API

### `SetRange`

```cpp
progress.SetRange(0, 100);
```

### `SetValue`

```cpp
progress.SetValue(72);
```

### `SetShowPercentage`

```cpp
progress.SetShowPercentage(true);
```

### `Options`

```cpp
darkui::ProgressBar::Options options;
options.minimum = 0;
options.maximum = 100;
options.variant = darkui::ProgressVariant::Panel;
```

### `variant`

Prefer semantic presets before hand-tuning low-level track geometry:

```cpp
options.variant = darkui::ProgressVariant::Default;
options.variant = darkui::ProgressVariant::Panel;
options.variant = darkui::ProgressVariant::Emphasis;
```

### `ThemedWindowHost \+ ThemeManager`

```cpp
darkui::ThemedWindowHost host;
darkui::ThemedWindowHost::Options hostOptions;
hostOptions.theme = theme;
host.Attach(hwnd, hostOptions);
auto& themeManager = host.theme_manager();
themeManager.Bind(progress);
themeManager.Apply();
```

### Button companions

When the page also uses action buttons near the progress bar, prefer semantic button variants:

```cpp
darkui::Button::Options decreaseOptions;
decreaseOptions.text = L"-10";
decreaseOptions.variant = darkui::ButtonVariant::Secondary;

darkui::Button::Options toggleOptions;
toggleOptions.text = L"Toggle Percent";
toggleOptions.variant = darkui::ButtonVariant::Primary;
```

## Reading State

```cpp
int value = progress.GetValue();
int minValue = progress.GetMinimum();
int maxValue = progress.GetMaximum();
bool showText = progress.show_percentage();
```

## Theme Fields Used

- `progressBackground`
- `progressTrack`
- `progressFill`
- `progressText`
- `progressHeight`
- `uiFont`
- `background`

## Usage Notes

- When the control sits on a custom card, prefer parenting it to `darkui::Panel` and keeping `surfaceRole = Auto`
- `ProgressVariant::Panel` is the preferred preset for dashboard cards and grouped status panels
- `ProgressVariant::Emphasis` is the preferred preset when one progress bar should read as the primary metric in a view
- Changing the logical range automatically clamps the current value

## Demo Reference

For complete examples, see:

- `../demo/src/demo_progress.cpp`
- `../demo/src/demo_showcase.cpp`

## Current Limitations

- Horizontal mode only
- No indeterminate animation
- No gradient fill
- No parent notification flow

