#include "darkui/radiobutton.h"

#include <commctrl.h>

#include <algorithm>

namespace darkui {

namespace {

constexpr wchar_t kRadioMarkerProperty[] = L"DarkUiRadioButtonMarker";
constexpr wchar_t kRadioStateProperty[] = L"DarkUiRadioButtonState";

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

struct RadioButton::Impl {
    RadioButton* owner = nullptr;
    HINSTANCE instance = nullptr;
    HFONT font = nullptr;
    HPEN penBorder = nullptr;
    HBRUSH brushSurface = nullptr;
    HBRUSH brushCircle = nullptr;
    HBRUSH brushCircleHot = nullptr;
    HBRUSH brushDot = nullptr;
    bool checked = false;
    bool hot = false;
    bool trackingMouseLeave = false;

    explicit Impl(RadioButton* radio) : owner(radio) {}

    ~Impl() {
        if (font) DeleteObject(font);
        if (penBorder) DeleteObject(penBorder);
        if (brushSurface) DeleteObject(brushSurface);
        if (brushCircle) DeleteObject(brushCircle);
        if (brushCircleHot) DeleteObject(brushCircleHot);
        if (brushDot) DeleteObject(brushDot);
    }

    bool UpdateThemeResources() {
        COLORREF borderColor = owner->theme_.radioBorder;
        COLORREF circleColor = owner->theme_.radioBackground;
        COLORREF circleHotColor = owner->theme_.radioBackgroundHot;
        COLORREF dotColor = owner->theme_.radioAccent;
        switch (owner->variant_) {
        case SelectionVariant::Panel:
            circleColor = MixColor(owner->surfaceColor_, owner->theme_.radioBackground, 0.72);
            circleHotColor = MixColor(owner->surfaceColor_, owner->theme_.radioBackgroundHot, 0.76);
            borderColor = MixColor(owner->surfaceColor_, owner->theme_.radioBorder, 0.78);
            break;
        case SelectionVariant::Accent:
            borderColor = MixColor(owner->theme_.radioAccent, owner->theme_.radioBorder, 0.38);
            circleColor = MixColor(owner->theme_.radioBackground, owner->theme_.radioAccent, 0.12);
            circleHotColor = MixColor(owner->theme_.radioBackgroundHot, owner->theme_.radioAccent, 0.20);
            dotColor = owner->theme_.radioAccent;
            break;
        case SelectionVariant::Default:
        default:
            break;
        }

        HFONT newFont = CreateFont(owner->theme_.uiFont);
        HPEN newPenBorder = CreatePen(PS_SOLID, 1, borderColor);
        HBRUSH newBrushSurface = CreateSolidBrush(owner->surfaceColor_);
        HBRUSH newBrushCircle = CreateSolidBrush(circleColor);
        HBRUSH newBrushCircleHot = CreateSolidBrush(circleHotColor);
        HBRUSH newBrushDot = CreateSolidBrush(dotColor);

        if (!newFont || !newPenBorder || !newBrushSurface || !newBrushCircle || !newBrushCircleHot || !newBrushDot) {
            if (newFont) DeleteObject(newFont);
            if (newPenBorder) DeleteObject(newPenBorder);
            if (newBrushSurface) DeleteObject(newBrushSurface);
            if (newBrushCircle) DeleteObject(newBrushCircle);
            if (newBrushCircleHot) DeleteObject(newBrushCircleHot);
            if (newBrushDot) DeleteObject(newBrushDot);
            return false;
        }

        if (font) DeleteObject(font);
        if (penBorder) DeleteObject(penBorder);
        if (brushSurface) DeleteObject(brushSurface);
        if (brushCircle) DeleteObject(brushCircle);
        if (brushCircleHot) DeleteObject(brushCircleHot);
        if (brushDot) DeleteObject(brushDot);

        font = newFont;
        penBorder = newPenBorder;
        brushSurface = newBrushSurface;
        brushCircle = newBrushCircle;
        brushCircleHot = newBrushCircleHot;
        brushDot = newBrushDot;

        if (owner->radioHwnd_) {
            SendMessageW(owner->radioHwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            InvalidateRect(owner->radioHwnd_, nullptr, TRUE);
        }
        return true;
    }

    HFONT SelectControlFont(HDC dc, HWND control) {
        HFONT current = reinterpret_cast<HFONT>(SendMessageW(control, WM_GETFONT, 0, 0));
        return current ? reinterpret_cast<HFONT>(SelectObject(dc, current)) : nullptr;
    }

    void StartMouseLeaveTracking() {
        if (!owner->radioHwnd_ || trackingMouseLeave) {
            return;
        }
        TRACKMOUSEEVENT track{};
        track.cbSize = sizeof(track);
        track.dwFlags = TME_LEAVE;
        track.hwndTrack = owner->radioHwnd_;
        if (TrackMouseEvent(&track)) {
            trackingMouseLeave = true;
        }
    }

    void Draw(const DRAWITEMSTRUCT* draw) {
        RECT rect = draw->rcItem;
        FillRect(draw->hDC, &rect, brushSurface);

        const bool enabled = IsWindowEnabled(draw->hwndItem) != FALSE;
        const bool pressed = (draw->itemState & ODS_SELECTED) != 0;

        RECT circleRect{rect.left + 2, rect.top + ((rect.bottom - rect.top) - 18) / 2, rect.left + 20, rect.top + ((rect.bottom - rect.top) - 18) / 2 + 18};

        HGDIOBJ oldPen = SelectObject(draw->hDC, penBorder);
        HGDIOBJ oldBrush = SelectObject(draw->hDC, hot || pressed ? brushCircleHot : brushCircle);
        Ellipse(draw->hDC, circleRect.left, circleRect.top, circleRect.right, circleRect.bottom);
        SelectObject(draw->hDC, oldBrush);

        if (checked) {
            HGDIOBJ oldDotBrush = SelectObject(draw->hDC, brushDot);
            HGDIOBJ oldDotPen = SelectObject(draw->hDC, GetStockObject(NULL_PEN));
            Ellipse(draw->hDC, circleRect.left + 4, circleRect.top + 4, circleRect.right - 4, circleRect.bottom - 4);
            SelectObject(draw->hDC, oldDotPen);
            SelectObject(draw->hDC, oldDotBrush);
        }
        SelectObject(draw->hDC, oldPen);

        RECT textRect = rect;
        textRect.left = circleRect.right + 10;

        HFONT oldFont = SelectControlFont(draw->hDC, draw->hwndItem);
        SetBkMode(draw->hDC, TRANSPARENT);
        SetTextColor(draw->hDC, enabled ? owner->theme_.radioText : owner->theme_.radioDisabledText);

        wchar_t text[1024] = {};
        GetWindowTextW(draw->hwndItem, text, _countof(text));
        DrawTextW(draw->hDC, text, -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
        if (oldFont) {
            SelectObject(draw->hDC, oldFont);
        }
    }

    void SelectInGroup() {
        if (!owner->radioHwnd_ || !owner->parentHwnd_ || !IsWindowEnabled(owner->radioHwnd_)) {
            return;
        }

        HWND groupStart = owner->radioHwnd_;
        for (HWND previous = GetWindow(owner->radioHwnd_, GW_HWNDPREV); previous; previous = GetWindow(previous, GW_HWNDPREV)) {
            const LONG_PTR style = GetWindowLongPtrW(previous, GWL_STYLE);
            if ((style & WS_GROUP) != 0) {
                groupStart = previous;
                break;
            }
            groupStart = previous;
        }

        for (HWND current = groupStart; current; current = GetWindow(current, GW_HWNDNEXT)) {
            wchar_t buffer[16] = {};
            GetClassNameW(current, buffer, _countof(buffer));
            const LONG_PTR style = GetWindowLongPtrW(current, GWL_STYLE);
            if (current != groupStart && (style & WS_GROUP) != 0) {
                break;
            }
            if (_wcsicmp(buffer, L"Button") == 0) {
                const LONG_PTR buttonStyle = style & BS_TYPEMASK;
                if (buttonStyle == BS_RADIOBUTTON || buttonStyle == BS_AUTORADIOBUTTON || GetPropW(current, kRadioMarkerProperty) != nullptr) {
                    SendMessageW(current, BM_SETCHECK, current == owner->radioHwnd_ ? BST_CHECKED : BST_UNCHECKED, 0);
                    if (GetPropW(current, kRadioMarkerProperty) != nullptr) {
                        SetPropW(current, kRadioStateProperty, reinterpret_cast<HANDLE>(current == owner->radioHwnd_ ? 1 : 0));
                    }
                    InvalidateRect(current, nullptr, FALSE);
                }
            }
        }
    }

    void NotifyClicked() {
        if (!owner->parentHwnd_ || !owner->radioHwnd_) {
            return;
        }
        SendMessageW(owner->parentHwnd_,
                     WM_COMMAND,
                     MAKEWPARAM(static_cast<UINT>(owner->controlId_), BN_CLICKED),
                     reinterpret_cast<LPARAM>(owner->radioHwnd_));
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
            self->SelectInGroup();
            self->NotifyClicked();
            return 0;
        case WM_KEYUP:
            if (wParam == VK_SPACE) {
                self->SelectInGroup();
                self->NotifyClicked();
                return 0;
            }
            break;
        case BM_SETCHECK: {
            self->checked = (wParam == BST_CHECKED);
            SetPropW(window, kRadioStateProperty, reinterpret_cast<HANDLE>(self->checked ? 1 : 0));
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        case BM_GETCHECK:
            return self->checked ? BST_CHECKED : BST_UNCHECKED;
        case BM_CLICK:
            self->SelectInGroup();
            self->NotifyClicked();
            return 0;
        case WM_DESTROY:
            RemovePropW(window, kRadioStateProperty);
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
            if (draw && draw->hwndItem == self->owner->radioHwnd_) {
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

RadioButton::RadioButton() : impl_(std::make_unique<Impl>(this)) {}

RadioButton::~RadioButton() {
    Destroy();
}

bool RadioButton::Create(HWND parent, int controlId, const Theme& theme, const Options& options) {
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
    radioHwnd_ = CreateWindowExW(options.exStyle,
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
    if (!radioHwnd_) {
        Destroy();
        return false;
    }

    SendMessageW(radioHwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(impl_->font), TRUE);
    SetPropW(radioHwnd_, kRadioMarkerProperty, reinterpret_cast<HANDLE>(1));
    SetPropW(radioHwnd_, kRadioStateProperty, reinterpret_cast<HANDLE>(0));
    SetWindowSubclass(radioHwnd_,
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

void RadioButton::Destroy() {
    if (parentHwnd_) {
        RemoveWindowSubclass(parentHwnd_, Impl::ParentSubclassProc, reinterpret_cast<UINT_PTR>(this));
    }
    if (radioHwnd_) {
        RemovePropW(radioHwnd_, kRadioMarkerProperty);
        RemovePropW(radioHwnd_, kRadioStateProperty);
        RemoveWindowSubclass(radioHwnd_, Impl::ControlSubclassProc, reinterpret_cast<UINT_PTR>(this));
        DestroyWindow(radioHwnd_);
        radioHwnd_ = nullptr;
    }
    parentHwnd_ = nullptr;
    controlId_ = 0;
    impl_->hot = false;
    impl_->trackingMouseLeave = false;
}

void RadioButton::SetTheme(const Theme& theme) {
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

void RadioButton::SetText(const std::wstring& text) {
    if (radioHwnd_) {
        SetWindowTextW(radioHwnd_, text.c_str());
        InvalidateRect(radioHwnd_, nullptr, TRUE);
    }
}

std::wstring RadioButton::GetText() const {
    if (!radioHwnd_) {
        return {};
    }
    const int length = GetWindowTextLengthW(radioHwnd_);
    if (length <= 0) {
        return {};
    }
    std::wstring text(length + 1, L'\0');
    GetWindowTextW(radioHwnd_, text.data(), length + 1);
    text.resize(length);
    return text;
}

bool RadioButton::GetChecked() const {
    return impl_ && impl_->checked;
}

void RadioButton::SetChecked(bool checked) {
    if (impl_) {
        impl_->checked = checked;
    }
    if (radioHwnd_) {
        SetPropW(radioHwnd_, kRadioStateProperty, reinterpret_cast<HANDLE>(checked ? 1 : 0));
        InvalidateRect(radioHwnd_, nullptr, FALSE);
    }
}

void RadioButton::SetSurfaceColor(COLORREF color) {
    const COLORREF previousSurfaceColor = surfaceColor_;
    hasCustomSurfaceColor_ = true;
    surfaceColor_ = color;
    if (!impl_->UpdateThemeResources()) {
        surfaceColor_ = previousSurfaceColor;
        impl_->UpdateThemeResources();
    }
}

}  // namespace darkui
