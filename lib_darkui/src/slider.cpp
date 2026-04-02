#include "darkui/slider.h"

#include <algorithm>

namespace darkui {
namespace {

constexpr wchar_t kSliderClassName[] = L"DarkUiSliderControl";

ATOM EnsureSliderClassRegistered(HINSTANCE instance);
LRESULT CALLBACK SliderWindowProcThunk(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

COLORREF MixColor(COLORREF a, COLORREF b, double ratio) {
    ratio = std::clamp(ratio, 0.0, 1.0);
    const auto mix = [ratio](int x, int y) {
        return static_cast<int>(x + (y - x) * ratio + 0.5);
    };
    return RGB(mix(GetRValue(a), GetRValue(b)),
               mix(GetGValue(a), GetGValue(b)),
               mix(GetBValue(a), GetBValue(b)));
}

}  // namespace

struct Slider::Impl {
    Slider* owner = nullptr;
    HINSTANCE instance = nullptr;
    HBRUSH brushBackground = nullptr;
    HBRUSH brushTrack = nullptr;
    HBRUSH brushFill = nullptr;
    HBRUSH brushThumb = nullptr;
    HBRUSH brushThumbHot = nullptr;
    HBRUSH brushTick = nullptr;
    bool hot = false;
    bool trackingMouseLeave = false;
    bool dragging = false;

    explicit Impl(Slider* slider) : owner(slider) {}

    ~Impl() {
        if (brushBackground) DeleteObject(brushBackground);
        if (brushTrack) DeleteObject(brushTrack);
        if (brushFill) DeleteObject(brushFill);
        if (brushThumb) DeleteObject(brushThumb);
        if (brushThumbHot) DeleteObject(brushThumbHot);
        if (brushTick) DeleteObject(brushTick);
    }

    bool UpdateThemeResources() {
        owner->trackHeight_ = std::max(2, owner->theme_.sliderTrackHeight);
        owner->thumbRadius_ = std::max(4, owner->theme_.sliderThumbRadius);
        owner->backgroundColor_ = owner->theme_.sliderBackground;
        owner->trackColor_ = owner->theme_.sliderTrack;
        owner->fillColor_ = owner->theme_.sliderFill;
        owner->thumbColor_ = owner->theme_.sliderThumb;
        owner->thumbHotColor_ = owner->theme_.sliderThumbHot;
        owner->tickColor_ = owner->theme_.sliderTick;

        switch (owner->variant_) {
        case SliderVariant::Dense:
            owner->trackHeight_ = std::max(2, owner->trackHeight_ - 2);
            owner->thumbRadius_ = std::max(4, owner->thumbRadius_ - 1);
            break;
        case SliderVariant::Emphasis:
            owner->trackHeight_ = std::max(4, owner->trackHeight_ + 2);
            owner->thumbRadius_ = std::max(5, owner->thumbRadius_ + 1);
            owner->fillColor_ = MixColor(owner->theme_.sliderFill, owner->theme_.accent, 0.45);
            owner->thumbHotColor_ = MixColor(owner->theme_.sliderThumbHot, owner->theme_.highlightText, 0.35);
            break;
        case SliderVariant::Default:
        default:
            break;
        }

        HBRUSH newBrushBackground = CreateSolidBrush(owner->backgroundColor_);
        HBRUSH newBrushTrack = CreateSolidBrush(owner->trackColor_);
        HBRUSH newBrushFill = CreateSolidBrush(owner->fillColor_);
        HBRUSH newBrushThumb = CreateSolidBrush(owner->thumbColor_);
        HBRUSH newBrushThumbHot = CreateSolidBrush(owner->thumbHotColor_);
        HBRUSH newBrushTick = CreateSolidBrush(owner->tickColor_);

        if (!newBrushBackground || !newBrushTrack || !newBrushFill || !newBrushThumb || !newBrushThumbHot || !newBrushTick) {
            if (newBrushBackground) DeleteObject(newBrushBackground);
            if (newBrushTrack) DeleteObject(newBrushTrack);
            if (newBrushFill) DeleteObject(newBrushFill);
            if (newBrushThumb) DeleteObject(newBrushThumb);
            if (newBrushThumbHot) DeleteObject(newBrushThumbHot);
            if (newBrushTick) DeleteObject(newBrushTick);
            return false;
        }

        if (brushBackground) DeleteObject(brushBackground);
        if (brushTrack) DeleteObject(brushTrack);
        if (brushFill) DeleteObject(brushFill);
        if (brushThumb) DeleteObject(brushThumb);
        if (brushThumbHot) DeleteObject(brushThumbHot);
        if (brushTick) DeleteObject(brushTick);

        brushBackground = newBrushBackground;
        brushTrack = newBrushTrack;
        brushFill = newBrushFill;
        brushThumb = newBrushThumb;
        brushThumbHot = newBrushThumbHot;
        brushTick = newBrushTick;

        if (owner->sliderHwnd_) {
            InvalidateRect(owner->sliderHwnd_, nullptr, TRUE);
        }
        return true;
    }

    RECT GetTrackRect() const {
        RECT client{};
        GetClientRect(owner->sliderHwnd_, &client);
        const int trackHeight = std::max(2, owner->trackHeight_);
        const int horizontalPadding = HorizontalPadding();
        RECT track{
            client.left + horizontalPadding,
            client.top + (client.bottom - client.top - trackHeight) / 2,
            client.right - horizontalPadding,
            client.top + (client.bottom - client.top + trackHeight) / 2
        };
        if (track.right <= track.left) {
            track.right = track.left + 1;
        }
        return track;
    }

    int ThumbCenterX() const {
        RECT track = GetTrackRect();
        if (owner->maximum_ <= owner->minimum_) {
            return track.left;
        }
        const double ratio = static_cast<double>(owner->value_ - owner->minimum_) /
                             static_cast<double>(owner->maximum_ - owner->minimum_);
        return track.left + static_cast<int>((track.right - track.left) * ratio);
    }

    int ValueFromX(int x) const {
        RECT track = GetTrackRect();
        const int clamped = std::clamp(x, static_cast<int>(track.left), static_cast<int>(track.right));
        if (owner->maximum_ <= owner->minimum_) {
            return owner->minimum_;
        }
        const double ratio = static_cast<double>(clamped - track.left) /
                             std::max(1, static_cast<int>(track.right - track.left));
        const int value = owner->minimum_ + static_cast<int>((owner->maximum_ - owner->minimum_) * ratio + 0.5);
        return std::clamp(value, owner->minimum_, owner->maximum_);
    }

    void NotifyScroll(WORD code) {
        if (!owner->parentHwnd_) {
            return;
        }
        SendMessageW(owner->parentHwnd_,
                     WM_HSCROLL,
                     MAKEWPARAM(code, owner->value_),
                     reinterpret_cast<LPARAM>(owner->sliderHwnd_));
    }

    void StartMouseLeaveTracking() {
        if (!owner->sliderHwnd_ || trackingMouseLeave) {
            return;
        }
        TRACKMOUSEEVENT track{};
        track.cbSize = sizeof(track);
        track.dwFlags = TME_LEAVE;
        track.hwndTrack = owner->sliderHwnd_;
        if (TrackMouseEvent(&track)) {
            trackingMouseLeave = true;
        }
    }

    int ThumbCenterY() const {
        RECT client{};
        GetClientRect(owner->sliderHwnd_, &client);
        return (client.top + client.bottom) / 2;
    }

    int HorizontalPadding() const {
        const int thumbRadius = std::max(4, owner->thumbRadius_);
        return thumbRadius + 2;
    }

    void DrawTicks(HDC targetDc, const RECT& track) {
        const int tickCount = std::max(0, owner->tickCount_);
        if (!owner->showTicks_ || tickCount < 2) {
            return;
        }

        const int tickTop = track.bottom + 6;
        const int tickBottom = tickTop + 8;
        HBRUSH oldBrush = reinterpret_cast<HBRUSH>(SelectObject(targetDc, brushTick));
        HPEN oldPen = reinterpret_cast<HPEN>(SelectObject(targetDc, GetStockObject(NULL_PEN)));
        for (int i = 0; i < tickCount; ++i) {
            const double ratio = static_cast<double>(i) / static_cast<double>(tickCount - 1);
            const int x = track.left + static_cast<int>((track.right - track.left) * ratio + 0.5);
            RECT tick{x, tickTop, x + 1, tickBottom};
            FillRect(targetDc, &tick, brushTick);
        }
        SelectObject(targetDc, oldPen);
        SelectObject(targetDc, oldBrush);
    }

    void Paint(HDC targetDc) {
        RECT client{};
        GetClientRect(owner->sliderHwnd_, &client);
        FillRect(targetDc, &client, brushBackground);

        RECT track = GetTrackRect();
        FillRect(targetDc, &track, brushTrack);

        RECT fill = track;
        fill.right = ThumbCenterX();
        if (fill.right > fill.left) {
            FillRect(targetDc, &fill, brushFill);
        }

        const int radius = std::max(4, owner->thumbRadius_);
        const int centerY = ThumbCenterY();
        RECT thumb{
            ThumbCenterX() - radius,
            centerY - radius,
            ThumbCenterX() + radius + 1,
            centerY + radius + 1
        };

        HBRUSH fillBrush = (dragging || hot) ? brushThumbHot : brushThumb;
        HGDIOBJ oldBrush = SelectObject(targetDc, fillBrush);
        HGDIOBJ oldPen = SelectObject(targetDc, GetStockObject(NULL_PEN));
        Ellipse(targetDc, thumb.left, thumb.top, thumb.right, thumb.bottom);
        SelectObject(targetDc, oldPen);
        SelectObject(targetDc, oldBrush);
        DrawTicks(targetDc, track);
    }

    void UpdateValueFromMouse(int x, bool notify, WORD code) {
        owner->SetValue(ValueFromX(x), false);
        if (notify) {
            NotifyScroll(code);
        }
    }

    static LRESULT CALLBACK SliderWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
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
        case WM_MOUSEMOVE:
            if (!self->hot) {
                self->hot = true;
                InvalidateRect(window, nullptr, FALSE);
            }
            self->StartMouseLeaveTracking();
            if (self->dragging) {
                self->UpdateValueFromMouse(static_cast<int>(LOWORD(lParam)), true, SB_THUMBTRACK);
            }
            return 0;
        case WM_MOUSELEAVE:
            self->hot = false;
            self->trackingMouseLeave = false;
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        case WM_LBUTTONDOWN:
            SetCapture(window);
            self->dragging = true;
            self->UpdateValueFromMouse(static_cast<int>(LOWORD(lParam)), true, SB_THUMBTRACK);
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        case WM_LBUTTONUP:
            if (self->dragging) {
                self->UpdateValueFromMouse(static_cast<int>(LOWORD(lParam)), true, SB_THUMBPOSITION);
                self->NotifyScroll(SB_ENDSCROLL);
                self->dragging = false;
                if (GetCapture() == window) {
                    ReleaseCapture();
                }
                InvalidateRect(window, nullptr, FALSE);
            }
            return 0;
        case WM_CAPTURECHANGED:
            if (self->dragging) {
                self->dragging = false;
                InvalidateRect(window, nullptr, FALSE);
            }
            return 0;
        case WM_KEYDOWN:
            if (wParam == VK_LEFT || wParam == VK_DOWN) {
                self->owner->SetValue(self->owner->value_ - 1, false);
                self->NotifyScroll(SB_LINELEFT);
                return 0;
            }
            if (wParam == VK_RIGHT || wParam == VK_UP) {
                self->owner->SetValue(self->owner->value_ + 1, false);
                self->NotifyScroll(SB_LINERIGHT);
                return 0;
            }
            if (wParam == VK_HOME) {
                self->owner->SetValue(self->owner->minimum_, false);
                self->NotifyScroll(SB_LEFT);
                return 0;
            }
            if (wParam == VK_END) {
                self->owner->SetValue(self->owner->maximum_, false);
                self->NotifyScroll(SB_RIGHT);
                return 0;
            }
            break;
        default:
            break;
        }

        return DefWindowProcW(window, message, wParam, lParam);
    }
};

namespace {

ATOM EnsureSliderClassRegistered(HINSTANCE instance) {
    static ATOM atom = 0;
    if (atom != 0) {
        return atom;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = SliderWindowProcThunk;
    wc.hInstance = instance;
    wc.lpszClassName = kSliderClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_HAND);
    wc.hbrBackground = nullptr;
    atom = RegisterClassExW(&wc);
    return atom;
}

LRESULT CALLBACK SliderWindowProcThunk(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    return Slider::Impl::SliderWindowProc(window, message, wParam, lParam);
}

}  // namespace

Slider::Slider() : impl_(std::make_unique<Impl>(this)) {}

Slider::~Slider() {
    Destroy();
}

bool Slider::Create(HWND parent, int controlId, const Theme& theme, const Options& options) {
    Destroy();
    parentHwnd_ = parent;
    controlId_ = controlId;
    theme_ = ResolveTheme(theme);
    variant_ = options.variant;
    impl_->instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(parent, GWLP_HINSTANCE));
    if (!impl_->instance) {
        impl_->instance = GetModuleHandleW(nullptr);
    }

    EnsureSliderClassRegistered(impl_->instance);
    sliderHwnd_ = CreateWindowExW(options.exStyle,
                                  kSliderClassName,
                                  L"",
                                  options.style | WS_CLIPSIBLINGS,
                                  0,
                                  0,
                                  0,
                                  0,
                                  parent,
                                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
                                  impl_->instance,
                                  impl_.get());
    if (!sliderHwnd_) {
        Destroy();
        return false;
    }

    if (!impl_->UpdateThemeResources()) {
        Destroy();
        return false;
    }
    SetRange(options.minimum, options.maximum);
    SetValue(options.value);
    SetShowTicks(options.showTicks);
    SetTickCount(options.tickCount);
    return true;
}

void Slider::Destroy() {
    if (sliderHwnd_) {
        if (GetCapture() == sliderHwnd_) {
            ReleaseCapture();
        }
        DestroyWindow(sliderHwnd_);
        sliderHwnd_ = nullptr;
    }
    parentHwnd_ = nullptr;
    controlId_ = 0;
}

void Slider::SetTheme(const Theme& theme) {
    const Theme previous = theme_;
    theme_ = ResolveTheme(theme);
    if (!impl_->UpdateThemeResources()) {
        theme_ = previous;
        impl_->UpdateThemeResources();
    }
}

void Slider::SetRange(int minimum, int maximum) {
    if (minimum > maximum) {
        std::swap(minimum, maximum);
    }
    minimum_ = minimum;
    maximum_ = maximum;
    value_ = std::clamp(value_, minimum_, maximum_);
    if (sliderHwnd_) {
        InvalidateRect(sliderHwnd_, nullptr, TRUE);
    }
}

void Slider::SetValue(int value, bool notify) {
    value_ = std::clamp(value, minimum_, maximum_);
    if (sliderHwnd_) {
        InvalidateRect(sliderHwnd_, nullptr, FALSE);
    }
    if (notify && parentHwnd_ && sliderHwnd_) {
        SendMessageW(parentHwnd_,
                     WM_HSCROLL,
                     MAKEWPARAM(SB_THUMBPOSITION, value_),
                     reinterpret_cast<LPARAM>(sliderHwnd_));
    }
}

void Slider::SetShowTicks(bool enabled) {
    showTicks_ = enabled;
    if (sliderHwnd_) {
        InvalidateRect(sliderHwnd_, nullptr, FALSE);
    }
}

void Slider::SetTickCount(int count) {
    tickCount_ = std::max(0, count);
    if (sliderHwnd_) {
        InvalidateRect(sliderHwnd_, nullptr, FALSE);
    }
}

}  // namespace darkui
