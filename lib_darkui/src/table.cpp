#include "darkui/table.h"
#include "darkui/scrollbar.h"

#include <algorithm>
#include <array>
#include <utility>

namespace darkui {
namespace {

constexpr wchar_t kTableClassName[] = L"DarkUiTableControl";

ATOM EnsureTableClassRegistered(HINSTANCE instance);
LRESULT CALLBACK TableWindowProcThunk(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

}  // namespace

struct Table::Impl {
    Table* owner = nullptr;
    HINSTANCE instance = nullptr;
    HFONT font = nullptr;
    HBRUSH brushBackground = nullptr;
    HBRUSH brushHeaderBackground = nullptr;
    HBRUSH brushHot = nullptr;
    HBRUSH brushSelected = nullptr;
    HPEN penGrid = nullptr;
    std::vector<TableColumn> columns;
    std::vector<TableRow> rows;
    int selectedRow = -1;
    int topRow = 0;
    int hotRow = -1;
    bool trackingMouseLeave = false;
    TableCellStyleCallback cellStyleCallback;
    ScrollBar verticalScrollBar;
    bool showVerticalScrollBar = false;

    explicit Impl(Table* table) : owner(table) {}

    ~Impl() {
        if (font) DeleteObject(font);
        if (brushBackground) DeleteObject(brushBackground);
        if (brushHeaderBackground) DeleteObject(brushHeaderBackground);
        if (brushHot) DeleteObject(brushHot);
        if (brushSelected) DeleteObject(brushSelected);
        if (penGrid) DeleteObject(penGrid);
    }

    void UpdateThemeResources() {
        if (font) DeleteObject(font);
        if (brushBackground) DeleteObject(brushBackground);
        if (brushHeaderBackground) DeleteObject(brushHeaderBackground);
        if (brushHot) DeleteObject(brushHot);
        if (brushSelected) DeleteObject(brushSelected);
        if (penGrid) DeleteObject(penGrid);

        font = CreateFont(owner->theme_.uiFont);
        brushBackground = CreateSolidBrush(owner->theme_.tableBackground);
        brushHeaderBackground = CreateSolidBrush(owner->theme_.tableHeaderBackground);
        brushHot = CreateSolidBrush(owner->theme_.buttonHover);
        brushSelected = CreateSolidBrush(owner->theme_.buttonHot);
        penGrid = CreatePen(PS_SOLID, 1, owner->theme_.tableGrid);

        if (owner->tableHwnd_) {
            SendMessageW(owner->tableHwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            InvalidateRect(owner->tableHwnd_, nullptr, TRUE);
        }
        if (verticalScrollBar.hwnd()) {
            verticalScrollBar.SetTheme(owner->theme_);
        }
    }

    RECT ContentRect() const {
        RECT client{};
        GetClientRect(owner->tableHwnd_, &client);
        if (showVerticalScrollBar) {
            client.right -= std::max(10, owner->theme_.scrollBarThickness);
            if (client.right < client.left) {
                client.right = client.left;
            }
        }
        return client;
    }

    void LayoutChildren() {
        if (!owner->tableHwnd_ || !verticalScrollBar.hwnd()) {
            return;
        }
        RECT client{};
        GetClientRect(owner->tableHwnd_, &client);
        const int scrollW = std::max(10, owner->theme_.scrollBarThickness);
        if (showVerticalScrollBar) {
            SetWindowPos(verticalScrollBar.hwnd(), nullptr, client.right - scrollW, client.top, scrollW, client.bottom - client.top, SWP_NOZORDER | SWP_SHOWWINDOW);
        } else {
            ShowWindow(verticalScrollBar.hwnd(), SW_HIDE);
        }
    }

    std::wstring GetCellText(int rowIndex, int columnIndex) const {
        if (rowIndex < 0 || rowIndex >= static_cast<int>(rows.size())) {
            return L"";
        }
        const auto& row = rows[rowIndex];
        if (columnIndex < 0 || columnIndex >= static_cast<int>(row.size())) {
            return L"";
        }
        return row[columnIndex];
    }

    HFONT SelectControlFont(HDC dc, HWND control) {
        HFONT current = reinterpret_cast<HFONT>(SendMessageW(control, WM_GETFONT, 0, 0));
        return current ? reinterpret_cast<HFONT>(SelectObject(dc, current)) : nullptr;
    }

    int ColumnCount() const {
        return std::max(1, static_cast<int>(columns.size()));
    }

    int TotalColumnWeight() const {
        int total = 0;
        for (const auto& column : columns) {
            total += std::max(1, column.width);
        }
        return std::max(1, total);
    }

    std::vector<int> BuildColumnEdges(const RECT& client) const {
        const int count = ColumnCount();
        std::vector<int> edges(static_cast<std::size_t>(count) + 1, client.left);
        edges[0] = client.left;
        if (count <= 0) {
            return edges;
        }

        const int width = static_cast<int>(client.right - client.left);
        const int total = TotalColumnWeight();
        int offset = client.left;
        for (int i = 0; i < count; ++i) {
            if (i == count - 1) {
                edges[static_cast<std::size_t>(i + 1)] = client.right;
            } else {
                const int weight = (i < static_cast<int>(columns.size())) ? std::max(1, columns[i].width) : 1;
                offset += (width * weight) / total;
                edges[static_cast<std::size_t>(i + 1)] = offset;
            }
        }
        return edges;
    }

    int RowTop(int rowIndex) const {
        return HeaderHeight() + (rowIndex - topRow) * owner->theme_.tableRowHeight;
    }

    int HeaderHeight() const {
        return owner->headerVisible_ ? owner->theme_.tableHeaderHeight : 0;
    }

    int VisibleRowCount(const RECT& client) const {
        const int rowHeight = std::max(1, owner->theme_.tableRowHeight);
        const int bodyHeight = std::max(0, static_cast<int>(client.bottom - HeaderHeight()));
        return std::max(1, static_cast<int>(bodyHeight / rowHeight));
    }

    int HitTestRow(int y) const {
        if (y < HeaderHeight()) {
            return -1;
        }
        RECT client = ContentRect();
        const int visibleRows = VisibleRowCount(client);
        const int index = (y - HeaderHeight()) / std::max(1, owner->theme_.tableRowHeight);
        const int row = topRow + index;
        if (index < 0 || index >= visibleRows) {
            return -1;
        }
        return (row >= 0 && row < static_cast<int>(rows.size())) ? row : -1;
    }

    int HitTestColumn(int x) const {
        RECT client = ContentRect();
        const std::vector<int> columnEdges = BuildColumnEdges(client);
        for (int col = 0; col < ColumnCount(); ++col) {
            if (x >= columnEdges[static_cast<std::size_t>(col)] && x < columnEdges[static_cast<std::size_t>(col + 1)]) {
                return col;
            }
        }
        return -1;
    }

    void NotifyParent(UINT code, int row, int column) {
        if (!owner->parentHwnd_) {
            return;
        }
        TableNotify notify{};
        notify.hdr.hwndFrom = owner->tableHwnd_;
        notify.hdr.idFrom = static_cast<UINT_PTR>(owner->controlId_);
        notify.hdr.code = code;
        notify.row = row;
        notify.column = column;
        SendMessageW(owner->parentHwnd_, WM_NOTIFY, notify.hdr.idFrom, reinterpret_cast<LPARAM>(&notify));
    }

    void ClampTopRow() {
        const int maxTop = std::max(0, static_cast<int>(rows.size()) - 1);
        topRow = std::clamp(topRow, 0, maxTop);
    }

    void StartMouseLeaveTracking() {
        if (!owner->tableHwnd_ || trackingMouseLeave) {
            return;
        }
        TRACKMOUSEEVENT track{};
        track.cbSize = sizeof(track);
        track.dwFlags = TME_LEAVE;
        track.hwndTrack = owner->tableHwnd_;
        if (TrackMouseEvent(&track)) {
            trackingMouseLeave = true;
        }
    }

    void UpdateScrollBar() {
        if (!owner->tableHwnd_) {
            return;
        }
        RECT fullClient{};
        GetClientRect(owner->tableHwnd_, &fullClient);
        showVerticalScrollBar = static_cast<int>(rows.size()) > VisibleRowCount(fullClient);
        RECT client = ContentRect();
        const int visibleRows = VisibleRowCount(client);
        const int maxTop = std::max(0, static_cast<int>(rows.size()) - visibleRows);
        topRow = std::clamp(topRow, 0, maxTop);
        verticalScrollBar.SetRange(0, std::max(0, static_cast<int>(rows.size()) - 1));
        verticalScrollBar.SetPageSize(visibleRows);
        verticalScrollBar.SetValue(topRow, false);
        LayoutChildren();
    }

    void EnsureRowVisible(int row) {
        if (!owner->tableHwnd_ || row < 0 || row >= static_cast<int>(rows.size())) {
            return;
        }
        RECT client = ContentRect();
        const int visibleRows = VisibleRowCount(client);
        if (row < topRow) {
            topRow = row;
        } else if (row >= topRow + visibleRows) {
            topRow = row - visibleRows + 1;
        }
        UpdateScrollBar();
    }

    void SetSelectedRowInternal(int row, bool notifyParent) {
        const int clamped = (row >= 0 && row < static_cast<int>(rows.size())) ? row : -1;
        if (selectedRow == clamped) {
            return;
        }
        selectedRow = clamped;
        if (selectedRow >= 0) {
            EnsureRowVisible(selectedRow);
        }
        if (owner->tableHwnd_) {
            InvalidateRect(owner->tableHwnd_, nullptr, FALSE);
        }
        if (notifyParent) {
            NotifyParent(TBN_SELCHANGE, selectedRow, -1);
        }
    }

    void DrawTextCell(HDC dc, const std::wstring& text, RECT rect, COLORREF color, UINT format) {
        rect.left += owner->theme_.textPadding;
        rect.right -= owner->theme_.textPadding;
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, color);
        DrawTextW(dc, text.c_str(), -1, &rect, format | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    }

    void Paint(HDC targetDc) {
        RECT fullClient{};
        GetClientRect(owner->tableHwnd_, &fullClient);
        RECT client = ContentRect();

        HDC memDc = CreateCompatibleDC(targetDc);
        if (!memDc) {
            FillRect(targetDc, &fullClient, brushBackground);
            return;
        }
        HBITMAP bitmap = CreateCompatibleBitmap(targetDc,
                                               std::max(1, static_cast<int>(fullClient.right)),
                                               std::max(1, static_cast<int>(fullClient.bottom)));
        if (!bitmap) {
            FillRect(targetDc, &fullClient, brushBackground);
            DeleteDC(memDc);
            return;
        }
        HGDIOBJ oldBitmap = SelectObject(memDc, bitmap);
        HFONT oldFont = SelectControlFont(memDc, owner->tableHwnd_);
        HGDIOBJ oldPen = SelectObject(memDc, penGrid);
        HGDIOBJ oldBrush = SelectObject(memDc, GetStockObject(NULL_BRUSH));

        FillRect(memDc, &fullClient, brushBackground);

        RECT headerRect{client.left, client.top, client.right, std::min(static_cast<int>(client.bottom), HeaderHeight())};
        if (owner->headerVisible_) {
            FillRect(memDc, &headerRect, brushHeaderBackground);
        }

        const int columnCount = ColumnCount();
        const std::vector<int> columnEdges = BuildColumnEdges(client);

        if (owner->headerVisible_) {
            for (int col = 0; col < columnCount; ++col) {
                RECT cell{
                    columnEdges[static_cast<std::size_t>(col)],
                    client.top,
                    columnEdges[static_cast<std::size_t>(col + 1)],
                    headerRect.bottom
                };
                Rectangle(memDc, cell.left, cell.top, cell.right, cell.bottom);
                if (col < static_cast<int>(columns.size())) {
                    UINT format = DT_LEFT;
                    if (columns[col].format == LVCFMT_CENTER) {
                        format = DT_CENTER;
                    } else if (columns[col].format == LVCFMT_RIGHT) {
                        format = DT_RIGHT;
                    }
                    DrawTextCell(memDc, columns[col].text, cell, owner->theme_.tableHeaderText, format);
                }
            }
        }

        const int visibleRows = VisibleRowCount(client);
        const int rowsToPaint = std::max(0, std::min(static_cast<int>(rows.size()) - topRow, visibleRows));

        for (int visibleRow = 0; visibleRow < rowsToPaint; ++visibleRow) {
            const int row = topRow + visibleRow;
            RECT rowRect{
                client.left,
                RowTop(row),
                client.right,
                std::min(static_cast<int>(client.bottom), RowTop(row) + owner->theme_.tableRowHeight)
            };
            TableCellStyle rowStyle = cellStyleCallback ? cellStyleCallback(row, -1, row == selectedRow) : TableCellStyle{};
            const bool hot = row == hotRow && row != selectedRow;
            HBRUSH rowBrush = (row == selectedRow) ? brushSelected : (hot ? brushHot : brushBackground);
            HBRUSH tempBrush = nullptr;
            if (rowStyle.useBackgroundColor) {
                tempBrush = CreateSolidBrush(rowStyle.backgroundColor);
                if (tempBrush) {
                    rowBrush = tempBrush;
                }
            }
            FillRect(memDc, &rowRect, rowBrush);
            if (tempBrush) {
                DeleteObject(tempBrush);
            }

            for (int col = 0; col < columnCount; ++col) {
                RECT cell{
                    columnEdges[static_cast<std::size_t>(col)],
                    rowRect.top,
                    columnEdges[static_cast<std::size_t>(col + 1)],
                    rowRect.bottom
                };
                Rectangle(memDc, cell.left, cell.top, cell.right, cell.bottom);
                TableCellStyle style = cellStyleCallback ? cellStyleCallback(row, col, row == selectedRow) : TableCellStyle{};
                DrawTextCell(memDc,
                             GetCellText(row, col),
                             cell,
                             style.useTextColor ? style.textColor : (row == selectedRow ? owner->theme_.tableHeaderText : owner->theme_.tableText),
                             DT_LEFT);
            }
        }

        const int bodyStart = HeaderHeight() + rowsToPaint * owner->theme_.tableRowHeight;
        if (bodyStart < client.bottom && owner->drawEmptyGrid_) {
            for (int col = 0; col < columnCount; ++col) {
                const int x = columnEdges[static_cast<std::size_t>(col + 1)];
                MoveToEx(memDc, x - 1, HeaderHeight(), nullptr);
                LineTo(memDc, x - 1, client.bottom);
            }
            for (int y = bodyStart; y < client.bottom; y += owner->theme_.tableRowHeight) {
                MoveToEx(memDc, client.left, y, nullptr);
                LineTo(memDc, client.right, y);
            }
        }

        Rectangle(memDc, client.left, client.top, client.right, client.bottom);
        BitBlt(targetDc, 0, 0, fullClient.right, fullClient.bottom, memDc, 0, 0, SRCCOPY);

        SelectObject(memDc, oldBrush);
        SelectObject(memDc, oldPen);
        if (oldFont) {
            SelectObject(memDc, oldFont);
        }
        SelectObject(memDc, oldBitmap);
        DeleteObject(bitmap);
        DeleteDC(memDc);
    }

    static LRESULT CALLBACK TableWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
        auto* self = reinterpret_cast<Impl*>(GetWindowLongPtrW(window, GWLP_USERDATA));

        if (message == WM_NCCREATE) {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            self = reinterpret_cast<Impl*>(create->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            return TRUE;
        }

        if (!self) {
            return DefWindowProcW(window, message, wParam, lParam);
        }

        switch (message) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC dc = BeginPaint(window, &ps);
            self->Paint(dc);
            EndPaint(window, &ps);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            SetFocus(window);
            const int row = self->HitTestRow(static_cast<int>(HIWORD(lParam)));
            const int column = self->HitTestColumn(static_cast<int>(LOWORD(lParam)));
            if (row >= 0) {
                self->SetSelectedRowInternal(row, true);
                self->NotifyParent(TBN_CELLCLICK, row, column);
            } else if (self->owner->clearSelectionOnBlankClick_) {
                RECT client = self->ContentRect();
                const int y = static_cast<int>(HIWORD(lParam));
                if (y >= self->HeaderHeight() && y < client.bottom) {
                    self->SetSelectedRowInternal(-1, true);
                }
            }
            return 0;
        }
        case WM_KILLFOCUS:
            if (self->owner->clearSelectionOnKillFocus_) {
                self->SetSelectedRowInternal(-1, true);
            }
            return 0;
        case WM_MOUSEMOVE: {
            const int row = self->HitTestRow(static_cast<int>(HIWORD(lParam)));
            if (row != self->hotRow) {
                self->hotRow = row;
                InvalidateRect(window, nullptr, FALSE);
            }
            self->StartMouseLeaveTracking();
            return 0;
        }
        case WM_MOUSELEAVE:
            self->trackingMouseLeave = false;
            if (self->hotRow != -1) {
                self->hotRow = -1;
                InvalidateRect(window, nullptr, FALSE);
            }
            return 0;
        case WM_MOUSEWHEEL: {
            RECT client = self->ContentRect();
            const int visibleRows = self->VisibleRowCount(client);
            const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            if (delta > 0) {
                self->topRow = std::max(0, self->topRow - std::max(1, visibleRows / 3));
            } else if (delta < 0) {
                self->topRow += std::max(1, visibleRows / 3);
            }
            self->UpdateScrollBar();
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        case WM_VSCROLL: {
            RECT client = self->ContentRect();
            const int visibleRows = self->VisibleRowCount(client);
            int next = self->topRow;
            const bool fromCustomScroll = reinterpret_cast<HWND>(lParam) == self->verticalScrollBar.hwnd();
            switch (LOWORD(wParam)) {
            case SB_LINEUP: next -= 1; break;
            case SB_LINEDOWN: next += 1; break;
            case SB_PAGEUP: next -= visibleRows; break;
            case SB_PAGEDOWN: next += visibleRows; break;
            case SB_THUMBPOSITION:
            case SB_THUMBTRACK: next = fromCustomScroll ? HIWORD(wParam) : self->verticalScrollBar.GetValue(); break;
            case SB_TOP: next = 0; break;
            case SB_BOTTOM: next = std::max(0, static_cast<int>(self->rows.size()) - visibleRows); break;
            default: break;
            }
            self->topRow = next;
            self->UpdateScrollBar();
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        case WM_KEYDOWN:
            if (self->rows.empty()) {
                return 0;
            }
            switch (wParam) {
            case VK_UP:
                self->SetSelectedRowInternal(self->selectedRow >= 0 ? self->selectedRow - 1 : 0, true);
                return 0;
            case VK_DOWN:
                self->SetSelectedRowInternal(self->selectedRow >= 0 ? self->selectedRow + 1 : 0, true);
                return 0;
            case VK_PRIOR: {
                RECT client{};
                GetClientRect(window, &client);
                self->SetSelectedRowInternal(self->selectedRow >= 0 ? self->selectedRow - self->VisibleRowCount(client) : 0, true);
                return 0;
            }
            case VK_NEXT: {
                RECT client{};
                GetClientRect(window, &client);
                self->SetSelectedRowInternal(self->selectedRow >= 0 ? self->selectedRow + self->VisibleRowCount(client) : 0, true);
                return 0;
            }
            default:
                break;
            }
            break;
        case WM_SIZE:
            self->UpdateScrollBar();
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        default:
            break;
        }

        return DefWindowProcW(window, message, wParam, lParam);
    }
};

namespace {

ATOM EnsureTableClassRegistered(HINSTANCE instance) {
    static ATOM atom = 0;
    if (atom != 0) {
        return atom;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = TableWindowProcThunk;
    wc.hInstance = instance;
    wc.lpszClassName = kTableClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    atom = RegisterClassExW(&wc);
    return atom;
}

}  // namespace

namespace {

LRESULT CALLBACK TableWindowProcThunk(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    return Table::Impl::TableWindowProc(window, message, wParam, lParam);
}

}  // namespace

Table::Table() : impl_(std::make_unique<Impl>(this)) {}

Table::~Table() {
    Destroy();
}

bool Table::Create(HWND parent, int controlId, const Theme& theme, DWORD style, DWORD exStyle) {
    Destroy();
    parentHwnd_ = parent;
    controlId_ = controlId;
    theme_ = theme;
    impl_->instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(parent, GWLP_HINSTANCE));
    if (!impl_->instance) {
        impl_->instance = GetModuleHandleW(nullptr);
    }

    EnsureTableClassRegistered(impl_->instance);
    tableHwnd_ = CreateWindowExW(exStyle,
                                 kTableClassName,
                                 L"",
                                 (style & ~(WS_VSCROLL | WS_HSCROLL)) | WS_CLIPSIBLINGS,
                                 0,
                                 0,
                                 0,
                                 0,
                                 parent,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
                                 impl_->instance,
                                 impl_.get());
    if (!tableHwnd_) {
        Destroy();
        return false;
    }

    if (!impl_->verticalScrollBar.Create(tableHwnd_,
                                         controlId + 10000,
                                         true,
                                         theme_,
                                         WS_CHILD | WS_VISIBLE,
                                         0)) {
        Destroy();
        return false;
    }

    impl_->UpdateThemeResources();
    impl_->UpdateScrollBar();
    return true;
}

void Table::Destroy() {
    impl_->verticalScrollBar.Destroy();
    if (tableHwnd_) {
        DestroyWindow(tableHwnd_);
        tableHwnd_ = nullptr;
    }
    parentHwnd_ = nullptr;
    controlId_ = 0;
    impl_->selectedRow = -1;
    impl_->topRow = 0;
    impl_->hotRow = -1;
    impl_->trackingMouseLeave = false;
    impl_->cellStyleCallback = {};
}

void Table::SetTheme(const Theme& theme) {
    theme_ = theme;
    impl_->UpdateThemeResources();
}

void Table::SetColumns(const std::vector<TableColumn>& columns) {
    impl_->columns = columns;
    if (tableHwnd_) {
        InvalidateRect(tableHwnd_, nullptr, TRUE);
    }
}

void Table::SetRows(const std::vector<TableRow>& rows) {
    impl_->rows = rows;
    if (impl_->selectedRow >= static_cast<int>(rows.size())) {
        impl_->selectedRow = -1;
    }
    if (tableHwnd_) {
        impl_->UpdateScrollBar();
        InvalidateRect(tableHwnd_, nullptr, TRUE);
    }
}

void Table::AddRow(const TableRow& row) {
    impl_->rows.push_back(row);
    if (tableHwnd_) {
        impl_->UpdateScrollBar();
        InvalidateRect(tableHwnd_, nullptr, TRUE);
    }
}

void Table::ClearRows() {
    impl_->rows.clear();
    impl_->selectedRow = -1;
    impl_->topRow = 0;
    if (tableHwnd_) {
        impl_->UpdateScrollBar();
        InvalidateRect(tableHwnd_, nullptr, TRUE);
    }
}

void Table::SetDrawEmptyGrid(bool enabled) {
    drawEmptyGrid_ = enabled;
    if (tableHwnd_) {
        InvalidateRect(tableHwnd_, nullptr, TRUE);
    }
}

void Table::SetHeaderVisible(bool visible) {
    headerVisible_ = visible;
    if (tableHwnd_) {
        impl_->UpdateScrollBar();
        InvalidateRect(tableHwnd_, nullptr, TRUE);
    }
}

int Table::GetSelectedRow() const {
    return impl_->selectedRow;
}

void Table::SetSelectedRow(int row) {
    impl_->SetSelectedRowInternal(row, false);
}

void Table::EnsureVisible(int row) {
    impl_->EnsureRowVisible(row);
    if (tableHwnd_) {
        InvalidateRect(tableHwnd_, nullptr, FALSE);
    }
}

int Table::HitTestRow(POINT point) const {
    return tableHwnd_ ? impl_->HitTestRow(point.y) : -1;
}

int Table::HitTestColumn(POINT point) const {
    return tableHwnd_ ? impl_->HitTestColumn(point.x) : -1;
}

void Table::SetCellStyleCallback(TableCellStyleCallback callback) {
    impl_->cellStyleCallback = std::move(callback);
    if (tableHwnd_) {
        InvalidateRect(tableHwnd_, nullptr, TRUE);
    }
}

void Table::SetClearSelectionOnKillFocus(bool enabled) {
    clearSelectionOnKillFocus_ = enabled;
}

void Table::SetClearSelectionOnBlankClick(bool enabled) {
    clearSelectionOnBlankClick_ = enabled;
}

std::size_t Table::GetRowCount() const {
    return impl_->rows.size();
}

std::size_t Table::GetColumnCount() const {
    return impl_->columns.size();
}

}  // namespace darkui
