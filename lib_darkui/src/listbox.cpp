#include "darkui/listbox.h"

#include <commctrl.h>
#include <uxtheme.h>

#include <algorithm>

namespace darkui {
namespace {

constexpr wchar_t kListBoxHostClassName[] = L"DarkUiListBoxHost";

int ResolveListBoxCornerRadius(FieldVariant variant) {
    switch (variant) {
    case FieldVariant::Panel:
        return 16;
    case FieldVariant::Dense:
        return 12;
    case FieldVariant::Default:
    default:
        return 18;
    }
}

int ResolveListBoxItemHeight(const Theme& theme, FieldVariant variant) {
    switch (variant) {
    case FieldVariant::Dense:
        return std::max(20, theme.listBoxItemHeight - 4);
    case FieldVariant::Panel:
        return std::max(22, theme.listBoxItemHeight - 2);
    case FieldVariant::Default:
    default:
        return std::max(22, theme.listBoxItemHeight);
    }
}

ATOM EnsureListBoxHostClassRegistered(HINSTANCE instance);
LRESULT CALLBACK ListBoxHostWindowProcThunk(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

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

struct ListBox::Impl {
    ListBox* owner = nullptr;
    HINSTANCE instance = nullptr;
    HBRUSH brushHost = nullptr;
    HBRUSH brushPanel = nullptr;
    HBRUSH brushSelected = nullptr;
    HPEN penBorder = nullptr;
    HFONT font = nullptr;
    std::vector<ListBoxItem> items;

    explicit Impl(ListBox* listBox) : owner(listBox) {}

    ~Impl() {
        if (brushHost) DeleteObject(brushHost);
        if (brushPanel) DeleteObject(brushPanel);
        if (brushSelected) DeleteObject(brushSelected);
        if (penBorder) DeleteObject(penBorder);
        if (font) DeleteObject(font);
    }

    bool UpdateThemeResources() {
        HBRUSH newHost = CreateSolidBrush(owner->theme_.listBoxBackground);
        HBRUSH newPanel = CreateSolidBrush(owner->theme_.listBoxPanel);
        HBRUSH newSelected = CreateSolidBrush(owner->theme_.listBoxItemSelected);
        HPEN newBorder = CreatePen(PS_SOLID, 1, owner->theme_.border);
        HFONT newFont = CreateFont(owner->theme_.uiFont);
        if (!newHost || !newPanel || !newSelected || !newBorder || !newFont) {
            if (newHost) DeleteObject(newHost);
            if (newPanel) DeleteObject(newPanel);
            if (newSelected) DeleteObject(newSelected);
            if (newBorder) DeleteObject(newBorder);
            if (newFont) DeleteObject(newFont);
            return false;
        }

        if (brushHost) DeleteObject(brushHost);
        if (brushPanel) DeleteObject(brushPanel);
        if (brushSelected) DeleteObject(brushSelected);
        if (penBorder) DeleteObject(penBorder);
        if (font) DeleteObject(font);

        brushHost = newHost;
        brushPanel = newPanel;
        brushSelected = newSelected;
        penBorder = newBorder;
        font = newFont;

        if (owner->listHwnd_) {
            SendMessageW(owner->listHwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            InvalidateRect(owner->listHwnd_, nullptr, TRUE);
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

    void UpdateWindowRegion() {
        if (!owner->hostHwnd_) {
            return;
        }
        RECT rect = ClientRect(owner->hostHwnd_);
        if (owner->cornerRadius_ <= 0) {
            SetWindowRgn(owner->hostHwnd_, nullptr, TRUE);
            return;
        }
        HRGN region = CreateRoundRectRgn(rect.left,
                                         rect.top,
                                         rect.right + 1,
                                         rect.bottom + 1,
                                         owner->cornerRadius_ * 2,
                                         owner->cornerRadius_ * 2);
        SetWindowRgn(owner->hostHwnd_, region, TRUE);
    }

    void LayoutChildren() {
        if (!owner->hostHwnd_ || !owner->listHwnd_) {
            return;
        }
        RECT rect = ContentRect(owner->hostHwnd_);
        const int width = std::max(1, static_cast<int>(rect.right - rect.left));
        const int height = std::max(1, static_cast<int>(rect.bottom - rect.top));
        MoveWindow(owner->listHwnd_, rect.left, rect.top, width, height, FALSE);
    }

    void SyncItemsToWindow() {
        if (!owner->listHwnd_) {
            return;
        }
        SendMessageW(owner->listHwnd_, WM_SETREDRAW, FALSE, 0);
        SendMessageW(owner->listHwnd_, LB_RESETCONTENT, 0, 0);
        for (std::size_t index = 0; index < items.size(); ++index) {
            const ListBoxItem& item = items[index];
            LRESULT added = SendMessageW(owner->listHwnd_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item.text.c_str()));
            if (added >= 0) {
                SendMessageW(owner->listHwnd_, LB_SETITEMDATA, added, static_cast<LPARAM>(item.userData));
            }
        }
        SendMessageW(owner->listHwnd_, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(owner->listHwnd_, nullptr, TRUE);
    }

    void NotifySelection() {
        if (!owner->parentHwnd_) {
            return;
        }
        SendMessageW(owner->parentHwnd_,
                     WM_COMMAND,
                     MAKEWPARAM(static_cast<UINT>(owner->controlId_), LBN_SELCHANGE),
                     reinterpret_cast<LPARAM>(owner->listHwnd_));
    }

    void PaintHost(HDC dc, HWND window) {
        RECT rect = ClientRect(window);
        HGDIOBJ oldBrush = SelectObject(dc, brushHost);
        HGDIOBJ oldPen = SelectObject(dc, penBorder);
        if (owner->cornerRadius_ > 0) {
            RoundRect(dc, rect.left, rect.top, rect.right + 1, rect.bottom + 1, owner->cornerRadius_ * 2, owner->cornerRadius_ * 2);
        } else {
            Rectangle(dc, rect.left, rect.top, rect.right + 1, rect.bottom + 1);
        }
        SelectObject(dc, oldPen);
        SelectObject(dc, oldBrush);
    }

    void DrawItem(const DRAWITEMSTRUCT* draw) {
        if (!draw || draw->itemID == static_cast<UINT>(-1)) {
            return;
        }

        const bool selected = (draw->itemState & ODS_SELECTED) != 0;
        FillRect(draw->hDC, &draw->rcItem, selected ? brushSelected : brushPanel);

        std::wstring text;
        if (draw->itemID < items.size()) {
            text = items[draw->itemID].text;
        } else {
            const int length = static_cast<int>(SendMessageW(draw->hwndItem, LB_GETTEXTLEN, draw->itemID, 0));
            if (length > 0) {
                text.resize(length + 1);
                SendMessageW(draw->hwndItem, LB_GETTEXT, draw->itemID, reinterpret_cast<LPARAM>(text.data()));
                text.resize(length);
            }
        }

        RECT textRect = draw->rcItem;
        textRect.left += owner->theme_.textPadding;
        textRect.right -= owner->theme_.textPadding;

        HFONT oldFont = font ? reinterpret_cast<HFONT>(SelectObject(draw->hDC, font)) : nullptr;
        SetBkMode(draw->hDC, TRANSPARENT);
        SetTextColor(draw->hDC, selected ? owner->theme_.listBoxItemSelectedText : owner->theme_.listBoxText);
        DrawTextW(draw->hDC, text.c_str(), -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
        if (oldFont) {
            SelectObject(draw->hDC, oldFont);
        }

        if ((draw->itemState & ODS_FOCUS) != 0) {
            DrawFocusRect(draw->hDC, &draw->rcItem);
        }
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
            self->UpdateWindowRegion();
            self->LayoutChildren();
            return 0;
        case WM_SETFOCUS:
        case WM_LBUTTONDOWN:
            if (self->owner->listHwnd_) {
                SetFocus(self->owner->listHwnd_);
            }
            return 0;
        case WM_MEASUREITEM: {
            auto* measure = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
            if (measure && measure->CtlType == ODT_LISTBOX) {
                measure->itemHeight = std::max(1, self->owner->itemHeight_);
                return TRUE;
            }
            break;
        }
        case WM_DRAWITEM: {
            auto* draw = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            if (draw && draw->hwndItem == self->owner->listHwnd_) {
                self->DrawItem(draw);
                return TRUE;
            }
            break;
        }
        case WM_CTLCOLORLISTBOX:
            if (reinterpret_cast<HWND>(lParam) == self->owner->listHwnd_) {
                HDC dc = reinterpret_cast<HDC>(wParam);
                SetBkMode(dc, OPAQUE);
                SetBkColor(dc, self->owner->theme_.listBoxPanel);
                SetTextColor(dc, self->owner->theme_.listBoxText);
                return reinterpret_cast<LRESULT>(self->brushPanel);
            }
            break;
        case WM_COMMAND:
            if (reinterpret_cast<HWND>(lParam) == self->owner->listHwnd_) {
                const UINT code = HIWORD(wParam);
                if (code == LBN_SELCHANGE || code == LBN_DBLCLK || code == LBN_SETFOCUS || code == LBN_KILLFOCUS) {
                    if (self->owner->parentHwnd_) {
                        SendMessageW(self->owner->parentHwnd_,
                                     WM_COMMAND,
                                     MAKEWPARAM(static_cast<UINT>(self->owner->controlId_), code),
                                     reinterpret_cast<LPARAM>(self->owner->listHwnd_));
                    }
                    return 0;
                }
            }
            break;
        default:
            break;
        }

        return DefWindowProcW(window, message, wParam, lParam);
    }
};

namespace {

ATOM EnsureListBoxHostClassRegistered(HINSTANCE instance) {
    static ATOM atom = 0;
    if (atom != 0) {
        return atom;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = ListBoxHostWindowProcThunk;
    wc.hInstance = instance;
    wc.lpszClassName = kListBoxHostClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    atom = RegisterClassExW(&wc);
    return atom;
}

LRESULT CALLBACK ListBoxHostWindowProcThunk(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    return ListBox::Impl::HostWindowProc(window, message, wParam, lParam);
}

}  // namespace

ListBox::ListBox() : impl_(std::make_unique<Impl>(this)) {}

ListBox::~ListBox() {
    Destroy();
}

bool ListBox::Create(HWND parent, int controlId, const Theme& theme, const Options& options) {
    Destroy();
    parentHwnd_ = parent;
    controlId_ = controlId;
    theme_ = ResolveTheme(theme);
    variant_ = options.variant;
    itemHeight_ = ResolveListBoxItemHeight(theme_, variant_);
    impl_->instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(parent, GWLP_HINSTANCE));
    if (!impl_->instance) {
        impl_->instance = GetModuleHandleW(nullptr);
    }

    if (!EnsureListBoxHostClassRegistered(impl_->instance)) {
        Destroy();
        return false;
    }

    hostHwnd_ = CreateWindowExW(options.exStyle & ~WS_EX_CLIENTEDGE,
                                kListBoxHostClassName,
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

    DWORD listStyle = options.style | WS_CHILD | WS_VISIBLE | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS;
    listStyle &= ~WS_BORDER;
    listStyle &= ~WS_TABSTOP;

    listHwnd_ = CreateWindowExW(0,
                                L"LISTBOX",
                                nullptr,
                                listStyle,
                                0,
                                0,
                                0,
                                0,
                                hostHwnd_,
                                nullptr,
                                impl_->instance,
                                nullptr);
    if (!listHwnd_) {
        Destroy();
        return false;
    }

    SetWindowTheme(listHwnd_, L"", L"");
    StripClientEdge(listHwnd_);

    if (!impl_->UpdateThemeResources()) {
        Destroy();
        return false;
    }

    SetCornerRadius(options.cornerRadius >= 0 ? options.cornerRadius : ResolveListBoxCornerRadius(variant_));
    if (!options.items.empty()) {
        SetItems(options.items);
    }
    if (options.selection >= 0) {
        SetSelection(options.selection);
    }
    impl_->UpdateWindowRegion();
    impl_->LayoutChildren();
    return true;
}

void ListBox::Destroy() {
    if (listHwnd_) {
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
        impl_->items.clear();
    }
}

void ListBox::SetTheme(const Theme& theme) {
    const Theme previous = theme_;
    theme_ = ResolveTheme(theme);
    itemHeight_ = ResolveListBoxItemHeight(theme_, variant_);
    if (!impl_->UpdateThemeResources()) {
        theme_ = previous;
        itemHeight_ = ResolveListBoxItemHeight(theme_, variant_);
        impl_->UpdateThemeResources();
        return;
    }
    if (hostHwnd_) {
        InvalidateRect(hostHwnd_, nullptr, TRUE);
    }
    if (listHwnd_) {
        SendMessageW(listHwnd_, LB_SETITEMHEIGHT, 0, itemHeight_);
        InvalidateRect(listHwnd_, nullptr, TRUE);
    }
}

void ListBox::SetItems(const std::vector<ListBoxItem>& items) {
    impl_->items = items;
    impl_->SyncItemsToWindow();
}

void ListBox::AddItem(const ListBoxItem& item) {
    impl_->items.push_back(item);
    impl_->SyncItemsToWindow();
}

void ListBox::ClearItems() {
    impl_->items.clear();
    impl_->SyncItemsToWindow();
}

std::size_t ListBox::GetCount() const {
    return impl_->items.size();
}

int ListBox::GetSelection() const {
    if (!listHwnd_) {
        return -1;
    }
    return static_cast<int>(SendMessageW(listHwnd_, LB_GETCURSEL, 0, 0));
}

std::vector<int> ListBox::GetSelections() const {
    std::vector<int> selections;
    if (!listHwnd_) {
        return selections;
    }

    const LONG_PTR style = GetWindowLongPtrW(listHwnd_, GWL_STYLE);
    if ((style & (LBS_MULTIPLESEL | LBS_EXTENDEDSEL)) == 0) {
        const int single = GetSelection();
        if (single >= 0) {
            selections.push_back(single);
        }
        return selections;
    }

    const int count = static_cast<int>(SendMessageW(listHwnd_, LB_GETSELCOUNT, 0, 0));
    if (count <= 0) {
        return selections;
    }

    selections.resize(count);
    SendMessageW(listHwnd_, LB_GETSELITEMS, count, reinterpret_cast<LPARAM>(selections.data()));
    return selections;
}

void ListBox::SetSelection(int index, bool notify) {
    if (!listHwnd_) {
        return;
    }

    const LONG_PTR style = GetWindowLongPtrW(listHwnd_, GWL_STYLE);
    if ((style & (LBS_MULTIPLESEL | LBS_EXTENDEDSEL)) != 0) {
        SendMessageW(listHwnd_, LB_SETSEL, index >= 0 ? TRUE : FALSE, index);
    } else {
        SendMessageW(listHwnd_, LB_SETCURSEL, index, 0);
    }

    if (notify) {
        impl_->NotifySelection();
    }
}

ListBoxItem ListBox::GetItem(int index) const {
    if (index < 0 || index >= static_cast<int>(impl_->items.size())) {
        return {};
    }
    return impl_->items[index];
}

void ListBox::SetCornerRadius(int radius) {
    cornerRadius_ = std::max(0, radius);
    if (impl_) {
        impl_->UpdateWindowRegion();
        impl_->LayoutChildren();
    }
    if (hostHwnd_) {
        InvalidateRect(hostHwnd_, nullptr, TRUE);
    }
}

}  // namespace darkui
