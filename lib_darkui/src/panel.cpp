#include "darkui/panel.h"

namespace darkui {
namespace {

constexpr wchar_t kPanelClassName[] = L"DarkUiPanelControl";

ATOM EnsurePanelClassRegistered(HINSTANCE instance);
LRESULT CALLBACK PanelWindowProcThunk(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

}  // namespace

struct Panel::Impl {
    Panel* owner = nullptr;
    HINSTANCE instance = nullptr;
    HBRUSH brushBackground = nullptr;
    HPEN penBorder = nullptr;

    explicit Impl(Panel* panel) : owner(panel) {}

    ~Impl() {
        if (brushBackground) DeleteObject(brushBackground);
        if (penBorder) DeleteObject(penBorder);
    }

    bool UpdateThemeResources() {
        HBRUSH newBrushBackground = CreateSolidBrush(owner->backgroundColor_);
        HPEN newPenBorder = CreatePen(PS_SOLID, 1, owner->borderColor_);
        if (!newBrushBackground || !newPenBorder) {
            if (newBrushBackground) DeleteObject(newBrushBackground);
            if (newPenBorder) DeleteObject(newPenBorder);
            return false;
        }

        if (brushBackground) DeleteObject(brushBackground);
        if (penBorder) DeleteObject(penBorder);

        brushBackground = newBrushBackground;
        penBorder = newPenBorder;
        if (owner->panelHwnd_) {
            InvalidateRect(owner->panelHwnd_, nullptr, TRUE);
        }
        return true;
    }

    void UpdateWindowRegion() {
        if (!owner->panelHwnd_) {
            return;
        }
        RECT rect{};
        GetClientRect(owner->panelHwnd_, &rect);
        const int width = rect.right - rect.left;
        const int height = rect.bottom - rect.top;
        if (width <= 0 || height <= 0) {
            return;
        }
        if (owner->cornerRadius_ <= 0) {
            SetWindowRgn(owner->panelHwnd_, nullptr, TRUE);
            return;
        }
        HRGN region = CreateRoundRectRgn(0, 0, width + 1, height + 1, owner->cornerRadius_ * 2, owner->cornerRadius_ * 2);
        SetWindowRgn(owner->panelHwnd_, region, TRUE);
    }

    void Paint(HDC dc) {
        RECT client{};
        GetClientRect(owner->panelHwnd_, &client);
        HGDIOBJ oldBrush = SelectObject(dc, brushBackground);
        HGDIOBJ oldPen = SelectObject(dc, penBorder);
        RoundRect(dc, client.left, client.top, client.right, client.bottom, owner->cornerRadius_ * 2, owner->cornerRadius_ * 2);
        SelectObject(dc, oldPen);
        SelectObject(dc, oldBrush);
    }

    static LRESULT CALLBACK PanelWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
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
        case WM_SIZE:
            self->UpdateWindowRegion();
            return 0;
        case WM_COMMAND:
        case WM_NOTIFY:
        case WM_HSCROLL:
        case WM_VSCROLL:
        case WM_CTLCOLORBTN:
        case WM_CTLCOLORDLG:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX:
        case WM_CTLCOLORSTATIC:
            if (self->owner->parentHwnd_) {
                return SendMessageW(self->owner->parentHwnd_, message, wParam, lParam);
            }
            break;
        case WM_DESTROY:
            RemovePropW(window, kSurfaceRoleProperty);
            break;
        default:
            break;
        }

        return DefWindowProcW(window, message, wParam, lParam);
    }
};

namespace {

ATOM EnsurePanelClassRegistered(HINSTANCE instance) {
    static ATOM atom = 0;
    if (atom != 0) {
        return atom;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = PanelWindowProcThunk;
    wc.hInstance = instance;
    wc.lpszClassName = kPanelClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    atom = RegisterClassExW(&wc);
    return atom;
}

LRESULT CALLBACK PanelWindowProcThunk(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    return Panel::Impl::PanelWindowProc(window, message, wParam, lParam);
}

}  // namespace

Panel::Panel() : impl_(std::make_unique<Impl>(this)) {}

Panel::~Panel() {
    Destroy();
}

bool Panel::Create(HWND parent, int controlId, const Theme& theme, const Options& options) {
    Destroy();
    parentHwnd_ = parent;
    controlId_ = controlId;
    theme_ = ResolveTheme(theme);
    role_ = options.role == SurfaceRole::Auto ? SurfaceRole::Panel : options.role;
    cornerRadius_ = options.cornerRadius;
    hasCustomBackgroundColor_ = options.backgroundColor != CLR_INVALID;
    hasCustomBorderColor_ = options.borderColor != CLR_INVALID;
    backgroundColor_ = hasCustomBackgroundColor_ ? options.backgroundColor : ResolveSurfaceColor(theme_, role_);
    borderColor_ = hasCustomBorderColor_ ? options.borderColor : theme_.border;
    impl_->instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(parent, GWLP_HINSTANCE));
    if (!impl_->instance) {
        impl_->instance = GetModuleHandleW(nullptr);
    }

    if (!EnsurePanelClassRegistered(impl_->instance)) {
        Destroy();
        return false;
    }

    panelHwnd_ = CreateWindowExW(options.exStyle,
                                 kPanelClassName,
                                 L"",
                                 options.style,
                                 0,
                                 0,
                                 0,
                                 0,
                                 parent,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
                                 impl_->instance,
                                 impl_.get());
    if (!panelHwnd_) {
        Destroy();
        return false;
    }

    SetWindowSurfaceRole(panelHwnd_, role_);
    if (!impl_->UpdateThemeResources()) {
        Destroy();
        return false;
    }
    impl_->UpdateWindowRegion();
    if (!impl_->brushBackground || !impl_->penBorder) {
        Destroy();
        return false;
    }
    return true;
}

void Panel::Destroy() {
    if (panelHwnd_) {
        DestroyWindow(panelHwnd_);
        panelHwnd_ = nullptr;
    }
    parentHwnd_ = nullptr;
    controlId_ = 0;
}

void Panel::SetTheme(const Theme& theme) {
    const Theme previousTheme = theme_;
    const COLORREF previousBackgroundColor = backgroundColor_;
    const COLORREF previousBorderColor = borderColor_;
    theme_ = ResolveTheme(theme);
    if (!hasCustomBackgroundColor_) {
        backgroundColor_ = ResolveSurfaceColor(theme_, role_);
    }
    if (!hasCustomBorderColor_) {
        borderColor_ = theme_.border;
    }
    if (!impl_->UpdateThemeResources()) {
        theme_ = previousTheme;
        backgroundColor_ = previousBackgroundColor;
        borderColor_ = previousBorderColor;
        impl_->UpdateThemeResources();
    }
}

void Panel::SetCornerRadius(int radius) {
    cornerRadius_ = radius;
    impl_->UpdateWindowRegion();
    if (panelHwnd_) {
        InvalidateRect(panelHwnd_, nullptr, TRUE);
    }
}

}  // namespace darkui
