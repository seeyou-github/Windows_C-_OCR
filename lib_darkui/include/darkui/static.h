#pragma once

#include "darkui/combobox.h"

#include <memory>
#include <string>

namespace darkui {

// Semantic text-presentational preset for darkui::Static.
// Usage:
// - Prefer a variant for common text hierarchy before manually tuning text format or colors.
// - Title and PanelTitle are suited for headings.
// - Body, PanelBody, and Muted cover the common supporting text cases.
enum class StaticVariant {
    Default = 0,
    Title,
    Body,
    Muted,
    PanelTitle,
    PanelBody
};

// Dark static control that can render text, an icon, or a bitmap.
// Usage:
// - Fill Static::Options and call Create(parent, id, theme, options).
// - Position it with MoveWindow().
// - When the static control sits inside darkui::Panel, the default `surfaceRole = Auto` inherits that panel surface.
// - Prefer `variant` for common text hierarchy before manually tuning formatting.
// - Bind it through ThemedWindowHost::theme_manager() when the page supports runtime theme changes.
// - Use SetText(), SetIcon(), or SetBitmap() to switch presentation mode.
// Notes:
// - The control is primarily presentation-oriented and usually non-interactive.
// - If you need click notifications, pass SS_NOTIFY in the style flags.
class Static {
public:
    enum class ContentMode {
        Text,
        Icon,
        Bitmap
    };
    struct Options {
        std::wstring text{};
        HICON icon = nullptr;
        HBITMAP bitmap = nullptr;
        COLORREF backgroundColor = CLR_INVALID;
        SurfaceRole surfaceRole = SurfaceRole::Auto;
        StaticVariant variant = StaticVariant::Default;
        UINT textFormat = DT_LEFT;
        bool ellipsis = true;
        DWORD style = WS_CHILD | WS_VISIBLE | SS_LEFT;
        DWORD exStyle = 0;
    };

    Static();
    ~Static();

    Static(const Static&) = delete;
    Static& operator=(const Static&) = delete;

    // Creates the static control from an options structure.
    bool Create(HWND parent, int controlId, const Theme& theme, const Options& options);
    // Destroys the underlying window and resets wrapper state.
    void Destroy();

    // Returns the underlying HWND.
    HWND hwnd() const { return staticHwnd_; }
    // Returns the parent HWND passed to Create().
    HWND parent() const { return parentHwnd_; }
    // Returns the child control ID passed to Create().
    int control_id() const { return controlId_; }
    // Returns the active theme.
    const Theme& theme() const { return theme_; }
    // Returns the current content mode.
    ContentMode content_mode() const { return contentMode_; }
    // Returns the background color currently used by the control.
    COLORREF background_color() const { return backgroundColor_; }
    // Returns the semantic static-text preset currently active on the control.
    StaticVariant variant() const { return variant_; }

    // Low-level theme hook used by ThemeManager.
    void SetTheme(const Theme& theme);
    // Updates the visible text and switches the mode to Text.
    void SetText(const std::wstring& text);
    // Returns the current text value.
    std::wstring GetText() const;
    // Switches the control to Icon mode.
    // Notes:
    // - The icon handle is not owned or destroyed by darkui.
    void SetIcon(HICON icon);
    // Switches the control to Bitmap mode.
    // Notes:
    // - The bitmap handle is not owned or destroyed by darkui.
    void SetBitmap(HBITMAP bitmap);
    // Clears any assigned icon or bitmap and returns to Text mode.
    void ClearImage();
    // Sets the background color used behind text, icons, or bitmaps.
    void SetBackgroundColor(COLORREF color);
    // Sets DrawText alignment flags such as DT_LEFT / DT_CENTER / DT_RIGHT.
    // Notes:
    // - Vertical centering is handled internally.
    void SetTextFormat(UINT format);
    // Enables or disables end ellipsis for long text.
    void SetEllipsis(bool enabled);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    HWND parentHwnd_ = nullptr;
    HWND staticHwnd_ = nullptr;
    int controlId_ = 0;
    Theme theme_{};
    ContentMode contentMode_ = ContentMode::Text;
    COLORREF backgroundColor_ = RGB(34, 36, 40);
    SurfaceRole surfaceRole_ = SurfaceRole::Auto;
    bool hasCustomBackgroundColor_ = false;
    StaticVariant variant_ = StaticVariant::Default;
    COLORREF textColor_ = RGB(224, 227, 232);
    UINT textFormat_ = DT_LEFT;
    bool ellipsis_ = true;
};

}  // namespace darkui
