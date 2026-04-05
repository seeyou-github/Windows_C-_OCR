# Native Scrollbar Dark Theme Guide

This note explains how `darkui_listview_demo` gets a native `SysListView32` scrollbar to appear as a dark gray scrollbar instead of the default light one, and how to apply the same pattern in another Win32 project.

## Goal

Use the operating system's native scrollbar and native scrolling behavior, while making the scrollbar participate in the Explorer dark-theme path.

This is the approach used by `darkui::ListView`. It does **not** replace the scrollbar with a custom control.

## What Actually Makes The Scrollbar Look Dark

The key line is:

```cpp
SetWindowTheme(listHwnd, L"DarkMode_Explorer", nullptr);
```

That call puts the native `SysListView32` onto the Explorer dark-theme path. When the runtime environment supports it, the scrollbar, client surface, and other native visuals move toward the dark gray look seen in `darkui_listview_demo`.

## Required Conditions

For this to work reliably, you need all of the following:

1. The control must be a native common control.
   Example: `WC_LISTVIEWW`, `WC_TREEVIEWW`, `WC_HEADERW`.

2. Common Controls must be initialized.
   Example:

```cpp
INITCOMMONCONTROLSEX icc{};
icc.dwSize = sizeof(icc);
icc.dwICC = ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES;
InitCommonControlsEx(&icc);
```

3. The application must link against `comctl32` and `uxtheme`.
   Example:

```bat
g++ app.cpp -lcomctl32 -luxtheme -lgdi32
```

4. The process must use Common Controls v6.
   In practice this usually means your application ships with the normal modern visual-styles manifest setup.

5. The OS/theme path must actually support `DarkMode_Explorer` for that control.
   This is not fully under application control. Some systems will still show a lighter native scrollbar.

## Generic Usage Pattern

### 1. Initialize common controls

```cpp
INITCOMMONCONTROLSEX icc{};
icc.dwSize = sizeof(icc);
icc.dwICC = ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES;
InitCommonControlsEx(&icc);
```

### 2. Create a native control

```cpp
HWND listHwnd = CreateWindowExW(
    WS_EX_CLIENTEDGE,
    WC_LISTVIEWW,
    L"",
    WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS,
    x, y, width, height,
    parent,
    reinterpret_cast<HMENU>(controlId),
    instance,
    nullptr);
```

### 3. Apply Explorer dark theme

```cpp
SetWindowTheme(listHwnd, L"DarkMode_Explorer", nullptr);
```

### 4. Set body colors so the dark scrollbar sits on a matching surface

```cpp
ListView_SetBkColor(listHwnd, theme.tableBackground);
ListView_SetTextBkColor(listHwnd, theme.tableBackground);
ListView_SetTextColor(listHwnd, theme.tableText);
```

Without this step, the control body can stay visually mismatched even if the scrollbar itself follows the dark theme path.

## Recommended Full Example

```cpp
#include <windows.h>
#include <commctrl.h>
#include <uxtheme.h>

bool CreateDarkNativeListView(HWND parent, HINSTANCE instance, int controlId, const darkui::Theme& theme, HWND* outList) {
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icc);

    HWND listHwnd = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        WC_LISTVIEWW,
        L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS,
        0, 0, 0, 0,
        parent,
        reinterpret_cast<HMENU>(controlId),
        instance,
        nullptr);
    if (!listHwnd) {
        return false;
    }

    SetWindowTheme(listHwnd, L"DarkMode_Explorer", nullptr);

    ListView_SetExtendedListViewStyle(
        listHwnd,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);

    ListView_SetBkColor(listHwnd, theme.tableBackground);
    ListView_SetTextBkColor(listHwnd, theme.tableBackground);
    ListView_SetTextColor(listHwnd, theme.tableText);

    *outList = listHwnd;
    return true;
}
```

## Header Handling

If you also want a dark header, apply a separate path to the native header control:

```cpp
HWND headerHwnd = ListView_GetHeader(listHwnd);
if (headerHwnd) {
    SetWindowTheme(headerHwnd, L"", L"");
    SetWindowSubclass(headerHwnd, HeaderSubclassProc, 1, refData);
}
```

This matters because the body/list and the header often need different treatment:

- list body: `DarkMode_Explorer`
- header: blank theme + custom paint

## Why This Is Preferable To Replacing The Scrollbar

If your goal is "native dark scrollbar that looks dark gray", this approach is usually the best tradeoff:

- native scrollbar behavior is preserved
- no custom scroll math
- no thumb-position sync bugs
- lower flicker risk
- native mouse wheel, keyboard, and accessibility behavior stay intact

By contrast, replacing a `ListView` scrollbar with a custom scrollbar gives more color control, but it is much easier to introduce:

- drag jitter
- repaint flicker
- wrong page-size mapping
- native scrollbar flashing back into view

## Limitations

This approach has important limits:

- You cannot precisely theme every scrollbar color token.
- `Theme::scrollBarBackground`, `Theme::scrollBarTrack`, `Theme::scrollBarThumb`, and `Theme::scrollBarThumbHot` do **not** automatically restyle a native `SysListView32` scrollbar.
- The final appearance still depends on OS support for the `DarkMode_Explorer` theme path.

So the right mental model is:

- use `DarkMode_Explorer` when you want a native scrollbar that looks dark enough
- use a custom scrollbar only when you need pixel-level scrollbar styling control

## How This Applies To `darkui::ListView`

The current `darkui::ListView` implementation follows this exact path:

1. Create a native `WC_LISTVIEWW`
2. Call `SetWindowTheme(listHwnd_, L"DarkMode_Explorer", nullptr)`
3. Apply dark body colors with `ListView_SetBkColor`, `ListView_SetTextBkColor`, `ListView_SetTextColor`
4. Pull out the native header and handle it separately

The core code looks like this:

```cpp
HWND listHwnd = CreateWindowExW(
    WS_EX_CLIENTEDGE,
    WC_LISTVIEWW,
    L"",
    WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS,
    0, 0, 0, 0,
    parent,
    reinterpret_cast<HMENU>(controlId),
    instance,
    nullptr);

if (!listHwnd) {
    return false;
}

SetWindowTheme(listHwnd, L"DarkMode_Explorer", nullptr);

ListView_SetExtendedListViewStyle(
    listHwnd,
    LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);

ListView_SetBkColor(listHwnd, theme.tableBackground);
ListView_SetTextBkColor(listHwnd, theme.tableBackground);
ListView_SetTextColor(listHwnd, theme.tableText);

HWND headerHwnd = ListView_GetHeader(listHwnd);
if (headerHwnd) {
    SetWindowTheme(headerHwnd, L"", L"");
    SetWindowSubclass(headerHwnd, HeaderSubclassProc, 1, refData);
}
```

## Practical Checklist

When another project wants the same effect, check these in order:

1. Is the control native `WC_LISTVIEWW` / other common control?
2. Did you initialize Common Controls?
3. Did you link `comctl32` and `uxtheme`?
4. Did you call `SetWindowTheme(control, L"DarkMode_Explorer", nullptr)` after creation?
5. Did you set matching dark body colors?
6. Is the process using visual styles / Common Controls v6?
7. Does the current OS actually honor the dark Explorer theme path for that control?

If all seven are true, you should get the same native dark gray scrollbar style as `darkui_listview_demo`, or very close to it.
