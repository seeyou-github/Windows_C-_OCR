#pragma once

#include "darkui/combobox.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace darkui {

// Single item stored by darkui::ListBox.
struct ListBoxItem {
    // Text shown to the user.
    std::wstring text;
    // Optional application-owned payload value.
    std::uintptr_t userData = 0;
};

// Dark list box built from a rounded host window plus an inner native LISTBOX.
// Usage:
// - Fill ListBox::Options and call Create(parent, id, theme, options).
// - Fill it with SetItems() or AddItem().
// - Position the host window with MoveWindow().
// - When the list box sits on a grouped card surface, prefer parenting it to darkui::Panel.
// - Prefer `variant` for common field styles before manually tuning shape or density.
// - Bind it through ThemedWindowHost::theme_manager() when the page supports runtime theme changes.
// - Listen for LBN_SELCHANGE in the parent window.
// Notes:
// - The inner LISTBOX keeps native keyboard, selection, and scrolling behavior.
// - Single-selection and multi-selection styles are both supported through the
//   standard Win32 list-box style flags passed to Create().
class ListBox {
public:
    struct Impl;
    struct Options {
        std::vector<ListBoxItem> items;
        int selection = -1;
        int cornerRadius = -1;
        FieldVariant variant = FieldVariant::Default;
        DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_VSCROLL;
        DWORD exStyle = 0;
    };

    ListBox();
    ~ListBox();

    ListBox(const ListBox&) = delete;
    ListBox& operator=(const ListBox&) = delete;

    // Creates the list box from an options structure.
    bool Create(HWND parent, int controlId, const Theme& theme, const Options& options);
    // Destroys the host and inner list-box windows.
    void Destroy();

    // Returns the dark host HWND.
    HWND hwnd() const { return hostHwnd_; }
    // Returns the inner native LISTBOX HWND.
    HWND list_hwnd() const { return listHwnd_; }
    // Returns the parent HWND passed to Create().
    HWND parent() const { return parentHwnd_; }
    // Returns the child control ID passed to Create().
    int control_id() const { return controlId_; }
    // Returns the active theme.
    const Theme& theme() const { return theme_; }
    // Returns the current corner radius in pixels.
    int corner_radius() const { return cornerRadius_; }
    // Returns the semantic field preset currently active on the control.
    FieldVariant variant() const { return variant_; }

    // Low-level theme hook used by ThemeManager.
    void SetTheme(const Theme& theme);
    // Replaces all items at once.
    void SetItems(const std::vector<ListBoxItem>& items);
    // Appends a single item to the end of the list.
    void AddItem(const ListBoxItem& item);
    // Removes all items.
    void ClearItems();
    // Returns the number of items currently stored in the control.
    std::size_t GetCount() const;
    // Returns the current zero-based selection index for single-select mode.
    int GetSelection() const;
    // Returns all selected indices for single-select or multi-select mode.
    std::vector<int> GetSelections() const;
    // Changes the current selection.
    // Parameters:
    // - index: Zero-based item index.
    // - notify: When true, forwards LBN_SELCHANGE to the parent window.
    void SetSelection(int index, bool notify = false);
    // Returns a copy of the item at the specified index.
    ListBoxItem GetItem(int index) const;
    // Sets the outer corner radius of the dark host surface.
    void SetCornerRadius(int radius);

private:
    std::unique_ptr<Impl> impl_;
    HWND parentHwnd_ = nullptr;
    HWND hostHwnd_ = nullptr;
    HWND listHwnd_ = nullptr;
    int controlId_ = 0;
    int cornerRadius_ = 10;
    int itemHeight_ = 26;
    FieldVariant variant_ = FieldVariant::Default;
    Theme theme_{};
};

}  // namespace darkui
