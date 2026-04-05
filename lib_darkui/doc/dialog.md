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
- Safe post-confirm value reading for input dialogs

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

For input dialogs, the recommended flow is:

```cpp
darkui::Dialog::Result result = dialog.ShowModal();
if (result == darkui::Dialog::Result::Confirm) {
    std::wstring value = titleEdit.GetText();
    // Read custom child control values here.
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

## Important Lifetime Rule

`darkui::Dialog` no longer destroys its child controls immediately inside `EndDialog()`.

Current close flow:

1. `EndDialog()` sets the modal result
2. `EndDialog()` hides the popup and stops the modal loop
3. `ShowModal()` returns to caller code
4. caller code reads values from custom child controls if needed
5. final destruction happens in `Destroy()` or the `Dialog` destructor

This change was made to fix a real bug:

- a custom dialog contained input fields such as `darkui::Edit`
- the user typed valid text and clicked Confirm
- old behavior destroyed the dialog window and child controls too early
- caller code read `Edit::GetText()` after `ShowModal()` returned
- the returned text was empty, so validation failed even though the user had entered data

How to avoid this problem:

- read child control values immediately after `ShowModal()` returns
- keep the dialog object alive until after those values are read
- if the dialog is a local stack object, this usually happens naturally
- if the dialog is a long-lived member or heap object, call `Destroy()` only after you no longer need to read child control state

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

Notes:

- `ShowModal()` returns before final destruction
- custom child controls are still readable after it returns
- this is the correct point to validate or copy user input

### `EndDialog`

```cpp
dialog.EndDialog(darkui::Dialog::Result::Cancel);
```

Notes:

- `EndDialog()` exits modal state and hides the dialog
- it does not immediately destroy the popup window
- delayed destruction is intentional so caller code can still read input values safely

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
- For input dialogs, read `Edit` / `ComboBox` / other child control values immediately after `ShowModal()` returns

## Demo Reference

For a complete example, see:

- `../demo/src/demo_dialog.cpp`

## Current Limitations

- No resize grip
- No maximize or minimize buttons
- No native task-dialog style command links
