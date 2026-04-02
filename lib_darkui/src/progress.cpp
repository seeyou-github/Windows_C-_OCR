#include "darkui/progress.h"

#include <algorithm>
#include <string>

namespace darkui {
namespace {

constexpr wchar_t kProgressClassName[] = L"DarkUiProgressBarControl";

ATOM EnsureProgressClassRegistered(HINSTANCE instance);
LRESULT CALLBACK ProgressWindowProcThunk(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

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

struct ProgressBar::Impl {
    ProgressBar* owner = nullptr;
    HINSTANCE instance = nullptr;
    HBRUSH brushBackground = nullptr;
    HBRUSH brushTrack = nullptr;
    HBRUSH brushFill = nullptr;
    HFONT font = nullptr;

    explicit Impl(ProgressBar* progress) : owner(progress) {}

    ~Impl() {
        if (brushBackground) DeleteObject(brushBackground);
        if (brushTrack) DeleteObject(brushTrack);
        if (brushFill) DeleteObject(brushFill);
        if (font) DeleteObject(font);
    }

    bool UpdateThemeResources() {
        owner->trackHeight_ = std::max(8, owner->theme_.progressHeight);
        owner->backgroundColor_ = owner->theme_.progressBackground;
        owner->trackColor_ = owner->theme_.progressTrack;
        owner->fillColor_ = owner->theme_.progressFill;
        owner->textColor_ = owner->theme_.progressText;

        switch (owner->variant_) {
        case ProgressVariant::Panel:
            owner->trackHeight_ = std::max(8, owner->theme_.progressHeight - 2);
            owner->backgroundColor_ = MixColor(owner->surfaceColor_, owner->theme_.progressBackground, 0.72);
            owner->trackColor_ = MixColor(owner->surfaceColor_, owner->theme_.progressTrack, 0.80);
            break;
        case ProgressVariant::Emphasis:
            owner->trackHeight_ = std::max(10, owner->theme_.progressHeight + 2);
            owner->fillColor_ = MixColor(owner->theme_.progressFill, owner->theme_.accent, 0.45);
            owner->textColor_ = owner->theme_.highlightText;
            break;
        case ProgressVariant::Default:
        default:
            break;
        }

        HBRUSH newBrushBackground = CreateSolidBrush(owner->backgroundColor_);
        HBRUSH newBrushTrack = CreateSolidBrush(owner->trackColor_);
        HBRUSH newBrushFill = CreateSolidBrush(owner->fillColor_);
        HFONT newFont = CreateFont(owner->theme_.uiFont);

        if (!newBrushBackground || !newBrushTrack || !newBrushFill || !newFont) {
            if (newBrushBackground) DeleteObject(newBrushBackground);
            if (newBrushTrack) DeleteObject(newBrushTrack);
            if (newBrushFill) DeleteObject(newBrushFill);
            if (newFont) DeleteObject(newFont);
            return false;
        }

        if (brushBackground) DeleteObject(brushBackground);
        if (brushTrack) DeleteObject(brushTrack);
        if (brushFill) DeleteObject(brushFill);
        if (font) DeleteObject(font);

        brushBackground = newBrushBackground;
        brushTrack = newBrushTrack;
        brushFill = newBrushFill;
        font = newFont;

        if (owner->progressHwnd_) {
            InvalidateRect(owner->progressHwnd_, nullptr, TRUE);
        }
        return true;
    }

    RECT GetTrackRect() const {
        RECT client{};
        GetClientRect(owner->progressHwnd_, &client);
        const int trackHeight = std::max(4, owner->trackHeight_);
        const int top = static_cast<int>(client.top) +
                        std::max(0, static_cast<int>(client.bottom - client.top - trackHeight) / 2);
        return RECT{client.left, top, client.right, top + trackHeight};
    }

    int FillRight(const RECT& track) const {
        if (owner->maximum_ <= owner->minimum_) {
            return track.left;
        }
        const double ratio = static_cast<double>(owner->value_ - owner->minimum_) /
                             static_cast<double>(owner->maximum_ - owner->minimum_);
        return track.left + static_cast<int>((track.right - track.left) * ratio + 0.5);
    }

    void Paint(HDC targetDc) {
        HBRUSH fallbackBackground = brushBackground ? nullptr : CreateSolidBrush(owner->theme_.progressBackground);
        HBRUSH fallbackTrack = brushTrack ? nullptr : CreateSolidBrush(owner->theme_.progressTrack);
        HBRUSH fallbackFill = brushFill ? nullptr : CreateSolidBrush(owner->theme_.progressFill);
        HBRUSH backgroundBrush = brushBackground ? brushBackground : fallbackBackground;
        HBRUSH trackBrush = brushTrack ? brushTrack : fallbackTrack;
        HBRUSH fillBrush = brushFill ? brushFill : fallbackFill;

        RECT client{};
        GetClientRect(owner->progressHwnd_, &client);
        HBRUSH surfaceBrush = CreateSolidBrush(owner->surfaceColor_);
        FillRect(targetDc, &client, surfaceBrush);
        DeleteObject(surfaceBrush);

        RECT track = GetTrackRect();
        FillRect(targetDc, &track, backgroundBrush);

        RECT innerTrack = track;
        InflateRect(&innerTrack, -1, -1);
        if (innerTrack.right > innerTrack.left && innerTrack.bottom > innerTrack.top) {
            FillRect(targetDc, &innerTrack, trackBrush);
        }

        RECT fill = innerTrack;
        fill.right = std::clamp(FillRight(innerTrack),
                                static_cast<int>(innerTrack.left),
                                static_cast<int>(innerTrack.right));
        if (fill.right > fill.left) {
            FillRect(targetDc, &fill, fillBrush);
        }

        if (!owner->showPercentage_) {
            if (fallbackBackground) DeleteObject(fallbackBackground);
            if (fallbackTrack) DeleteObject(fallbackTrack);
            if (fallbackFill) DeleteObject(fallbackFill);
            return;
        }

        const int range = std::max(1, owner->maximum_ - owner->minimum_);
        const int percent = ((owner->value_ - owner->minimum_) * 100) / range;
        const std::wstring text = std::to_wstring(std::clamp(percent, 0, 100)) + L"%";

        HFONT oldFont = font ? reinterpret_cast<HFONT>(SelectObject(targetDc, font)) : nullptr;
        SetBkMode(targetDc, TRANSPARENT);
        SetTextColor(targetDc, owner->textColor_);
        DrawTextW(targetDc, text.c_str(), -1, &client, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        if (oldFont) {
            SelectObject(targetDc, oldFont);
        }
        if (fallbackBackground) DeleteObject(fallbackBackground);
        if (fallbackTrack) DeleteObject(fallbackTrack);
        if (fallbackFill) DeleteObject(fallbackFill);
    }

    static LRESULT CALLBACK ProgressWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
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
        default:
            break;
        }

        return DefWindowProcW(window, message, wParam, lParam);
    }
};

namespace {

ATOM EnsureProgressClassRegistered(HINSTANCE instance) {
    static ATOM atom = 0;
    if (atom != 0) {
        return atom;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = ProgressWindowProcThunk;
    wc.hInstance = instance;
    wc.lpszClassName = kProgressClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    atom = RegisterClassExW(&wc);
    return atom;
}

LRESULT CALLBACK ProgressWindowProcThunk(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    return ProgressBar::Impl::ProgressWindowProc(window, message, wParam, lParam);
}

}  // namespace

ProgressBar::ProgressBar() : impl_(std::make_unique<Impl>(this)) {}

ProgressBar::~ProgressBar() {
    Destroy();
}

bool ProgressBar::Create(HWND parent, int controlId, const Theme& theme, const Options& options) {
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

    if (!EnsureProgressClassRegistered(impl_->instance)) {
        Destroy();
        return false;
    }
    progressHwnd_ = CreateWindowExW(options.exStyle,
                                    kProgressClassName,
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
    if (!progressHwnd_) {
        Destroy();
        return false;
    }

    if (!impl_->UpdateThemeResources() || !impl_->brushBackground || !impl_->brushTrack || !impl_->brushFill || !impl_->font) {
        Destroy();
        return false;
    }
    SetRange(options.minimum, options.maximum);
    SetValue(options.value);
    SetShowPercentage(options.showPercentage);
    return true;
}

void ProgressBar::Destroy() {
    if (progressHwnd_) {
        DestroyWindow(progressHwnd_);
        progressHwnd_ = nullptr;
    }
    parentHwnd_ = nullptr;
    controlId_ = 0;
}

void ProgressBar::SetTheme(const Theme& theme) {
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

void ProgressBar::SetRange(int minimum, int maximum) {
    if (minimum > maximum) {
        std::swap(minimum, maximum);
    }
    minimum_ = minimum;
    maximum_ = maximum;
    value_ = std::clamp(value_, minimum_, maximum_);
    if (progressHwnd_) {
        InvalidateRect(progressHwnd_, nullptr, TRUE);
    }
}

void ProgressBar::SetValue(int value) {
    value_ = std::clamp(value, minimum_, maximum_);
    if (progressHwnd_) {
        InvalidateRect(progressHwnd_, nullptr, FALSE);
    }
}

void ProgressBar::SetShowPercentage(bool enabled) {
    showPercentage_ = enabled;
    if (progressHwnd_) {
        InvalidateRect(progressHwnd_, nullptr, FALSE);
    }
}

void ProgressBar::SetSurfaceColor(COLORREF color) {
    const COLORREF previousSurfaceColor = surfaceColor_;
    hasCustomSurfaceColor_ = true;
    surfaceColor_ = color;
    if (progressHwnd_ && !impl_->UpdateThemeResources()) {
        surfaceColor_ = previousSurfaceColor;
        impl_->UpdateThemeResources();
    } else if (progressHwnd_) {
        InvalidateRect(progressHwnd_, nullptr, TRUE);
    }
}

}  // namespace darkui
