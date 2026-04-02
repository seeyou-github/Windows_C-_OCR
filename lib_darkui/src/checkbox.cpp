#include "darkui/checkbox.h"

#include <commctrl.h>

#include <algorithm>

namespace darkui {

namespace {

constexpr wchar_t kCheckBoxStateProperty[] = L"DarkUiCheckBoxState";

COLORREF MixColor(COLORREF a, COLORREF b, double ratio) {
    ratio = std::clamp(ratio, 0.0, 1.0);
    const auto mix = [ratio](int x, int y) {
        return static_cast<int>(x + (y - x) * ratio + 0.5);
    };
    return RGB(mix(GetRValue(a), GetRValue(b)),
               mix(GetGValue(a), GetGValue(b)),
               mix(GetBValue(a), GetBValue(b)));
}

}

struct CheckBox::Impl {
    CheckBox* owner = nullptr;
    HINSTANCE instance = nullptr;
    HFONT font = nullptr;
    HPEN penBorder = nullptr;
    HPEN penAccent = nullptr;
    HBRUSH brushSurface = nullptr;
    HBRUSH brushBox = nullptr;
    HBRUSH brushBoxHot = nullptr;
    HBRUSH brushAccent = nullptr;
    bool checked = false;
    bool hot = false;
    bool trackingMouseLeave = false;

    explicit Impl(CheckBox* checkbox) : owner(checkbox) {}

    ~Impl() {
        if (font) DeleteObject(font);
        if (penBorder) DeleteObject(penBorder);
        if (penAccent) DeleteObject(penAccent);
        if (brushSurface) DeleteObject(brushSurface);
        if (brushBox) DeleteObject(brushBox);
        if (brushBoxHot) DeleteObject(brushBoxHot);
        if (brushAccent) DeleteObject(brushAccent);
    }

    bool UpdateThemeResources() {
        COLORREF borderColor = owner->theme_.checkBorder;
        COLORREF boxColor = owner->theme_.checkBackground;
        COLORREF boxHotColor = owner->theme_.checkBackgroundHot;
        COLORREF accentColor = owner->theme_.checkAccent;
        COLORREF checkmarkColor = RGB(245, 247, 250);
        switch (owner->variant_) {
        case SelectionVariant::Panel:
            boxColor = MixColor(owner->surfaceColor_, owner->theme_.checkBackground, 0.72);
            boxHotColor = MixColor(owner->surfaceColor_, owner->theme_.checkBackgroundHot, 0.76);
            borderColor = MixColor(owner->surfaceColor_, owner->theme_.checkBorder, 0.78);
            break;
        case SelectionVariant::Accent:
            borderColor = MixColor(owner->theme_.checkAccent, owner->theme_.checkBorder, 0.38);
            boxColor = MixColor(owner->theme_.checkBackground, owner->theme_.checkAccent, 0.14);
            boxHotColor = MixColor(owner->theme_.checkBackgroundHot, owner->theme_.checkAccent, 0.22);
            accentColor = owner->theme_.checkAccent;
            checkmarkColor = owner->theme_.highlightText;
            break;
        case SelectionVariant::Default:
        default:
            break;
        }

        HFONT newFont = CreateFont(owner->theme_.uiFont);
        HPEN newPenBorder = CreatePen(PS_SOLID, 1, borderColor);
        HPEN newPenAccent = CreatePen(PS_SOLID, 2, checkmarkColor);
        HBRUSH newBrushSurface = CreateSolidBrush(owner->surfaceColor_);
        HBRUSH newBrushBox = CreateSolidBrush(boxColor);
        HBRUSH newBrushBoxHot = CreateSolidBrush(boxHotColor);
        HBRUSH newBrushAccent = CreateSolidBrush(accentColor);

        if (!newFont || !newPenBorder || !newPenAccent || !newBrushSurface || !newBrushBox || !newBrushBoxHot || !newBrushAccent) {
            if (newFont) DeleteObject(newFont);
            if (newPenBorder) DeleteObject(newPenBorder);
            if (newPenAccent) DeleteObject(newPenAccent);
            if (newBrushSurface) DeleteObject(newBrushSurface);
            if (newBrushBox) DeleteObject(newBrushBox);
            if (newBrushBoxHot) DeleteObject(newBrushBoxHot);
            if (newBrushAccent) DeleteObject(newBrushAccent);
            return false;
        }

        if (font) DeleteObject(font);
        if (penBorder) DeleteObject(penBorder);
        if (penAccent) DeleteObject(penAccent);
        if (brushSurface) DeleteObject(brushSurface);
        if (brushBox) DeleteObject(brushBox);
        if (brushBoxHot) DeleteObject(brushBoxHot);
        if (brushAccent) DeleteObject(brushAccent);

        font = newFont;
        penBorder = newPenBorder;
        penAccent = newPenAccent;
        brushSurface = newBrushSurface;
        brushBox = newBrushBox;
        brushBoxHot = newBrushBoxHot;
        brushAccent = newBrushAccent;

        if (owner->checkboxHwnd_) {
            SendMessageW(owner->checkboxHwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            InvalidateRect(owner->checkboxHwnd_, nullptr, TRUE);
        }
        return true;
    }

    HFONT SelectControlFont(HDC dc, HWND control) {
        HFONT current = reinterpret_cast<HFONT>(SendMessageW(control, WM_GETFONT, 0, 0));
        return current ? reinterpret_cast<HFONT>(SelectObject(dc, current)) : nullptr;
    }

    void StartMouseLeaveTracking() {
        if (!owner->checkboxHwnd_ || trackingMouseLeave) {
            return;
        }
        TRACKMOUSEEVENT track{};
        track.cbSize = sizeof(track);
        track.dwFlags = TME_LEAVE;
        track.hwndTrack = owner->checkboxHwnd_;
        if (TrackMouseEvent(&track)) {
            trackingMouseLeave = true;
        }
    }

    void Draw(const DRAWITEMSTRUCT* draw) {
        RECT rect = draw->rcItem;
        FillRect(draw->hDC, &rect, brushSurface);

        const bool enabled = IsWindowEnabled(draw->hwndItem) != FALSE;
        const bool pressed = (draw->itemState & ODS_SELECTED) != 0;

        RECT boxRect{rect.left + 2, rect.top + ((rect.bottom - rect.top) - 18) / 2, rect.left + 20, rect.top + ((rect.bottom - rect.top) - 18) / 2 + 18};
        HGDIOBJ oldPen = SelectObject(draw->hDC, checked ? penAccent : penBorder);
        HBRUSH fillBrush = checked ? brushAccent : (hot || pressed ? brushBoxHot : brushBox);
        HGDIOBJ oldBrush = SelectObject(draw->hDC, fillBrush);
        RoundRect(draw->hDC, boxRect.left, boxRect.top, boxRect.right, boxRect.bottom, 6, 6);
        SelectObject(draw->hDC, oldBrush);
        SelectObject(draw->hDC, oldPen);

        if (checked) {
            oldPen = SelectObject(draw->hDC, penAccent);
            MoveToEx(draw->hDC, boxRect.left + 4, boxRect.top + 9, nullptr);
            LineTo(draw->hDC, boxRect.left + 8, boxRect.top + 13);
            LineTo(draw->hDC, boxRect.left + 15, boxRect.top + 5);
            SelectObject(draw->hDC, oldPen);
        }

        RECT textRect = rect;
        textRect.left = boxRect.right + 10;

        HFONT oldFont = SelectControlFont(draw->hDC, draw->hwndItem);
        SetBkMode(draw->hDC, TRANSPARENT);
        SetTextColor(draw->hDC, enabled ? owner->theme_.checkText : owner->theme_.checkDisabledText);

        wchar_t text[1024] = {};
        GetWindowTextW(draw->hwndItem, text, _countof(text));
        DrawTextW(draw->hDC, text, -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
        if (oldFont) {
            SelectObject(draw->hDC, oldFont);
        }
    }

    void ToggleCheckedState() {
        if (!owner->checkboxHwnd_ || !IsWindowEnabled(owner->checkboxHwnd_)) {
            return;
        }
        checked = !checked;
        SetPropW(owner->checkboxHwnd_, kCheckBoxStateProperty, reinterpret_cast<HANDLE>(checked ? 1 : 0));
        InvalidateRect(owner->checkboxHwnd_, nullptr, FALSE);
    }

    void NotifyClicked() {
        if (!owner->parentHwnd_ || !owner->checkboxHwnd_) {
            return;
        }
        SendMessageW(owner->parentHwnd_,
                     WM_COMMAND,
                     MAKEWPARAM(static_cast<UINT>(owner->controlId_), BN_CLICKED),
                     reinterpret_cast<LPARAM>(owner->checkboxHwnd_));
    }

    static LRESULT CALLBACK ControlSubclassProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassId, DWORD_PTR refData) {
        auto* self = reinterpret_cast<Impl*>(refData);
        if (!self) {
            return DefSubclassProc(window, message, wParam, lParam);
        }

        switch (message) {
        case WM_ERASEBKGND:
            return 1;
        case WM_MOUSEMOVE:
            if (!self->hot) {
                self->hot = true;
                InvalidateRect(window, nullptr, FALSE);
            }
            self->StartMouseLeaveTracking();
            break;
        case WM_MOUSELEAVE:
            self->hot = false;
            self->trackingMouseLeave = false;
            InvalidateRect(window, nullptr, FALSE);
            break;
        case WM_ENABLE:
            self->hot = false;
            self->trackingMouseLeave = false;
            InvalidateRect(window, nullptr, FALSE);
            break;
        case WM_LBUTTONDOWN:
            SetFocus(window);
            return 0;
        case WM_LBUTTONUP:
            self->ToggleCheckedState();
            self->NotifyClicked();
            return 0;
        case WM_KEYUP:
            if (wParam == VK_SPACE) {
                self->ToggleCheckedState();
                self->NotifyClicked();
                return 0;
            }
            break;
        case BM_SETCHECK: {
            self->checked = (wParam == BST_CHECKED);
            SetPropW(window, kCheckBoxStateProperty, reinterpret_cast<HANDLE>(self->checked ? 1 : 0));
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        case BM_GETCHECK:
            return self->checked ? BST_CHECKED : BST_UNCHECKED;
        case BM_CLICK:
            self->ToggleCheckedState();
            self->NotifyClicked();
            return 0;
        case WM_DESTROY:
            RemovePropW(window, kCheckBoxStateProperty);
            RemoveWindowSubclass(window, ControlSubclassProc, subclassId);
            break;
        default:
            break;
        }

        return DefSubclassProc(window, message, wParam, lParam);
    }

    static LRESULT CALLBACK ParentSubclassProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassId, DWORD_PTR refData) {
        auto* self = reinterpret_cast<Impl*>(refData);
        if (!self) {
            return DefSubclassProc(window, message, wParam, lParam);
        }

        switch (message) {
        case WM_DRAWITEM: {
            auto* draw = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            if (draw && draw->hwndItem == self->owner->checkboxHwnd_) {
                self->Draw(draw);
                return TRUE;
            }
            break;
        }
        case WM_DESTROY:
            RemoveWindowSubclass(window, ParentSubclassProc, subclassId);
            break;
        default:
            break;
        }

        return DefSubclassProc(window, message, wParam, lParam);
    }
};

CheckBox::CheckBox() : impl_(std::make_unique<Impl>(this)) {}

CheckBox::~CheckBox() {
    Destroy();
}

bool CheckBox::Create(HWND parent, int controlId, const Theme& theme, const Options& options) {
    Destroy();
    parentHwnd_ = parent;
    controlId_ = controlId;
    theme_ = ResolveTheme(theme);
    surfaceRole_ = options.surfaceRole;
    variant_ = options.variant;
    hasCustomSurfaceColor_ = options.surfaceColor != CLR_INVALID;
    surfaceColor_ = hasCustomSurfaceColor_ ? options.surfaceColor : ResolveInheritedSurfaceColor(theme_, parent, surfaceRole_);
    impl_->instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(parent, GWLP_HINSTANCE));
    if (!impl_->instance) {
        impl_->instance = GetModuleHandleW(nullptr);
    }

    if (!impl_->UpdateThemeResources()) {
        Destroy();
        return false;
    }

    DWORD style = options.style;
    style &= ~BS_TYPEMASK;
    style |= BS_OWNERDRAW;
    checkboxHwnd_ = CreateWindowExW(options.exStyle,
                                    L"BUTTON",
                                    options.text.c_str(),
                                    style,
                                    0,
                                    0,
                                    0,
                                    0,
                                    parent,
                                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
                                    impl_->instance,
                                    nullptr);
    if (!checkboxHwnd_) {
        Destroy();
        return false;
    }

    SendMessageW(checkboxHwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(impl_->font), TRUE);
    SetPropW(checkboxHwnd_, kCheckBoxStateProperty, reinterpret_cast<HANDLE>(0));
    SetWindowSubclass(checkboxHwnd_,
                      Impl::ControlSubclassProc,
                      reinterpret_cast<UINT_PTR>(this),
                      reinterpret_cast<DWORD_PTR>(impl_.get()));
    SetWindowSubclass(parentHwnd_,
                      Impl::ParentSubclassProc,
                      reinterpret_cast<UINT_PTR>(this),
                      reinterpret_cast<DWORD_PTR>(impl_.get()));
    if (options.checked) {
        SetChecked(true);
    }
    return true;
}

void CheckBox::Destroy() {
    if (parentHwnd_) {
        RemoveWindowSubclass(parentHwnd_, Impl::ParentSubclassProc, reinterpret_cast<UINT_PTR>(this));
    }
    if (checkboxHwnd_) {
        RemovePropW(checkboxHwnd_, kCheckBoxStateProperty);
        RemoveWindowSubclass(checkboxHwnd_, Impl::ControlSubclassProc, reinterpret_cast<UINT_PTR>(this));
        DestroyWindow(checkboxHwnd_);
        checkboxHwnd_ = nullptr;
    }
    parentHwnd_ = nullptr;
    controlId_ = 0;
    impl_->hot = false;
    impl_->trackingMouseLeave = false;
}

void CheckBox::SetTheme(const Theme& theme) {
    const Theme previousTheme = theme_;
    const COLORREF previousSurfaceColor = surfaceColor_;
    theme_ = ResolveTheme(theme);
    if (!hasCustomSurfaceColor_) {
        surfaceColor_ = ResolveInheritedSurfaceColor(theme_, parentHwnd_, surfaceRole_);
    }
    if (!impl_->UpdateThemeResources()) {
        theme_ = previousTheme;
        surfaceColor_ = previousSurfaceColor;
        impl_->UpdateThemeResources();
    }
}

void CheckBox::SetText(const std::wstring& text) {
    if (checkboxHwnd_) {
        SetWindowTextW(checkboxHwnd_, text.c_str());
        InvalidateRect(checkboxHwnd_, nullptr, TRUE);
    }
}

std::wstring CheckBox::GetText() const {
    if (!checkboxHwnd_) {
        return {};
    }
    const int length = GetWindowTextLengthW(checkboxHwnd_);
    if (length <= 0) {
        return {};
    }
    std::wstring text(length + 1, L'\0');
    GetWindowTextW(checkboxHwnd_, text.data(), length + 1);
    text.resize(length);
    return text;
}

bool CheckBox::GetChecked() const {
    return impl_ && impl_->checked;
}

void CheckBox::SetChecked(bool checked) {
    if (impl_) {
        impl_->checked = checked;
    }
    if (checkboxHwnd_) {
        SetPropW(checkboxHwnd_, kCheckBoxStateProperty, reinterpret_cast<HANDLE>(checked ? 1 : 0));
        InvalidateRect(checkboxHwnd_, nullptr, FALSE);
    }
}

void CheckBox::SetSurfaceColor(COLORREF color) {
    const COLORREF previousSurfaceColor = surfaceColor_;
    hasCustomSurfaceColor_ = true;
    surfaceColor_ = color;
    if (!impl_->UpdateThemeResources()) {
        surfaceColor_ = previousSurfaceColor;
        impl_->UpdateThemeResources();
    }
}

}  // namespace darkui
