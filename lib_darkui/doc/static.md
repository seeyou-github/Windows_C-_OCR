# Static

## Overview

`darkui::Static` is a dark presentation control for Win32 that can render text, an icon, or a bitmap inside a themed rectangular area.

## Files

- `include/darkui/static.h`
- `src/static.cpp`

## Suitable Scenarios

- Section titles and helper text in dark panels
- Status icons or product glyphs
- Bitmap previews or decorative badges

## Main Features

- Dark background and themed text rendering
- Text, icon, and bitmap display modes
- Semantic text variants for titles, body text, muted text, and panel text
- Configurable text alignment and ellipsis behavior
- Keeps normal `STATIC` window usage and optional `SS_NOTIFY`

## Recommended Usage

```cpp
#include "darkui/static.h"

darkui::Theme theme;
theme.staticBackground = RGB(24, 27, 31);
theme.staticText = RGB(236, 239, 244);

darkui::Panel card;
darkui::Panel::Options cardOptions;
cardOptions.cornerRadius = 20;
card.Create(hwnd, 1200, theme, cardOptions);

darkui::Static title;
darkui::Static::Options options;
options.text = L"Repository Overview";
options.variant = darkui::StaticVariant::PanelTitle;

title.Create(card.hwnd(), 1201, theme, options);

darkui::ThemedWindowHost host;
darkui::ThemedWindowHost::Options hostOptions;
hostOptions.theme = theme;
host.Attach(hwnd, hostOptions);
auto& themeManager = host.theme_manager();
themeManager.Bind(card, title);
themeManager.Apply();

MoveWindow(card.hwnd(), 20, 20, 300, 120, TRUE);
MoveWindow(title.hwnd(), 20, 20, 260, 36, TRUE);
```

## Icon And Bitmap Modes

```cpp
title.SetIcon(LoadIconW(nullptr, IDI_INFORMATION));
title.SetBitmap(previewBitmap);
```

Notes:

- `SetIcon()` and `SetBitmap()` do not take ownership of the passed handles
- Call `SetText()` or `ClearImage()` to return to text mode

## Common API

### `Create`

```cpp
bool Create(HWND parent, int controlId, const Theme& theme, const Static::Options& options);
```

### `SetText` / `GetText`

```cpp
title.SetText(L"Queued jobs");
std::wstring text = title.GetText();
```

### `SetIcon`

```cpp
title.SetIcon(LoadIconW(nullptr, IDI_INFORMATION));
```

### `SetBitmap`

```cpp
title.SetBitmap(bitmapHandle);
```

### `Options`

```cpp
darkui::Static::Options options;
options.variant = darkui::StaticVariant::Title;
```

Available variants:

```cpp
darkui::StaticVariant::Default
darkui::StaticVariant::Title
darkui::StaticVariant::Body
darkui::StaticVariant::Muted
darkui::StaticVariant::PanelTitle
darkui::StaticVariant::PanelBody
```

### `SetTextFormat`

```cpp
title.SetTextFormat(DT_CENTER | DT_SINGLELINE);
```

## Theme Fields Used

- `staticBackground`
- `staticText`
- `textPadding`
- `uiFont`

## Usage Notes

- Use `DT_SINGLELINE` when you want centered title text
- Omit `DT_SINGLELINE` when you want wrapped paragraph-style text
- Prefer `variant` for common text hierarchy and only override `textFormat` for one-off alignment needs
- For icons and bitmaps, the control centers the image inside the client area

## Demo Reference

For a complete example, see:

- `../demo/src/demo_static.cpp`

## Current Limitations

- No automatic image scaling
- No alpha-composited bitmap pipeline
- No built-in hyperlink behavior
