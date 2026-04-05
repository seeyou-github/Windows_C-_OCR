#pragma once

#include "darkui/combobox.h"

#include <commctrl.h>

#include <memory>
#include <string>
#include <vector>

namespace darkui {

// Describes one native ListView report column.
struct ListViewColumn {
    // Column header text.
    std::wstring text;
    // Initial column width in pixels.
    // Notes:
    // - The user can still resize the native header at runtime.
    // - The wrapper keeps its dark grid overlay aligned with the live native widths.
    int width = 120;
    // Header and cell text alignment, for example LVCFMT_LEFT or LVCFMT_CENTER.
    int format = LVCFMT_LEFT;
};

// Represents one report row. Each string corresponds to one column cell.
using ListViewRow = std::vector<std::wstring>;

// Native ListView wrapper with a dark header and dark color setup.
// Usage:
// - Fill ListView::Options and call Create(parent, id, theme, options).
// - Position the wrapper with MoveWindow().
// - Bind it through ThemedWindowHost::theme_manager() when the page supports runtime theme changes.
// Notes:
// - The inner control is SysListView32.
// - Native scrollbars, column resize, native selection visuals, and keyboard behavior are preserved.
// - The wrapper custom-paints the header and draws a dark grid overlay in report mode.
// - Ctrl+C and a small right-click menu (Copy / Select All) are wired on top of the native control.
class ListView {
public:
    struct Impl;
    struct Options {
        // Initial report columns.
        std::vector<ListViewColumn> columns;
        // Initial rows inserted into the native control.
        std::vector<ListViewRow> rows;
        // Optional initial selected row index. Use -1 for none.
        int selection = -1;
        // Base window style passed to the native report ListView.
        // Notes:
        // - LVS_REPORT is always enforced by the wrapper.
        // - Multi-select is enabled by default because LVS_SINGLESEL is not included here.
        DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SHOWSELALWAYS;
        // Native extended ListView styles.
        DWORD exStyle = LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP;
    };

    ListView();
    ~ListView();

    ListView(const ListView&) = delete;
    ListView& operator=(const ListView&) = delete;

    // Creates the host window, inner native SysListView32, and dark header subclass.
    bool Create(HWND parent, int controlId, const Theme& theme, const Options& options);
    // Destroys the host, inner list, and native subclasses.
    void Destroy();

    // Returns the outer host HWND.
    HWND hwnd() const { return hostHwnd_; }
    // Returns the inner native SysListView32 HWND.
    HWND list_hwnd() const { return listHwnd_; }
    // Returns the native header HWND.
    HWND header_hwnd() const { return headerHwnd_; }
    HWND parent() const { return parentHwnd_; }
    int control_id() const { return controlId_; }
    const Theme& theme() const { return theme_; }

    // Low-level theme hook used by ThemeManager.
    // Updates dark header/background/grid resources without changing native selection visuals.
    void SetTheme(const Theme& theme);
    // Replaces the report columns and rebinds the current rows.
    void SetColumns(const std::vector<ListViewColumn>& columns);
    // Updates one column width and keeps the dark grid overlay aligned.
    bool SetColumnWidth(std::size_t index, int width);
    // Returns the last known width for a column.
    // Notes:
    // - Runtime user header resizing is synced back into this value.
    int GetColumnWidth(std::size_t index) const;
    // Replaces every row in the native control while preserving selection/focus when possible.
    void SetRows(const std::vector<ListViewRow>& rows);
    // Appends one row and refreshes the native control contents.
    void AddRow(const ListViewRow& row);
    // Removes every row from the native control.
    void ClearRows();
    // Returns the first selected row index, or -1 when nothing is selected.
    int GetSelection() const;
    // Clears the current selection and selects one row.
    // Notes:
    // - This is a convenience single-row setter even though the control supports multi-select.
    void SetSelection(int index, bool notify = false);
    // Returns the current row count tracked by the wrapper.
    std::size_t GetRowCount() const;
    // Returns the current column count tracked by the wrapper.
    std::size_t GetColumnCount() const;

private:
    std::unique_ptr<Impl> impl_;
    HWND parentHwnd_ = nullptr;
    HWND hostHwnd_ = nullptr;
    HWND listHwnd_ = nullptr;
    HWND headerHwnd_ = nullptr;
    int controlId_ = 0;
    Theme theme_{};
};

}  // namespace darkui
