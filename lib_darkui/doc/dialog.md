# Dialog

## Overview

`darkui::Dialog` is a modal dark popup window for Win32 with a custom black title bar, dark background, and built-in confirm/cancel buttons. The center area can either use a built-in message `Static` or host your own child controls such as `Static`, `Edit`, `Button`, and multiline `Edit`.

## Files

- `include/darkui/dialog.h`
- `src/dialog.cpp`

## Suitable Scenarios

- Confirmation dialogs
- Input prompts
- Small custom forms and notes popups
- Dark replacement for basic system message boxes

## Main Features

- Custom dark title bar and background
- Built-in confirm and cancel buttons
- Built-in centered message label
- Exposes `content_hwnd()` for custom child controls
- Modal message loop with owner-window disabling

## Recommended Usage

```cpp
#include "darkui/dialog.h"

darkui::Dialog dialog;
darkui::Dialog::Options options;
options.title = L"Delete File";
options.message = L"Delete the selected item permanently?";
options.confirmText = L"Delete";
options.cancelText = L"Cancel";
options.width = 460;
options.height = 240;

dialog.Create(hwnd, 5001, theme, options);

darkui::Dialog::Result result = dialog.ShowModal();
if (result == darkui::Dialog::Result::Confirm) {
    // confirmed
}
```

## Custom Content Example

```cpp
darkui::Dialog dialog;
darkui::Dialog::Options dialogOptions;
dialogOptions.title = L"Create Note";
dialogOptions.width = 560;
dialogOptions.height = 360;
dialogOptions.messageVisible = false;
dialog.Create(hwnd, 5002, theme, dialogOptions);

darkui::Static label;
darkui::Edit titleEdit;
darkui::Button fillButton;
darkui::Edit notesEdit;
darkui::Panel formPanel;

darkui::Panel::Options panelOptions;
panelOptions.cornerRadius = 18;
formPanel.Create(dialog.content_hwnd(), 5100, theme, panelOptions);

darkui::Static::Options labelOptions;
labelOptions.text = L"Title";
labelOptions.variant = darkui::StaticVariant::PanelTitle;
darkui::Edit::Options titleOptions;
titleOptions.cueBanner = L"Enter a title";
titleOptions.variant = darkui::FieldVariant::Panel;
darkui::Button::Options fillOptions;
fillOptions.text = L"Fill Sample";
fillOptions.variant = darkui::ButtonVariant::Secondary;
darkui::Edit::Options notesOptions;
notesOptions.cueBanner = L"Write notes here";
notesOptions.variant = darkui::FieldVariant::Panel;
notesOptions.style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL;

label.Create(formPanel.hwnd(), 5101, theme, labelOptions);
titleEdit.Create(formPanel.hwnd(), 5102, theme, titleOptions);
fillButton.Create(formPanel.hwnd(), 5103, theme, fillOptions);
notesEdit.Create(formPanel.hwnd(), 5104, theme, notesOptions);

MoveWindow(formPanel.hwnd(), 16, 16, 512, 240, TRUE);
MoveWindow(label.hwnd(), 16, 16, 120, 24, TRUE);
MoveWindow(titleEdit.hwnd(), 16, 44, 360, 38, TRUE);
MoveWindow(fillButton.hwnd(), 392, 44, 104, 38, TRUE);
MoveWindow(notesEdit.hwnd(), 16, 96, 480, 128, TRUE);

dialog.ShowModal();
```

## Common API

### `Create`

```cpp
bool Create(HWND owner, int controlId, const Theme& theme, const Dialog::Options& options);
```

### `SetMessage`

```cpp
dialog.SetMessage(L"Operation completed.");
```

### `SetMessageVisible`

```cpp
dialog.SetMessageVisible(false);
```

### `content_hwnd`

```cpp
HWND host = dialog.content_hwnd();
```

### `ShowModal`

```cpp
darkui::Dialog::Result result = dialog.ShowModal();
```

### `EndDialog`

```cpp
dialog.EndDialog(darkui::Dialog::Result::Cancel);
```

## Theme Fields Used

- `background`
- `panel`
- `border`
- `text`
- `mutedText`
- `button`
- `buttonHover`
- `buttonHot`
- `buttonDisabled`
- `buttonDisabledText`
- `staticBackground`
- `staticText`
- `uiFont`

## Usage Notes

- `ShowModal()` disables the owner window until the popup closes
- Use `SetMessageVisible(false)` when you want a fully custom body
- Prefer per-control `variant` fields for button, static, and input styling inside custom dialog bodies
- Unknown `WM_COMMAND` and `WM_NOTIFY` traffic from custom child controls is forwarded to the owner window

## Demo Reference

For a complete example, see:

- `../demo/src/demo_dialog.cpp`

## Current Limitations

- No resize grip
- No maximize or minimize buttons
- No native task-dialog style command links
