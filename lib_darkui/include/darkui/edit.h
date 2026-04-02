#pragma once

#include "darkui/combobox.h"

#include <memory>
#include <string>

namespace darkui {

// Custom dark edit control built from a lightweight host window plus an inner borderless EDIT.
// Usage:
// - Fill Edit::Options and call Create(parent, id, theme, options).
// - Position the host window with MoveWindow().
// - When several inputs share one card surface, prefer parenting them to darkui::Panel.
// - Prefer `variant` for common field styles before manually tuning shape or density.
// - Bind it through ThemedWindowHost::theme_manager() when the page supports runtime theme changes.
// - Handle WM_COMMAND notifications such as EN_CHANGE in the parent window.
// Notes:
// - The host paints the dark background so no native border line is visible.
// - The inner EDIT keeps native text input, caret, selection, and IME behavior.
// - Placeholder text is rendered by darkui itself so color and visibility remain
//   controllable even when native cue-banner support is unavailable.
class Edit {
public:
    struct Impl;
    struct Options {
        std::wstring text{};
        std::wstring cueBanner{};
        int cornerRadius = -1;
        FieldVariant variant = FieldVariant::Default;
        bool readOnly = false;
        DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL;
        DWORD exStyle = 0;
    };

    // Constructs an empty edit wrapper.
    Edit();
    // Destroys the host and inner edit windows if they still exist.
    ~Edit();

    Edit(const Edit&) = delete;
    Edit& operator=(const Edit&) = delete;

    // Creates the dark edit control from an options structure.
    bool Create(HWND parent, int controlId, const Theme& theme, const Options& options);
    // Destroys the control window hierarchy and resets wrapper state.
    void Destroy();

    // Returns the dark host HWND.
    HWND hwnd() const { return hostHwnd_; }
    // Returns the inner native EDIT HWND.
    HWND edit_hwnd() const { return editHwnd_; }
    // Returns the parent HWND passed to Create().
    HWND parent() const { return parentHwnd_; }
    // Returns the child control ID passed to Create().
    int control_id() const { return controlId_; }
    // Returns the theme currently stored by the control.
    const Theme& theme() const { return theme_; }
    // Returns the current corner radius in pixels.
    int corner_radius() const { return cornerRadius_; }
    // Returns the semantic field preset currently active on the control.
    FieldVariant variant() const { return variant_; }

    // Low-level theme hook used by ThemeManager.
    void SetTheme(const Theme& theme);
    // Updates the visible text.
    void SetText(const std::wstring& text);
    // Returns the current text value.
    std::wstring GetText() const;
    // Sets the placeholder text shown while the control is empty and unfocused.
    // Notes:
    // - The placeholder is drawn by darkui itself so color and visibility stay
    //   consistent even on systems without EM_SETCUEBANNER support.
    void SetCueBanner(const std::wstring& text);
    // Sets the outer corner radius of the dark host surface.
    // Notes:
    // - Values below zero are clamped to zero.
    // - This updates both painting and the host window region.
    void SetCornerRadius(int radius);
    // Toggles native read-only mode.
    // Parameters:
    // - readOnly: When true, the inner EDIT becomes non-editable but keeps the same dark colors.
    void SetReadOnly(bool readOnly);
    // Returns the latest internal layout debug string for this control.
    std::wstring DebugLayoutInfo() const;

private:
    std::unique_ptr<Impl> impl_;
    HWND parentHwnd_ = nullptr;
    HWND hostHwnd_ = nullptr;
    HWND editHwnd_ = nullptr;
    int controlId_ = 0;
    int cornerRadius_ = 10;
    FieldVariant variant_ = FieldVariant::Default;
    Theme theme_{};
};

}  // namespace darkui
