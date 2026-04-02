#pragma once

#include "darkui/combobox.h"

#include <memory>
#include <string>

namespace darkui {

// Semantic visual preset for darkui::Button.
// Usage:
// - Prefer `variant` for common button roles before manually tuning radius or colors.
// - Primary is the default call-to-action style.
// - Secondary, Subtle, Ghost, and Danger cover the most common supporting actions.
enum class ButtonVariant {
    Primary = 0,
    Secondary,
    Subtle,
    Ghost,
    Danger
};

// Owner-drawn dark button with standard Win32 BN_CLICKED behavior.
// Usage:
// - Fill Button::Options and call Create(parent, id, theme, options).
// - Position it with MoveWindow().
// - When the button sits inside darkui::Panel, the default `surfaceRole = Auto` inherits that panel surface.
// - Prefer `variant` for common roles before manually tuning radius or surface values.
// - Bind it into ThemeManager when the page supports theme switching, usually through ThemedWindowHost::theme_manager().
// - Handle clicks through WM_COMMAND / BN_CLICKED in the parent window.
class Button {
public:
    struct Options {
        std::wstring text{};
        int cornerRadius = -1;
        COLORREF surfaceColor = CLR_INVALID;
        SurfaceRole surfaceRole = SurfaceRole::Auto;
        ButtonVariant variant = ButtonVariant::Primary;
        DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW;
        DWORD exStyle = 0;
    };

    // Constructs an empty button wrapper.
    Button();
    // Destroys the underlying window if it still exists.
    ~Button();

    Button(const Button&) = delete;
    Button& operator=(const Button&) = delete;

    // Creates the button from an options structure.
    bool Create(HWND parent, int controlId, const Theme& theme, const Options& options);
    // Destroys the underlying button window and resets the wrapper state.
    void Destroy();

    // Returns the underlying HWND, or nullptr if the button has not been created.
    HWND hwnd() const { return buttonHwnd_; }
    // Returns the parent HWND passed to Create().
    HWND parent() const { return parentHwnd_; }
    // Returns the child control ID passed to Create().
    int control_id() const { return controlId_; }
    // Returns the theme currently stored by the control.
    const Theme& theme() const { return theme_; }
    // Returns the current corner radius in pixels.
    int corner_radius() const { return cornerRadius_; }
    // Returns the background color used to fill the area outside the rounded button body.
    COLORREF surface_color() const { return surfaceColor_; }
    // Returns the semantic visual preset currently active on the button.
    ButtonVariant variant() const { return variant_; }

    // Low-level theme hook used by ThemeManager.
    void SetTheme(const Theme& theme);
    // Updates the visible button caption.
    // Parameter:
    // - text: New caption text.
    void SetText(const std::wstring& text);
    // Reads the current button caption.
    // Returns:
    // - The current window text as a std::wstring.
    std::wstring GetText() const;
    // Sets the visual corner radius.
    // Parameter:
    // - radius: Corner radius in pixels. Values below zero are clamped to zero.
    // Notes:
    // - This changes both drawing and the button window region.
    void SetCornerRadius(int radius);
    // Sets the host surface color behind the rounded button body.
    // Purpose:
    // - Fills the rectangular area outside the rounded shape before the button body is painted.
    // - Prevents visible color seams when the button is placed on a panel/card whose color differs from `theme.background`.
    // Typical usage:
    // - Leave the default value when the button sits directly on the main window background.
    // - Set it to the surrounding card or panel color when the button is placed on a custom surface.
    // Parameters:
    // - color: Host/background color expected behind the rounded button corners.
    // Notes:
    // - This only affects the corner fallback area outside the rounded body.
    // - It does not change the main button fill color. Use SetTheme() for that.
    void SetSurfaceColor(COLORREF color);

private:
    struct Impl;
    // Internal implementation object.
    std::unique_ptr<Impl> impl_;
    // Parent window handle.
    HWND parentHwnd_ = nullptr;
    // Underlying Win32 button handle.
    HWND buttonHwnd_ = nullptr;
    // Child control ID.
    int controlId_ = 0;
    // Corner radius in pixels.
    int cornerRadius_ = 8;
    // Host/background color behind the rounded button corners.
    COLORREF surfaceColor_ = RGB(34, 36, 40);
    // Semantic surface role used when the color is inherited.
    SurfaceRole surfaceRole_ = SurfaceRole::Auto;
    // Tracks whether SetSurfaceColor or Options::surfaceColor overrode inheritance.
    bool hasCustomSurfaceColor_ = false;
    // Semantic visual preset used to derive the effective palette.
    ButtonVariant variant_ = ButtonVariant::Primary;
    // Resolved visual colors for this button instance.
    COLORREF fillColor_ = RGB(65, 72, 82);
    COLORREF fillHoverColor_ = RGB(72, 80, 92);
    COLORREF fillHotColor_ = RGB(78, 86, 98);
    COLORREF fillDisabledColor_ = RGB(50, 54, 60);
    COLORREF borderColor_ = RGB(61, 66, 74);
    COLORREF textColor_ = RGB(245, 247, 250);
    COLORREF disabledTextColor_ = RGB(130, 136, 144);
    // Theme currently used by the control.
    Theme theme_{};
};

}  // namespace darkui
