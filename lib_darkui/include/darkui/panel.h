#pragma once

#include "darkui/combobox.h"

#include <memory>

namespace darkui {

// Lightweight themed container for card/panel sections.
// Usage:
// - Fill Panel::Options and call Create(parent, id, theme, options).
// - Parent child controls to panel.hwnd() so `SurfaceRole::Auto` can inherit the panel surface.
// - Bind the panel through ThemedWindowHost::theme_manager() when the page supports runtime theme changes.
// Notes:
// - Prefer Panel when several child controls should share one card/panel surface.
// - This avoids repeating `options.surfaceRole = SurfaceRole::Panel` on every child control.
// - Child controls can usually rely on their own `variant` presets once they are parented to the panel.
class Panel {
public:
    struct Impl;
    struct Options {
        SurfaceRole role = SurfaceRole::Panel;
        int cornerRadius = 24;
        COLORREF backgroundColor = CLR_INVALID;
        COLORREF borderColor = CLR_INVALID;
        DWORD style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN;
        DWORD exStyle = 0;
    };

    Panel();
    ~Panel();

    Panel(const Panel&) = delete;
    Panel& operator=(const Panel&) = delete;

    bool Create(HWND parent, int controlId, const Theme& theme, const Options& options);
    void Destroy();

    HWND hwnd() const { return panelHwnd_; }
    HWND parent() const { return parentHwnd_; }
    int control_id() const { return controlId_; }
    const Theme& theme() const { return theme_; }
    SurfaceRole surface_role() const { return role_; }

    void SetTheme(const Theme& theme);
    void SetCornerRadius(int radius);

private:
    std::unique_ptr<Impl> impl_;
    HWND parentHwnd_ = nullptr;
    HWND panelHwnd_ = nullptr;
    int controlId_ = 0;
    Theme theme_{};
    SurfaceRole role_ = SurfaceRole::Panel;
    int cornerRadius_ = 24;
    COLORREF backgroundColor_ = RGB(44, 47, 52);
    COLORREF borderColor_ = RGB(61, 66, 74);
    bool hasCustomBackgroundColor_ = false;
    bool hasCustomBorderColor_ = false;
};

}  // namespace darkui
