#include "darkui/listview.h"

#include <commctrl.h>
#include <uxtheme.h>
#include <windowsx.h>

#include <algorithm>
#include <sstream>
namespace darkui {
namespace {

constexpr wchar_t kListViewHostClassName[] = L"DarkUiListViewHost";
constexpr UINT_PTR kListSubclassId = 0x6A01;
constexpr UINT kListViewCommandCopy = 0x7101;
constexpr UINT kListViewCommandSelectAll = 0x7102;

ATOM EnsureListViewHostClassRegistered(HINSTANCE instance);
LRESULT CALLBACK ListViewHostWindowProcThunk(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK HeaderSubclassProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassId, DWORD_PTR refData);
LRESULT CALLBACK ListSubclassProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassId, DWORD_PTR refData);

void StripClientEdge(HWND window) {
    const LONG_PTR exStyle = GetWindowLongPtrW(window, GWL_EXSTYLE);
    const LONG_PTR newStyle = exStyle & ~static_cast<LONG_PTR>(WS_EX_CLIENTEDGE) & ~static_cast<LONG_PTR>(WS_EX_STATICEDGE);
    if (newStyle != exStyle) {
        SetWindowLongPtrW(window, GWL_EXSTYLE, newStyle);
        SetWindowPos(window, nullptr, 0, 0, 0, 0,
                     SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

}  // namespace

struct ListView::Impl {
    ListView* owner = nullptr;
    HINSTANCE instance = nullptr;
    HBRUSH brushHost = nullptr;
    HBRUSH brushHeader = nullptr;
    HPEN penBorder = nullptr;
    HPEN penGrid = nullptr;
    HFONT font = nullptr;
    std::vector<ListViewColumn> columns;
    std::vector<ListViewRow> rows;

    explicit Impl(ListView* listView) : owner(listView) {}

    ~Impl() {
        if (brushHost) DeleteObject(brushHost);
        if (brushHeader) DeleteObject(brushHeader);
        if (penBorder) DeleteObject(penBorder);
        if (penGrid) DeleteObject(penGrid);
        if (font) DeleteObject(font);
    }

    bool UpdateThemeResources() {
        HBRUSH newBrushHost = CreateSolidBrush(owner->theme_.tableBackground);
        HBRUSH newBrushHeader = CreateSolidBrush(owner->theme_.tableHeaderBackground);
        HPEN newPenBorder = CreatePen(PS_SOLID, 1, owner->theme_.border);
        HPEN newPenGrid = CreatePen(PS_SOLID, 1, owner->theme_.tableGrid);
        HFONT newFont = CreateFont(owner->theme_.uiFont);
        if (!newBrushHost || !newBrushHeader || !newPenBorder || !newPenGrid || !newFont) {
            if (newBrushHost) DeleteObject(newBrushHost);
            if (newBrushHeader) DeleteObject(newBrushHeader);
            if (newPenBorder) DeleteObject(newPenBorder);
            if (newPenGrid) DeleteObject(newPenGrid);
            if (newFont) DeleteObject(newFont);
            return false;
        }

        if (brushHost) DeleteObject(brushHost);
        if (brushHeader) DeleteObject(brushHeader);
        if (penBorder) DeleteObject(penBorder);
        if (penGrid) DeleteObject(penGrid);
        if (font) DeleteObject(font);

        brushHost = newBrushHost;
        brushHeader = newBrushHeader;
        penBorder = newPenBorder;
        penGrid = newPenGrid;
        font = newFont;

        if (owner->listHwnd_) {
            SendMessageW(owner->listHwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            ListView_SetBkColor(owner->listHwnd_, owner->theme_.tableBackground);
            ListView_SetTextBkColor(owner->listHwnd_, owner->theme_.tableBackground);
            ListView_SetTextColor(owner->listHwnd_, owner->theme_.tableText);
            InvalidateRect(owner->listHwnd_, nullptr, TRUE);
        }
        if (owner->headerHwnd_) {
            SendMessageW(owner->headerHwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            InvalidateRect(owner->headerHwnd_, nullptr, TRUE);
        }
        if (owner->hostHwnd_) {
            InvalidateRect(owner->hostHwnd_, nullptr, TRUE);
        }
        return true;
    }

    RECT ClientRect(HWND window) const {
        RECT rect{};
        GetClientRect(window, &rect);
        return rect;
    }

    RECT ContentRect(HWND window) const {
        RECT rect = ClientRect(window);
        rect.left += 1;
        rect.top += 1;
        rect.right -= 1;
        rect.bottom -= 1;
        return rect;
    }

    void LayoutChildren() {
        if (!owner->hostHwnd_ || !owner->listHwnd_) {
            return;
        }
        RECT rect = ContentRect(owner->hostHwnd_);
        MoveWindow(owner->listHwnd_,
                   rect.left,
                   rect.top,
                   std::max(1, static_cast<int>(rect.right - rect.left)),
                   std::max(1, static_cast<int>(rect.bottom - rect.top)),
                   FALSE);
    }

    void SyncColumnsToWindow() {
        if (!owner->listHwnd_) {
            return;
        }
        while (ListView_DeleteColumn(owner->listHwnd_, 0)) {
        }
        for (std::size_t index = 0; index < columns.size(); ++index) {
            LVCOLUMNW column{};
            column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT | LVCF_SUBITEM;
            column.pszText = const_cast<LPWSTR>(columns[index].text.c_str());
            column.cx = std::max(24, columns[index].width);
            column.fmt = columns[index].format;
            column.iSubItem = static_cast<int>(index);
            ListView_InsertColumn(owner->listHwnd_, static_cast<int>(index), &column);
        }
        owner->headerHwnd_ = ListView_GetHeader(owner->listHwnd_);
    }

    void SyncColumnWidthsFromControl() {
        if (!owner->listHwnd_) {
            return;
        }
        for (std::size_t index = 0; index < columns.size(); ++index) {
            const int width = ListView_GetColumnWidth(owner->listHwnd_, static_cast<int>(index));
            if (width > 0) {
                columns[index].width = width;
            }
        }
    }

    void SyncRowsToWindow() {
        if (!owner->listHwnd_) {
            return;
        }
        std::vector<int> selected;
        const int focused = ListView_GetNextItem(owner->listHwnd_, -1, LVNI_FOCUSED);
        for (int index = ListView_GetNextItem(owner->listHwnd_, -1, LVNI_SELECTED);
             index != -1;
             index = ListView_GetNextItem(owner->listHwnd_, index, LVNI_SELECTED)) {
            selected.push_back(index);
        }
        ListView_DeleteAllItems(owner->listHwnd_);
        for (std::size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
            LVITEMW item{};
            item.mask = LVIF_TEXT;
            item.iItem = static_cast<int>(rowIndex);
            std::wstring firstCell = rows[rowIndex].empty() ? L"" : rows[rowIndex].front();
            item.pszText = const_cast<LPWSTR>(firstCell.c_str());
            const int inserted = ListView_InsertItem(owner->listHwnd_, &item);
            if (inserted < 0) {
                continue;
            }
            for (std::size_t columnIndex = 1; columnIndex < rows[rowIndex].size(); ++columnIndex) {
                ListView_SetItemText(owner->listHwnd_,
                                     inserted,
                                     static_cast<int>(columnIndex),
                                     const_cast<LPWSTR>(rows[rowIndex][columnIndex].c_str()));
            }
        }
        for (int index : selected) {
            if (index >= 0 && index < static_cast<int>(rows.size())) {
                ListView_SetItemState(owner->listHwnd_, index, LVIS_SELECTED, LVIS_SELECTED);
            }
        }
        if (focused >= 0 && focused < static_cast<int>(rows.size())) {
            ListView_SetItemState(owner->listHwnd_, focused, LVIS_FOCUSED, LVIS_FOCUSED);
            ListView_EnsureVisible(owner->listHwnd_, focused, FALSE);
        }
    }

    void PaintHost(HDC dc, HWND window) {
        RECT rect = ClientRect(window);
        HGDIOBJ oldBrush = SelectObject(dc, brushHost);
        HGDIOBJ oldPen = SelectObject(dc, penBorder);
        Rectangle(dc, rect.left, rect.top, rect.right + 1, rect.bottom + 1);
        SelectObject(dc, oldPen);
        SelectObject(dc, oldBrush);
    }

    void PaintHeader(HDC dc) {
        if (!owner->headerHwnd_) {
            return;
        }
        RECT client = ClientRect(owner->headerHwnd_);
        FillRect(dc, &client, brushHeader);

        HFONT oldFont = font ? reinterpret_cast<HFONT>(SelectObject(dc, font)) : nullptr;
        HGDIOBJ oldPen = SelectObject(dc, penGrid);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, owner->theme_.tableHeaderText);

        const int itemCount = Header_GetItemCount(owner->headerHwnd_);
        for (int index = 0; index < itemCount; ++index) {
            RECT itemRect{};
            if (!Header_GetItemRect(owner->headerHwnd_, index, &itemRect)) {
                continue;
            }

            HDITEMW item{};
            wchar_t text[128]{};
            item.mask = HDI_TEXT | HDI_FORMAT;
            item.pszText = text;
            item.cchTextMax = static_cast<int>(std::size(text));
            Header_GetItem(owner->headerHwnd_, index, &item);

            RECT textRect = itemRect;
            textRect.left += owner->theme_.textPadding;
            textRect.right -= owner->theme_.textPadding;

            UINT format = DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX;
            if ((item.fmt & HDF_CENTER) != 0) {
                format |= DT_CENTER;
            } else if ((item.fmt & HDF_RIGHT) != 0) {
                format |= DT_RIGHT;
            } else {
                format |= DT_LEFT;
            }
            DrawTextW(dc, text, -1, &textRect, format);

            MoveToEx(dc, itemRect.right - 1, itemRect.top + 4, nullptr);
            LineTo(dc, itemRect.right - 1, itemRect.bottom - 1);
            MoveToEx(dc, itemRect.left, itemRect.bottom - 1, nullptr);
            LineTo(dc, itemRect.right, itemRect.bottom - 1);
        }

        SelectObject(dc, oldPen);
        if (oldFont) {
            SelectObject(dc, oldFont);
        }
    }

    void PaintListGrid(HDC dc) {
        if (!owner->listHwnd_ || !penGrid) {
            return;
        }

        RECT client = ClientRect(owner->listHwnd_);
        const int rowCount = ListView_GetItemCount(owner->listHwnd_);
        if (rowCount <= 0) {
            return;
        }

        const int topIndex = std::max(0, ListView_GetTopIndex(owner->listHwnd_));
        const int visibleCount = std::max(1, ListView_GetCountPerPage(owner->listHwnd_)) + 1;
        const int lastIndex = std::min(rowCount - 1, topIndex + visibleCount);

        RECT firstRow{};
        if (!ListView_GetItemRect(owner->listHwnd_, topIndex, &firstRow, LVIR_BOUNDS)) {
            return;
        }

        HGDIOBJ oldPen = SelectObject(dc, penGrid);
        const int gridTop = firstRow.top;

        int x = 0;
        for (std::size_t index = 0; index + 1 < columns.size(); ++index) {
            x += std::max(24, columns[index].width);
            if (x >= client.right) {
                break;
            }
            MoveToEx(dc, x - 1, gridTop, nullptr);
            LineTo(dc, x - 1, client.bottom);
        }

        for (int row = topIndex; row <= lastIndex; ++row) {
            RECT rowRect{};
            if (!ListView_GetItemRect(owner->listHwnd_, row, &rowRect, LVIR_BOUNDS)) {
                continue;
            }
            MoveToEx(dc, client.left, rowRect.bottom - 1, nullptr);
            LineTo(dc, client.right, rowRect.bottom - 1);
        }

        SelectObject(dc, oldPen);
    }

    LRESULT HandleListCustomDraw(NMLVCUSTOMDRAW* drawInfo) {
        switch (drawInfo->nmcd.dwDrawStage) {
        case CDDS_PREPAINT:
            return CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTPAINT;
        case CDDS_ITEMPREPAINT:
            return CDRF_NOTIFYSUBITEMDRAW;
        case CDDS_ITEMPREPAINT | CDDS_SUBITEM:
            drawInfo->clrText = owner->theme_.tableText;
            drawInfo->clrTextBk = owner->theme_.tableBackground;
            return CDRF_NEWFONT;
        case CDDS_POSTPAINT:
            PaintListGrid(drawInfo->nmcd.hdc);
            return CDRF_DODEFAULT;
        default:
            return CDRF_DODEFAULT;
        }
    }

    void SelectAll() {
        if (!owner->listHwnd_) {
            return;
        }
        const int itemCount = ListView_GetItemCount(owner->listHwnd_);
        for (int index = 0; index < itemCount; ++index) {
            ListView_SetItemState(owner->listHwnd_, index, LVIS_SELECTED, LVIS_SELECTED);
        }
    }

    std::wstring BuildSelectedText() const {
        std::wostringstream stream;
        bool firstRow = true;
        for (int index = ListView_GetNextItem(owner->listHwnd_, -1, LVNI_SELECTED);
             index != -1;
             index = ListView_GetNextItem(owner->listHwnd_, index, LVNI_SELECTED)) {
            if (!firstRow) {
                stream << L"\r\n";
            }
            firstRow = false;

            if (index < 0 || index >= static_cast<int>(rows.size())) {
                continue;
            }
            const ListViewRow& row = rows[static_cast<std::size_t>(index)];
            for (std::size_t column = 0; column < row.size(); ++column) {
                if (column > 0) {
                    stream << L'\t';
                }
                stream << row[column];
            }
        }
        return stream.str();
    }

    bool CopySelectionToClipboard() {
        if (!owner->listHwnd_) {
            return false;
        }

        const std::wstring text = BuildSelectedText();
        if (text.empty()) {
            return false;
        }

        if (!OpenClipboard(owner->listHwnd_)) {
            return false;
        }
        EmptyClipboard();

        const std::size_t bytes = (text.size() + 1) * sizeof(wchar_t);
        HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (!handle) {
            CloseClipboard();
            return false;
        }

        void* memory = GlobalLock(handle);
        if (!memory) {
            GlobalFree(handle);
            CloseClipboard();
            return false;
        }
        CopyMemory(memory, text.c_str(), bytes);
        GlobalUnlock(handle);

        if (!SetClipboardData(CF_UNICODETEXT, handle)) {
            GlobalFree(handle);
            CloseClipboard();
            return false;
        }
        CloseClipboard();
        return true;
    }

    void ShowContextMenu(POINT screenPoint) {
        if (!owner->listHwnd_) {
            return;
        }

        if (screenPoint.x == -1 && screenPoint.y == -1) {
            RECT rect{};
            GetWindowRect(owner->listHwnd_, &rect);
            screenPoint.x = rect.left + 24;
            screenPoint.y = rect.top + 24;
        }

        HMENU menu = CreatePopupMenu();
        if (!menu) {
            return;
        }

        AppendMenuW(menu, MF_STRING, kListViewCommandCopy, L"Copy");
        AppendMenuW(menu, MF_STRING, kListViewCommandSelectAll, L"Select All");
        const UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, screenPoint.x, screenPoint.y, 0, owner->listHwnd_, nullptr);
        DestroyMenu(menu);

        if (command == kListViewCommandCopy) {
            CopySelectionToClipboard();
        } else if (command == kListViewCommandSelectAll) {
            SelectAll();
        }
    }

    LRESULT ForwardNotifyToParent(WPARAM wParam, LPARAM lParam) {
        if (!owner->parentHwnd_) {
            return 0;
        }
        return SendMessageW(owner->parentHwnd_, WM_NOTIFY, wParam, lParam);
    }

    static LRESULT CALLBACK HostWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
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
            self->PaintHost(dc, window);
            EndPaint(window, &ps);
            return 0;
        }
        case WM_SIZE:
            self->LayoutChildren();
            return 0;
        case WM_SETFOCUS:
        case WM_LBUTTONDOWN:
            if (self->owner->listHwnd_) {
                SetFocus(self->owner->listHwnd_);
            }
            return 0;
        case WM_CONTEXTMENU:
            if (reinterpret_cast<HWND>(wParam) == self->owner->listHwnd_) {
                POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                self->ShowContextMenu(point);
                return 0;
            }
            break;
        case WM_NOTIFY: {
            auto* hdr = reinterpret_cast<NMHDR*>(lParam);
            if (!hdr) {
                return 0;
            }
            if (hdr->hwndFrom == self->owner->listHwnd_ && hdr->code == NM_CUSTOMDRAW) {
                return self->HandleListCustomDraw(reinterpret_cast<NMLVCUSTOMDRAW*>(lParam));
            }
            if (hdr->hwndFrom == self->owner->headerHwnd_) {
                switch (hdr->code) {
                case HDN_ENDTRACKA:
                case HDN_ENDTRACKW:
                case HDN_ITEMCHANGEDA:
                case HDN_ITEMCHANGEDW:
                    self->SyncColumnWidthsFromControl();
                    if (self->owner->listHwnd_) {
                        InvalidateRect(self->owner->listHwnd_, nullptr, FALSE);
                    }
                    InvalidateRect(self->owner->headerHwnd_, nullptr, FALSE);
                    break;
                default:
                    break;
                }
            }
            return self->ForwardNotifyToParent(wParam, lParam);
        }
        default:
            break;
        }

        return DefWindowProcW(window, message, wParam, lParam);
    }
};

namespace {

ATOM EnsureListViewHostClassRegistered(HINSTANCE instance) {
    static ATOM atom = 0;
    if (atom != 0) {
        return atom;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = ListViewHostWindowProcThunk;
    wc.hInstance = instance;
    wc.lpszClassName = kListViewHostClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    atom = RegisterClassExW(&wc);
    return atom;
}

LRESULT CALLBACK ListViewHostWindowProcThunk(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    return ListView::Impl::HostWindowProc(window, message, wParam, lParam);
}

LRESULT CALLBACK HeaderSubclassProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR refData) {
    auto* self = reinterpret_cast<ListView::Impl*>(refData);
    if (!self) {
        return DefSubclassProc(window, message, wParam, lParam);
    }

    switch (message) {
    case WM_THEMECHANGED:
        InvalidateRect(window, nullptr, TRUE);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(window, &ps);
        self->PaintHeader(dc);
        EndPaint(window, &ps);
        return 0;
    }
    default:
        break;
    }

    return DefSubclassProc(window, message, wParam, lParam);
}

LRESULT CALLBACK ListSubclassProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR refData) {
    auto* self = reinterpret_cast<ListView::Impl*>(refData);
    if (!self) {
        return DefSubclassProc(window, message, wParam, lParam);
    }

    switch (message) {
    case WM_KEYDOWN:
        if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) {
            if (wParam == 'C') {
                self->CopySelectionToClipboard();
                return 0;
            }
            if (wParam == 'A') {
                self->SelectAll();
                return 0;
            }
        }
        break;
    case WM_CONTEXTMENU: {
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        self->ShowContextMenu(point);
        return 0;
    }
    default:
        break;
    }

    return DefSubclassProc(window, message, wParam, lParam);
}

}  // namespace

ListView::ListView() : impl_(std::make_unique<Impl>(this)) {}

ListView::~ListView() {
    Destroy();
}

bool ListView::Create(HWND parent, int controlId, const Theme& theme, const Options& options) {
    Destroy();
    parentHwnd_ = parent;
    controlId_ = controlId;
    theme_ = ResolveTheme(theme);
    impl_->columns = options.columns;
    impl_->rows = options.rows;
    impl_->instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(parent, GWLP_HINSTANCE));
    if (!impl_->instance) {
        impl_->instance = GetModuleHandleW(nullptr);
    }

    if (!EnsureListViewHostClassRegistered(impl_->instance)) {
        Destroy();
        return false;
    }

    hostHwnd_ = CreateWindowExW(0,
                                kListViewHostClassName,
                                L"",
                                WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | ((options.style & WS_TABSTOP) != 0 ? WS_TABSTOP : 0),
                                0,
                                0,
                                0,
                                0,
                                parent,
                                reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
                                impl_->instance,
                                impl_.get());
    if (!hostHwnd_) {
        Destroy();
        return false;
    }

    DWORD listStyle = options.style | WS_CHILD | WS_VISIBLE;
    listStyle &= ~WS_TABSTOP;
    listStyle |= LVS_REPORT;

    listHwnd_ = CreateWindowExW(WS_EX_CLIENTEDGE,
                                WC_LISTVIEWW,
                                L"",
                                listStyle,
                                0,
                                0,
                                0,
                                0,
                                hostHwnd_,
                                reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
                                impl_->instance,
                                nullptr);
    if (!listHwnd_) {
        Destroy();
        return false;
    }

    SetWindowTheme(listHwnd_, L"DarkMode_Explorer", nullptr);
    StripClientEdge(listHwnd_);
    ListView_SetExtendedListViewStyle(listHwnd_, options.exStyle);
    headerHwnd_ = ListView_GetHeader(listHwnd_);
    SetWindowSubclass(listHwnd_, ListSubclassProc, kListSubclassId, reinterpret_cast<DWORD_PTR>(impl_.get()));
    if (headerHwnd_) {
        SetWindowTheme(headerHwnd_, L"", L"");
        SetWindowSubclass(headerHwnd_, HeaderSubclassProc, 1, reinterpret_cast<DWORD_PTR>(impl_.get()));
    }

    if (!impl_->UpdateThemeResources()) {
        Destroy();
        return false;
    }

    impl_->SyncColumnsToWindow();
    impl_->SyncRowsToWindow();
    impl_->LayoutChildren();
    if (options.selection >= 0) {
        SetSelection(options.selection, false);
    }
    return true;
}

void ListView::Destroy() {
    if (headerHwnd_) {
        RemoveWindowSubclass(headerHwnd_, HeaderSubclassProc, 1);
        headerHwnd_ = nullptr;
    }
    if (listHwnd_) {
        RemoveWindowSubclass(listHwnd_, ListSubclassProc, kListSubclassId);
        DestroyWindow(listHwnd_);
        listHwnd_ = nullptr;
    }
    if (hostHwnd_) {
        DestroyWindow(hostHwnd_);
        hostHwnd_ = nullptr;
    }
    parentHwnd_ = nullptr;
    controlId_ = 0;
    if (impl_) {
        impl_->columns.clear();
        impl_->rows.clear();
    }
}

void ListView::SetTheme(const Theme& theme) {
    const Theme previous = theme_;
    theme_ = ResolveTheme(theme);
    if (!impl_->UpdateThemeResources()) {
        theme_ = previous;
        impl_->UpdateThemeResources();
        return;
    }
    if (headerHwnd_) {
        InvalidateRect(headerHwnd_, nullptr, TRUE);
    }
}

void ListView::SetColumns(const std::vector<ListViewColumn>& columns) {
    impl_->columns = columns;
    impl_->SyncColumnsToWindow();
    impl_->SyncRowsToWindow();
}

bool ListView::SetColumnWidth(std::size_t index, int width) {
    if (index >= impl_->columns.size()) {
        return false;
    }
    impl_->columns[index].width = std::max(24, width);
    if (listHwnd_) {
        ListView_SetColumnWidth(listHwnd_, static_cast<int>(index), impl_->columns[index].width);
        if (headerHwnd_) {
            InvalidateRect(headerHwnd_, nullptr, TRUE);
        }
        InvalidateRect(listHwnd_, nullptr, FALSE);
    }
    return true;
}

int ListView::GetColumnWidth(std::size_t index) const {
    if (index >= impl_->columns.size()) {
        return 0;
    }
    return impl_->columns[index].width;
}

void ListView::SetRows(const std::vector<ListViewRow>& rows) {
    impl_->rows = rows;
    impl_->SyncRowsToWindow();
}

void ListView::AddRow(const ListViewRow& row) {
    impl_->rows.push_back(row);
    impl_->SyncRowsToWindow();
}

void ListView::ClearRows() {
    impl_->rows.clear();
    impl_->SyncRowsToWindow();
}

int ListView::GetSelection() const {
    if (!listHwnd_) {
        return -1;
    }
    return ListView_GetNextItem(listHwnd_, -1, LVNI_SELECTED);
}

void ListView::SetSelection(int index, bool notify) {
    if (!listHwnd_) {
        return;
    }
    for (int current = ListView_GetNextItem(listHwnd_, -1, LVNI_SELECTED);
         current != -1;
         current = ListView_GetNextItem(listHwnd_, current, LVNI_SELECTED)) {
        ListView_SetItemState(listHwnd_, current, 0, LVIS_SELECTED | LVIS_FOCUSED);
    }
    if (index >= 0 && index < static_cast<int>(impl_->rows.size())) {
        ListView_SetItemState(listHwnd_, index, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(listHwnd_, index, FALSE);
    }
    if (notify && parentHwnd_) {
        SendMessageW(parentHwnd_,
                     WM_COMMAND,
                     MAKEWPARAM(static_cast<UINT>(controlId_), LBN_SELCHANGE),
                     reinterpret_cast<LPARAM>(listHwnd_));
    }
}

std::size_t ListView::GetRowCount() const {
    return impl_->rows.size();
}

std::size_t ListView::GetColumnCount() const {
    return impl_->columns.size();
}

}  // namespace darkui
