#pragma once

#include "darkui/combobox.h"

#include <memory>
#include <string>

namespace darkui {

// Owner-drawn dark checkbox that preserves Win32 BN_CLICKED behavior.
// Usage:
// - Fill CheckBox::Options and call Create(parent, id, theme, options).
// - Position it with MoveWindow().
// - When the checkbox sits inside darkui::Panel, the default `surfaceRole = Auto` inherits that panel surface.
// - Prefer `variant` for common selection styles before manually tuning surfaces.
// - Bind it into ThemeManager when the page supports theme switching, usually through ThemedWindowHost::theme_manager().
// - Handle state changes through WM_COMMAND / BN_CLICKED in the parent window.
class CheckBox {
public:
    struct Options {
        std::wstring text{};
        bool checked = false;
        COLORREF surfaceColor = CLR_INVALID;
        SurfaceRole surfaceRole = SurfaceRole::Auto;
        SelectionVariant variant = SelectionVariant::Default;
        DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP;
        DWORD exStyle = 0;
    };

    CheckBox();
    ~CheckBox();

    CheckBox(const CheckBox&) = delete;
    CheckBox& operator=(const CheckBox&) = delete;

    // Creates the checkbox from an options structure.
    bool Create(HWND parent, int controlId, const Theme& theme, const Options& options);
    // Destroys the underlying checkbox window.
    void Destroy();

    HWND hwnd() const { return checkboxHwnd_; }
    HWND parent() const { return parentHwnd_; }
    int control_id() const { return controlId_; }
    const Theme& theme() const { return theme_; }
    COLORREF surface_color() const { return surfaceColor_; }
    SelectionVariant variant() const { return variant_; }

    // Low-level theme hook used by ThemeManager.
    void SetTheme(const Theme& theme);
    void SetText(const std::wstring& text);
    std::wstring GetText() const;
    bool GetChecked() const;
    void SetChecked(bool checked);
    void SetSurfaceColor(COLORREF color);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    HWND parentHwnd_ = nullptr;
    HWND checkboxHwnd_ = nullptr;
    int controlId_ = 0;
    COLORREF surfaceColor_ = RGB(34, 36, 40);
    SurfaceRole surfaceRole_ = SurfaceRole::Auto;
    bool hasCustomSurfaceColor_ = false;
    SelectionVariant variant_ = SelectionVariant::Default;
    Theme theme_{};
};

}  // namespace darkui
