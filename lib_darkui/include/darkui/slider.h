#pragma once

#include "darkui/combobox.h"

#include <memory>

namespace darkui {

// Custom horizontal slider control for dark Win32 UIs.
// Usage:
// - Fill Slider::Options and call Create(parent, id, theme, options).
// - Position it with MoveWindow().
// - Bind it into ThemeManager when the page supports theme switching, usually through ThemedWindowHost::theme_manager().
// - Handle value changes through WM_HSCROLL in the parent window.
// - Prefer `variant` for common density/emphasis presets before manually tuning low-level styling:
//   `Default`, `Dense`, `Emphasis`.
class Slider {
public:
    struct Impl;
    struct Options {
        int minimum = 0;
        int maximum = 100;
        int value = 0;
        bool showTicks = false;
        int tickCount = 0;
        SliderVariant variant = SliderVariant::Default;
        DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP;
        DWORD exStyle = 0;
    };

    // Constructs an empty slider wrapper.
    Slider();
    // Destroys the underlying slider window if it still exists.
    ~Slider();

    Slider(const Slider&) = delete;
    Slider& operator=(const Slider&) = delete;

    // Creates the slider from an options structure.
    bool Create(HWND parent, int controlId, const Theme& theme, const Options& options);
    // Destroys the slider window and resets wrapper state.
    void Destroy();

    // Returns the underlying slider HWND, or nullptr before Create().
    HWND hwnd() const { return sliderHwnd_; }
    // Returns the parent HWND passed to Create().
    HWND parent() const { return parentHwnd_; }
    // Returns the child control ID passed to Create().
    int control_id() const { return controlId_; }
    // Returns the theme currently stored by the control.
    const Theme& theme() const { return theme_; }
    // Returns the semantic slider preset currently active on the control.
    SliderVariant variant() const { return variant_; }

    // Low-level theme hook used by ThemeManager.
    void SetTheme(const Theme& theme);
    // Sets the logical slider range.
    // Parameters:
    // - minimum: Minimum logical value.
    // - maximum: Maximum logical value.
    // Notes:
    // - If minimum is greater than maximum, the values are swapped.
    // - The current value is clamped into the new range.
    void SetRange(int minimum, int maximum);
    // Sets the current slider value.
    // Parameters:
    // - value: New logical value.
    // - notify: When true, sends WM_HSCROLL / SB_THUMBPOSITION to the parent.
    // Notes:
    // - The value is clamped into the current range.
    void SetValue(int value, bool notify = false);
    // Enables or disables tick-mark drawing.
    // Parameter:
    // - enabled: true to show tick marks, false to hide them.
    void SetShowTicks(bool enabled);
    // Sets how many tick marks are drawn.
    // Parameter:
    // - count: Total number of ticks. Values below zero are clamped to zero.
    // Notes:
    // - Tick marks are only visible when SetShowTicks(true) is enabled.
    void SetTickCount(int count);
    // Returns the current logical value.
    int GetValue() const { return value_; }
    // Returns the current minimum logical value.
    int GetMinimum() const { return minimum_; }
    // Returns the current maximum logical value.
    int GetMaximum() const { return maximum_; }
    // Returns whether tick marks are currently enabled.
    bool show_ticks() const { return showTicks_; }
    // Returns the configured tick-count value.
    int tick_count() const { return tickCount_; }

private:
    // Internal implementation object.
    std::unique_ptr<Impl> impl_;
    // Parent window handle.
    HWND parentHwnd_ = nullptr;
    // Underlying slider window handle.
    HWND sliderHwnd_ = nullptr;
    // Child control ID.
    int controlId_ = 0;
    // Minimum logical value.
    int minimum_ = 0;
    // Maximum logical value.
    int maximum_ = 100;
    // Current logical value.
    int value_ = 0;
    // Whether tick marks should be rendered.
    bool showTicks_ = false;
    // Number of tick marks to render.
    int tickCount_ = 0;
    // Semantic slider preset used to derive the effective visuals.
    SliderVariant variant_ = SliderVariant::Default;
    int trackHeight_ = 6;
    int thumbRadius_ = 9;
    COLORREF backgroundColor_ = RGB(34, 36, 40);
    COLORREF trackColor_ = RGB(52, 56, 62);
    COLORREF fillColor_ = RGB(78, 120, 184);
    COLORREF thumbColor_ = RGB(224, 227, 232);
    COLORREF thumbHotColor_ = RGB(245, 247, 250);
    COLORREF tickColor_ = RGB(92, 100, 112);
    // Theme currently used by the control.
    Theme theme_{};
};

}  // namespace darkui
