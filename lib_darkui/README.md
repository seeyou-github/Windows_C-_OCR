# Windows_C++_lib_darkui

`Windows_C++_lib_darkui` is a lightweight custom dark-control library for native Win32 applications. It does not try to replace a full GUI framework. Instead, it provides a focused set of dark-themed controls that can be embedded into existing Win32 projects while keeping the standard Win32 message model.

## Project Positioning

- Designed for native Win32 applications
- Suitable for source integration or static-library style reuse
- Keeps Win32-style control creation, message handling, and parent-child window structure
- Focuses on solving dark-theme consistency issues that are difficult to address with stock controls

## Available Custom Controls

- `Button`: dark owner-draw push button with standard click notifications. See [doc/button.md](doc/button.md)
- `CheckBox`: dark owner-draw checkbox with native-style checked behavior. See [doc/checkbox.md](doc/checkbox.md)
- `ComboBox`: dark combo box with custom popup host and list rendering. See [doc/combobox.md](doc/combobox.md)
- `Dialog`: modal dark popup dialog with confirm/cancel flow. See [doc/dialog.md](doc/dialog.md)
- `Edit`: dark host plus inner native `EDIT`, with placeholder, read-only mode, and multiline support. See [doc/edit.md](doc/edit.md)
- `ListBox`: dark list box wrapper with native selection and scrolling behavior. See [doc/listbox.md](doc/listbox.md)
- `ListView`: native dark `SysListView32` wrapper with dark header and restrained styling. See [doc/listview.md](doc/listview.md)
- `Panel`: dark card/container surface for grouping controls. See [doc/panel.md](doc/panel.md)
- `ProgressBar`: dark progress bar with custom track, fill, and text. See [doc/progress.md](doc/progress.md)
- `RadioButton`: dark owner-draw radio button with native grouping behavior. See [doc/radiobutton.md](doc/radiobutton.md)
- `Slider`: dark slider with custom track, thumb, ticks, and `WM_HSCROLL`. See [doc/slider.md](doc/slider.md)
- `Static`: dark static display control for text, icons, and bitmaps. See [doc/static.md](doc/static.md)
- `Tab`: dark tab control with attached child pages and `TCN_SELCHANGE`. See [doc/tab.md](doc/tab.md)
- `Toolbar`: dark toolbar with buttons, separators, drop-downs, overflow, and right-aligned items. See [doc/toolbar.md](doc/toolbar.md)

Native scrollbar theme note for native controls such as `Edit`, `ListBox`, and `ListView`: [doc/native-dark-scrollbar.md](doc/native-dark-scrollbar.md)

## Main Characteristics

- Built around a shared `darkui::Theme` structure
- Runtime theme switching is standardized around `ThemedWindowHost + ThemeManager`
- Supports a unified semantic theme entry through `Theme::useSemanticPalette`
- Keeps native Win32 usage patterns where practical
- Control layout remains the responsibility of the host window
- Suitable for gradually replacing stock light-themed controls in existing projects

## Unified Theme Entry

If you want every control to follow one palette and one typography system, enable semantic palette mode:

```cpp
darkui::Theme theme;
theme.useSemanticPalette = true;
theme.primaryBackground = RGB(20, 22, 26);
theme.secondaryBackground = RGB(32, 36, 42);
theme.primaryText = RGB(228, 232, 238);
theme.highlightText = RGB(248, 250, 252);
theme.accent = RGB(82, 132, 204);
theme.accentSecondary = RGB(48, 86, 148);
theme.fontFamily = L"Segoe UI";
theme.fontSize = 20;
theme.secondaryFontSize = 18;
```

Notes:

- In semantic mode, darkui expands those values into the detailed per-control colors internally
- Existing per-control theme fields remain available when `useSemanticPalette` is left `false`

## Quick Helpers

If the calling side should stay as short as possible, use the helper layer in `darkui/quick.h`.

## Options-Based Creation

Every custom control now provides an `Options` structure plus an overload shaped like:

```cpp
control.Create(parent, controlId, theme, options);
```

This is available for:

- `Button::Options`
- `CheckBox::Options`
- `ComboBox::Options`
- `Dialog::Options`
- `Edit::Options`
- `ListBox::Options`
- `ListView::Options`
- `Panel::Options`
- `ProgressBar::Options`
- `RadioButton::Options`
- `Slider::Options`
- `Static::Options`
- `Tab::Options`
- `Toolbar::Options`

Recommended example:

```cpp
darkui::ThemedWindowHost host;
darkui::ThemedWindowHost::Options hostOptions;
hostOptions.theme = darkui::MakePresetTheme(darkui::ThemePreset::Graphite);
hostOptions.titleBarStyle = darkui::TitleBarStyle::Black;

host.Attach(hwnd, hostOptions);

darkui::Button button;
darkui::Button::Options options;
options.text = L"Refresh";
options.variant = darkui::ButtonVariant::Secondary;

darkui::Panel card;
darkui::Panel::Options cardOptions;
card.Create(hwnd, 1000, host.theme(), cardOptions);

button.Create(card.hwnd(), 1001, host.theme(), options);
host.theme_manager().Bind(card, button);
host.theme_manager().Apply();
```

For the most common control styles, prefer semantic variants before hand-tuning low-level fields:

- `ButtonVariant::Primary / Secondary / Subtle / Ghost / Danger`
- `FieldVariant::Default / Panel / Dense` for `Edit`, `ComboBox`, and `ListBox`
- `StaticVariant::Title / Body / Muted / PanelTitle / PanelBody`
- `SelectionVariant::Default / Panel / Accent` for `CheckBox` and `RadioButton`
- `ProgressVariant::Default / Panel / Emphasis` for `ProgressBar`
- `SliderVariant::Default / Dense / Emphasis` for `Slider`
- `TabVariant::Default / Panel / Accent` for `Tab`
- `ToolbarVariant::Default / Dense / Accent` for `Toolbar`

Ready-made preset theme:

```cpp
darkui::Theme theme = darkui::MakePresetTheme(darkui::ThemePreset::Graphite);
```

Compact semantic theme creation:

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

Use `ThemedWindowHost` as the preferred top-level window shell. Together with `theme_manager()`, it is the unified caller-facing theme entry:

- stores the current resolved theme
- owns a page-level `ThemeManager`
- rebuilds the background brush and standard title/body fonts
- applies dark or theme-based title-bar colors
- gives the window a consistent background fill path

Basic pattern:

```cpp
darkui::ThemedWindowHost host;
darkui::ThemedWindowHost::Options options;
options.theme = darkui::MakePresetTheme();
options.titleBarStyle = darkui::TitleBarStyle::Black;

host.Attach(hwnd, options);
host.theme_manager().Bind(buttonA, editA, listBoxA);
host.theme_manager().Apply();
```

Recommended repeated theme update:

```cpp
host.theme_manager().Bind(buttonA, buttonB, editA, listBoxA);
host.theme_manager().Apply();

host.ApplyTheme(darkui::MakePresetTheme(darkui::ThemePreset::Moss));
```

## Panel And Surface Inheritance

Use `Panel` when several child controls live on the same card surface:

```cpp
darkui::Panel card;
darkui::Panel::Options cardOptions;
card.Create(hwnd, 1200, host.theme(), cardOptions);

darkui::Static title;
darkui::Static::Options titleOptions;
titleOptions.text = L"Repository Overview";

title.Create(card.hwnd(), 1201, host.theme(), titleOptions);
```

With that structure, child controls can usually keep the default `surfaceRole = Auto`.

One-shot dark dialog:

```cpp
darkui::ShowConfirmDialog(
    hwnd,
    5001,
    theme,
    L"Publish Session",
    L"Apply the current settings to every workstation?",
    L"Publish",
    L"Cancel");
```

See: [doc/quick.md](doc/quick.md)

## Dialog Lifetime Note

`darkui::Dialog` now uses a two-step close path for modal dialogs:

- `EndDialog()` ends the modal loop and hides the popup first
- final destruction happens later in `Destroy()` or the `Dialog` destructor

This behavior is important for input dialogs. A real bug was found in production:

- caller code opened a custom dialog with `Edit` controls
- the user clicked Confirm
- old dialog behavior destroyed the child controls immediately inside `EndDialog()`
- caller code then tried to read the input text after `ShowModal()` returned
- the read came back empty even though the user had typed valid text

Recommended pattern:

```cpp
darkui::Dialog dialog;
dialog.Create(hwnd, 5001, theme, options);

// Create child Edit / ComboBox / other controls here.

const darkui::Dialog::Result result = dialog.ShowModal();
if (result == darkui::Dialog::Result::Confirm) {
    // Read child control values here, before dialog goes out of scope.
}
// Destruction happens when dialog falls out of scope or Destroy() is called.
```

Avoid this pattern:

- do not design code around child controls being destroyed inside `EndDialog()`
- do not delay reading dialog input until after the owning wrapper object has already been destroyed

## Header Entry Points

Unified include:

```cpp
#include "darkui/darkui.h"
```

Per-control includes:

```cpp
#include "darkui/checkbox.h"
#include "darkui/button.h"
#include "darkui/combobox.h"
#include "darkui/dialog.h"
#include "darkui/edit.h"
#include "darkui/listbox.h"
#include "darkui/listview.h"
#include "darkui/panel.h"
#include "darkui/progress.h"
#include "darkui/quick.h"
#include "darkui/radiobutton.h"
#include "darkui/slider.h"
#include "darkui/static.h"
#include "darkui/tab.h"
#include "darkui/toolbar.h"
```

## Directory Layout

```text
Windows_C++_lib_darkui/
  README.md
  include/
    darkui/
      *.h
  src/
    *.cpp
  demo/
    src/
      demo_*.cpp
    build/
    build_demo*.bat
  doc/
    button.md
    checkbox.md
    combobox.md
    dialog.md
    edit.md
    listbox.md
    listview.md
    panel.md
    progress.md
    radiobutton.md
    slider.md
    static.md
    tab.md
    toolbar.md
```

## Integration Options

Recommended approaches:

- Source integration: copy or include `include/darkui` and `src` in your Win32 project
- Static-library integration: build once, then link from other projects

If you only need the demo build scripts, the current `demo/build_demo*.bat` files already compile sources directly and do not depend on `CMakeLists.txt`.

## Documentation Index

- [Button](doc/button.md)
- [CheckBox](doc/checkbox.md)
- [ComboBox](doc/combobox.md)
- [Dialog](doc/dialog.md)
- [Edit](doc/edit.md)
- [ListBox](doc/listbox.md)
- [ListView](doc/listview.md)
- [ProgressBar](doc/progress.md)
- [Quick Helpers](doc/quick.md)
- [RadioButton](doc/radiobutton.md)
- [Slider](doc/slider.md)
- [Static](doc/static.md)
- [Tab](doc/tab.md)
- [Toolbar](doc/toolbar.md)

