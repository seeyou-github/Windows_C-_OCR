#pragma once

#include "darkui/combobox.h"

#include <memory>

namespace darkui {

// Custom dark scrollbar for Win32.
// Usage:
// - Fill ScrollBar::Options and call Create(parent, id, theme, options).
// - Position it with MoveWindow().
// - Bind it into ThemeManager when the page supports theme switching, usually through ThemedWindowHost::theme_manager().
// - Handle WM_VSCROLL or WM_HSCROLL notifications in the parent window.
class ScrollBar {
public:
    struct Impl;
    struct Options {
        bool vertical = true;
        int minimum = 0;
        int maximum = 100;
        int pageSize = 10;
        int value = 0;
        DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP;
        DWORD exStyle = 0;
    };

    // Constructs an empty scrollbar wrapper.
    ScrollBar();
    // Destroys the underlying scrollbar window if it still exists.
    ~ScrollBar();

    ScrollBar(const ScrollBar&) = delete;
    ScrollBar& operator=(const ScrollBar&) = delete;

    // Creates the scrollbar from an options structure.
    bool Create(HWND parent, int controlId, const Theme& theme, const Options& options);
    // Destroys the scrollbar window and resets wrapper state.
    void Destroy();

    // Returns the underlying HWND, or nullptr before Create().
    HWND hwnd() const { return scrollBarHwnd_; }
    // Returns the parent HWND passed to Create().
    HWND parent() const { return parentHwnd_; }
    // Returns the child control ID passed to Create().
    int control_id() const { return controlId_; }
    // Returns the theme currently stored by the control.
    const Theme& theme() const { return theme_; }
    // Returns true for a vertical scrollbar, false for a horizontal scrollbar.
    bool vertical() const { return vertical_; }

    // Low-level theme hook used by ThemeManager.
    void SetTheme(const Theme& theme);
    // Sets the logical scroll range.
    // Parameters:
    // - minimum: Minimum logical value.
    // - maximum: Maximum logical value.
    // Notes:
    // - If minimum is greater than maximum, the values are swapped.
    // - The current value is clamped into the reachable range.
    void SetRange(int minimum, int maximum);
    // Sets the logical page size used for thumb length and page jumps.
    // Parameter:
    // - pageSize: Page size in logical units. Negative values are clamped to zero.
    // Notes:
    // - A larger page size produces a larger thumb.
    void SetPageSize(int pageSize);
    // Sets the current scroll position.
    // Parameters:
    // - value: New logical scroll value.
    // - notify: When true, sends WM_VSCROLL or WM_HSCROLL / SB_THUMBPOSITION to the parent.
    // Notes:
    // - The value is clamped into the reachable range determined by range and page size.
    void SetValue(int value, bool notify = false);

    // Returns the current logical value.
    int GetValue() const { return value_; }
    // Returns the current minimum logical value.
    int GetMinimum() const { return minimum_; }
    // Returns the current maximum logical value.
    int GetMaximum() const { return maximum_; }
    // Returns the current logical page size.
    int GetPageSize() const { return pageSize_; }

private:
    // Internal implementation object.
    std::unique_ptr<Impl> impl_;
    // Parent window handle.
    HWND parentHwnd_ = nullptr;
    // Underlying scrollbar window handle.
    HWND scrollBarHwnd_ = nullptr;
    // Child control ID.
    int controlId_ = 0;
    // Minimum logical value.
    int minimum_ = 0;
    // Maximum logical value.
    int maximum_ = 100;
    // Logical page size used for thumb size and page jump behavior.
    int pageSize_ = 10;
    // Current logical scroll value.
    int value_ = 0;
    // Orientation flag. true = vertical, false = horizontal.
    bool vertical_ = true;
    // Theme currently used by the control.
    Theme theme_{};
};

}  // namespace darkui
