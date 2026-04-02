#include "darkui/button.h"

#include <commctrl.h>

#include <algorithm>

namespace darkui {
namespace {

COLORREF ClampColor(int r, int g, int b) {
    return RGB(std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255));
}

COLORREF AdjustColor(COLORREF color, int delta) {
    return ClampColor(static_cast<int>(GetRValue(color)) + delta,
                      static_cast<int>(GetGValue(color)) + delta,
                      static_cast<int>(GetBValue(color)) + delta);
}

COLORREF MixColor(COLORREF a, COLORREF b, double ratio) {
    ratio = std::clamp(ratio, 0.0, 1.0);
    const auto mix = [ratio](int x, int y) {
        return static_cast<int>(x + (y - x) * ratio + 0.5);
    };
    return RGB(mix(GetRValue(a), GetRValue(b)),
               mix(GetGValue(a), GetGValue(b)),
               mix(GetBValue(a), GetBValue(b)));
}

COLORREF MakeDangerBase(const Theme& theme) {
    const COLORREF fallback = RGB(170, 62, 62);
    return MixColor(fallback, theme.accent, 0.18);
}

int ResolveButtonCornerRadius(ButtonVariant variant) {
    switch (variant) {
    case ButtonVariant::Ghost:
        return 12;
    case ButtonVariant::Subtle:
        return 12;
    case ButtonVariant::Danger:
        return 14;
    case ButtonVariant::Secondary:
        return 14;
    case ButtonVariant::Primary:
    default:
        return 14;
    }
}

struct ResolvedButtonVisuals {
    COLORREF fill = RGB(65, 72, 82);
    COLORREF fillHover = RGB(72, 80, 92);
    COLORREF fillHot = RGB(78, 86, 98);
    COLORREF fillDisabled = RGB(50, 54, 60);
    COLORREF border = RGB(61, 66, 74);
    COLORREF text = RGB(245, 247, 250);
    COLORREF disabledText = RGB(130, 136, 144);
};

ResolvedButtonVisuals ResolveButtonVisuals(const Theme& theme, COLORREF surface, ButtonVariant variant) {
    ResolvedButtonVisuals visuals{};
    switch (variant) {
    case ButtonVariant::Secondary:
        visuals.fill = theme.button;
        visuals.fillHover = theme.buttonHover;
        visuals.fillHot = theme.buttonHot;
        visuals.fillDisabled = theme.buttonDisabled;
        visuals.border = theme.border;
        visuals.text = theme.text;
        visuals.disabledText = theme.buttonDisabledText;
        break;
    case ButtonVariant::Subtle:
        visuals.fill = MixColor(surface, theme.text, 0.08);
        visuals.fillHover = MixColor(surface, theme.text, 0.13);
        visuals.fillHot = MixColor(surface, theme.text, 0.18);
        visuals.fillDisabled = MixColor(surface, theme.background, 0.14);
        visuals.border = MixColor(surface, theme.text, 0.14);
        visuals.text = theme.text;
        visuals.disabledText = theme.buttonDisabledText;
        break;
    case ButtonVariant::Ghost:
        visuals.fill = surface;
        visuals.fillHover = MixColor(surface, theme.text, 0.08);
        visuals.fillHot = MixColor(surface, theme.text, 0.13);
        visuals.fillDisabled = surface;
        visuals.border = MixColor(surface, theme.text, 0.16);
        visuals.text = theme.text;
        visuals.disabledText = theme.buttonDisabledText;
        break;
    case ButtonVariant::Danger: {
        const COLORREF base = MakeDangerBase(theme);
        visuals.fill = base;
        visuals.fillHover = AdjustColor(base, 10);
        visuals.fillHot = AdjustColor(base, 18);
        visuals.fillDisabled = MixColor(base, surface, 0.48);
        visuals.border = AdjustColor(base, -18);
        visuals.text = theme.highlightText;
        visuals.disabledText = MixColor(theme.highlightText, surface, 0.55);
        break;
    }
    case ButtonVariant::Primary:
    default:
        visuals.fill = theme.accentSecondary;
        visuals.fillHover = theme.accent;
        visuals.fillHot = AdjustColor(theme.accent, 10);
        visuals.fillDisabled = MixColor(theme.accentSecondary, surface, 0.55);
        visuals.border = AdjustColor(theme.accentSecondary, -14);
        visuals.text = theme.highlightText;
        visuals.disabledText = MixColor(theme.highlightText, surface, 0.52);
        break;
    }
    return visuals;
}

}  // namespace

struct Button::Impl {
    static constexpr UINT_PTR kAnimationTimerId = 1;

    Button* owner = nullptr;
    HINSTANCE instance = nullptr;
    HBRUSH brushButton = nullptr;
    HBRUSH brushButtonHover = nullptr;
    HBRUSH brushButtonHot = nullptr;
    HBRUSH brushButtonDisabled = nullptr;
    HBRUSH brushBorder = nullptr;
    HPEN penBorder = nullptr;
    HFONT font = nullptr;
    bool hot = false;
    bool trackingMouseLeave = false;
    int visualOffsetY = 0;
    int targetOffsetY = 0;

    explicit Impl(Button* button) : owner(button) {}

    ~Impl() {
        if (font) DeleteObject(font);
        if (brushButton) DeleteObject(brushButton);
        if (brushButtonHover) DeleteObject(brushButtonHover);
        if (brushButtonHot) DeleteObject(brushButtonHot);
        if (brushButtonDisabled) DeleteObject(brushButtonDisabled);
        if (brushBorder) DeleteObject(brushBorder);
        if (penBorder) DeleteObject(penBorder);
    }

    bool UpdateThemeResources() {
        const ResolvedButtonVisuals visuals = ResolveButtonVisuals(owner->theme_, owner->surfaceColor_, owner->variant_);
        owner->fillColor_ = visuals.fill;
        owner->fillHoverColor_ = visuals.fillHover;
        owner->fillHotColor_ = visuals.fillHot;
        owner->fillDisabledColor_ = visuals.fillDisabled;
        owner->borderColor_ = visuals.border;
        owner->textColor_ = visuals.text;
        owner->disabledTextColor_ = visuals.disabledText;

        HFONT newFont = CreateFont(owner->theme_.uiFont);
        HBRUSH newBrushButton = CreateSolidBrush(owner->fillColor_);
        HBRUSH newBrushButtonHover = CreateSolidBrush(owner->fillHoverColor_);
        HBRUSH newBrushButtonHot = CreateSolidBrush(owner->fillHotColor_);
        HBRUSH newBrushButtonDisabled = CreateSolidBrush(owner->fillDisabledColor_);
        HBRUSH newBrushBorder = CreateSolidBrush(owner->borderColor_);
        HPEN newPenBorder = CreatePen(PS_SOLID, 1, owner->borderColor_);

        if (!newFont || !newBrushButton || !newBrushButtonHover || !newBrushButtonHot ||
            !newBrushButtonDisabled || !newBrushBorder || !newPenBorder) {
            if (newFont) DeleteObject(newFont);
            if (newBrushButton) DeleteObject(newBrushButton);
            if (newBrushButtonHover) DeleteObject(newBrushButtonHover);
            if (newBrushButtonHot) DeleteObject(newBrushButtonHot);
            if (newBrushButtonDisabled) DeleteObject(newBrushButtonDisabled);
            if (newBrushBorder) DeleteObject(newBrushBorder);
            if (newPenBorder) DeleteObject(newPenBorder);
            return false;
        }

        if (font) DeleteObject(font);
        if (brushButton) DeleteObject(brushButton);
        if (brushButtonHover) DeleteObject(brushButtonHover);
        if (brushButtonHot) DeleteObject(brushButtonHot);
        if (brushButtonDisabled) DeleteObject(brushButtonDisabled);
        if (brushBorder) DeleteObject(brushBorder);
        if (penBorder) DeleteObject(penBorder);

        font = newFont;
        brushButton = newBrushButton;
        brushButtonHover = newBrushButtonHover;
        brushButtonHot = newBrushButtonHot;
        brushButtonDisabled = newBrushButtonDisabled;
        brushBorder = newBrushBorder;
        penBorder = newPenBorder;

        if (owner->buttonHwnd_) {
            SendMessageW(owner->buttonHwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            InvalidateRect(owner->buttonHwnd_, nullptr, TRUE);
        }
        return true;
    }

    HFONT SelectControlFont(HDC dc, HWND control) {
        HFONT current = reinterpret_cast<HFONT>(SendMessageW(control, WM_GETFONT, 0, 0));
        return current ? reinterpret_cast<HFONT>(SelectObject(dc, current)) : nullptr;
    }

    void StepAnimation() {
        if (!owner->buttonHwnd_) {
            return;
        }
        if (visualOffsetY == targetOffsetY) {
            KillTimer(owner->buttonHwnd_, kAnimationTimerId);
            return;
        }

        if (visualOffsetY < targetOffsetY) {
            ++visualOffsetY;
        } else {
            --visualOffsetY;
        }
        InvalidateRect(owner->buttonHwnd_, nullptr, FALSE);
        if (visualOffsetY == targetOffsetY) {
            KillTimer(owner->buttonHwnd_, kAnimationTimerId);
        }
    }

    void UpdateWindowRegion() {
        if (!owner->buttonHwnd_) {
            return;
        }

        RECT rect{};
        GetClientRect(owner->buttonHwnd_, &rect);
        const int width = rect.right - rect.left;
        const int height = rect.bottom - rect.top;
        if (width <= 0 || height <= 0) {
            return;
        }

        if (owner->cornerRadius_ <= 0) {
            SetWindowRgn(owner->buttonHwnd_, nullptr, TRUE);
            return;
        }

        HRGN region = CreateRoundRectRgn(0, 0, width + 1, height + 1, owner->cornerRadius_ * 2, owner->cornerRadius_ * 2);
        SetWindowRgn(owner->buttonHwnd_, region, TRUE);
    }

    void AnimateTo(int offsetY) {
        targetOffsetY = offsetY;
        if (!owner->buttonHwnd_) {
            visualOffsetY = targetOffsetY;
            return;
        }
        SetTimer(owner->buttonHwnd_, kAnimationTimerId, 15, nullptr);
        StepAnimation();
    }

    void DrawButton(const DRAWITEMSTRUCT* draw) {
        const bool selected = (draw->itemState & ODS_SELECTED) != 0;
        const bool enabled = IsWindowEnabled(draw->hwndItem) != FALSE;

        HBRUSH fillBrush = brushButton;
        COLORREF textColor = owner->textColor_;
        if (!enabled) {
            fillBrush = brushButtonDisabled;
            textColor = owner->disabledTextColor_;
        } else if (selected) {
            fillBrush = brushButtonHot;
        } else if (hot) {
            fillBrush = brushButtonHover;
        }

        RECT buttonRect = draw->rcItem;

        HBRUSH parentBrush = CreateSolidBrush(owner->surfaceColor_);
        FillRect(draw->hDC, &buttonRect, parentBrush);
        DeleteObject(parentBrush);

        OffsetRect(&buttonRect, 0, visualOffsetY);

        HPEN oldPen = reinterpret_cast<HPEN>(SelectObject(draw->hDC, penBorder));
        HBRUSH oldBrush = reinterpret_cast<HBRUSH>(SelectObject(draw->hDC, fillBrush));
        RoundRect(draw->hDC,
                  buttonRect.left,
                  buttonRect.top,
                  buttonRect.right,
                  buttonRect.bottom,
                  owner->cornerRadius_ * 2,
                  owner->cornerRadius_ * 2);
        SelectObject(draw->hDC, oldBrush);
        SelectObject(draw->hDC, oldPen);

        RECT textRect = buttonRect;
        textRect.left += owner->theme_.textPadding;
        textRect.right -= owner->theme_.textPadding;

        HFONT oldFont = SelectControlFont(draw->hDC, draw->hwndItem);
        SetBkMode(draw->hDC, TRANSPARENT);
        SetTextColor(draw->hDC, textColor);

        wchar_t text[1024] = {};
        GetWindowTextW(draw->hwndItem, text, 1024);
        DrawTextW(draw->hDC, text, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);

        if (oldFont) {
            SelectObject(draw->hDC, oldFont);
        }
    }

    void StartMouseLeaveTracking() {
        if (!owner->buttonHwnd_ || trackingMouseLeave) {
            return;
        }

        TRACKMOUSEEVENT track{};
        track.cbSize = sizeof(track);
        track.dwFlags = TME_LEAVE;
        track.hwndTrack = owner->buttonHwnd_;
        if (TrackMouseEvent(&track)) {
            trackingMouseLeave = true;
        }
    }

    static LRESULT CALLBACK ButtonSubclassProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassId, DWORD_PTR refData) {
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
        case WM_LBUTTONDOWN:
            self->AnimateTo(2);
            break;
        case WM_LBUTTONUP:
        case WM_CAPTURECHANGED:
            self->AnimateTo(0);
            if (!IsWindowEnabled(window)) {
                self->hot = false;
                self->trackingMouseLeave = false;
            }
            break;
        case WM_TIMER:
            if (wParam == kAnimationTimerId) {
                self->StepAnimation();
                return 0;
            }
            break;
        case WM_ENABLE:
            self->hot = false;
            self->trackingMouseLeave = false;
            self->AnimateTo(0);
            InvalidateRect(window, nullptr, FALSE);
            break;
        case WM_SIZE:
            self->UpdateWindowRegion();
            break;
        case WM_DESTROY:
            KillTimer(window, kAnimationTimerId);
            RemoveWindowSubclass(window, ButtonSubclassProc, subclassId);
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
            if (draw && draw->hwndItem == self->owner->buttonHwnd_) {
                self->DrawButton(draw);
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

Button::Button() : impl_(std::make_unique<Impl>(this)) {}

Button::~Button() {
    Destroy();
}

bool Button::Create(HWND parent, int controlId, const Theme& theme, const Options& options) {
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

    DWORD style = options.style | BS_OWNERDRAW;
    buttonHwnd_ = CreateWindowExW(options.exStyle,
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
    if (!buttonHwnd_) {
        Destroy();
        return false;
    }

    SendMessageW(buttonHwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(impl_->font), TRUE);
    impl_->UpdateWindowRegion();
    SetCornerRadius(options.cornerRadius >= 0 ? options.cornerRadius : ResolveButtonCornerRadius(variant_));
    SetWindowSubclass(buttonHwnd_,
                      Impl::ButtonSubclassProc,
                      reinterpret_cast<UINT_PTR>(this),
                      reinterpret_cast<DWORD_PTR>(impl_.get()));
    SetWindowSubclass(parentHwnd_,
                      Impl::ParentSubclassProc,
                      reinterpret_cast<UINT_PTR>(this),
                      reinterpret_cast<DWORD_PTR>(impl_.get()));
    return true;
}

void Button::Destroy() {
    if (parentHwnd_) {
        RemoveWindowSubclass(parentHwnd_, Impl::ParentSubclassProc, reinterpret_cast<UINT_PTR>(this));
    }
    if (buttonHwnd_) {
        RemoveWindowSubclass(buttonHwnd_, Impl::ButtonSubclassProc, reinterpret_cast<UINT_PTR>(this));
        DestroyWindow(buttonHwnd_);
        buttonHwnd_ = nullptr;
    }
    parentHwnd_ = nullptr;
    controlId_ = 0;
    impl_->hot = false;
    impl_->trackingMouseLeave = false;
    impl_->visualOffsetY = 0;
    impl_->targetOffsetY = 0;
}

void Button::SetTheme(const Theme& theme) {
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

void Button::SetText(const std::wstring& text) {
    if (buttonHwnd_) {
        SetWindowTextW(buttonHwnd_, text.c_str());
        InvalidateRect(buttonHwnd_, nullptr, TRUE);
    }
}

std::wstring Button::GetText() const {
    if (!buttonHwnd_) {
        return L"";
    }
    const int length = GetWindowTextLengthW(buttonHwnd_);
    std::wstring text(length + 1, L'\0');
    GetWindowTextW(buttonHwnd_, text.data(), length + 1);
    text.resize(length);
    return text;
}

void Button::SetCornerRadius(int radius) {
    cornerRadius_ = radius < 0 ? 0 : radius;
    if (buttonHwnd_) {
        impl_->UpdateWindowRegion();
        InvalidateRect(buttonHwnd_, nullptr, TRUE);
    }
}

void Button::SetSurfaceColor(COLORREF color) {
    const COLORREF previousSurfaceColor = surfaceColor_;
    hasCustomSurfaceColor_ = true;
    surfaceColor_ = color;
    if (!impl_->UpdateThemeResources()) {
        surfaceColor_ = previousSurfaceColor;
        impl_->UpdateThemeResources();
    }
}

}  // namespace darkui
