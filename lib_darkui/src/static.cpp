#include "darkui/static.h"

#include <commctrl.h>

#include <algorithm>

namespace darkui {
namespace {

int AdjustFontHeight(int height, int delta) {
    if (height == 0) {
        return delta;
    }
    if (height < 0) {
        return std::min(-1, height - delta);
    }
    return std::max(1, height + delta);
}

COLORREF ResolveStaticTextColor(const Theme& theme, StaticVariant variant) {
    switch (variant) {
    case StaticVariant::Title:
    case StaticVariant::PanelTitle:
        return theme.highlightText;
    case StaticVariant::Muted:
        return theme.mutedText;
    case StaticVariant::Body:
    case StaticVariant::PanelBody:
    case StaticVariant::Default:
    default:
        return theme.staticText;
    }
}

FontSpec ResolveStaticFont(const Theme& theme, StaticVariant variant) {
    FontSpec spec = theme.uiFont;
    switch (variant) {
    case StaticVariant::Title:
    case StaticVariant::PanelTitle:
        spec.weight = FW_SEMIBOLD;
        spec.height = AdjustFontHeight(spec.height, 4);
        break;
    case StaticVariant::Muted:
        spec.height = AdjustFontHeight(spec.height, -1);
        break;
    case StaticVariant::PanelBody:
        spec.height = AdjustFontHeight(spec.height, -1);
        break;
    case StaticVariant::Body:
    case StaticVariant::Default:
    default:
        break;
    }
    return spec;
}

SurfaceRole ResolveStaticSurfaceRole(StaticVariant variant, SurfaceRole requested) {
    if (requested != SurfaceRole::Auto) {
        return requested;
    }
    switch (variant) {
    case StaticVariant::PanelTitle:
    case StaticVariant::PanelBody:
        return SurfaceRole::Panel;
    case StaticVariant::Title:
    case StaticVariant::Body:
    case StaticVariant::Muted:
    case StaticVariant::Default:
    default:
        return SurfaceRole::Auto;
    }
}

UINT ResolveStaticTextFormat(StaticVariant variant, UINT requested) {
    if (requested != DT_LEFT) {
        return requested;
    }
    switch (variant) {
    case StaticVariant::Title:
    case StaticVariant::PanelTitle:
        return DT_LEFT | DT_SINGLELINE;
    case StaticVariant::Body:
    case StaticVariant::PanelBody:
    case StaticVariant::Muted:
    case StaticVariant::Default:
    default:
        return DT_LEFT;
    }
}

}  // namespace

struct Static::Impl {
    Static* owner = nullptr;
    HINSTANCE instance = nullptr;
    HBRUSH brushBackground = nullptr;
    HFONT font = nullptr;
    HICON icon = nullptr;
    HBITMAP bitmap = nullptr;

    explicit Impl(Static* control) : owner(control) {}

    ~Impl() {
        if (brushBackground) DeleteObject(brushBackground);
        if (font) DeleteObject(font);
    }

    bool UpdateThemeResources() {
        HBRUSH newBrushBackground = CreateSolidBrush(owner->backgroundColor_);
        HFONT newFont = CreateFont(ResolveStaticFont(owner->theme_, owner->variant_));
        if (!newBrushBackground || !newFont) {
            if (newBrushBackground) DeleteObject(newBrushBackground);
            if (newFont) DeleteObject(newFont);
            return false;
        }

        if (brushBackground) DeleteObject(brushBackground);
        if (font) DeleteObject(font);

        brushBackground = newBrushBackground;
        font = newFont;
        if (owner->staticHwnd_ && font) {
            SendMessageW(owner->staticHwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        }
        if (owner->staticHwnd_) {
            InvalidateRect(owner->staticHwnd_, nullptr, TRUE);
        }
        return true;
    }

    void DrawContent(HDC dc, const RECT& rect) {
        FillRect(dc, &rect, brushBackground ? brushBackground : reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));

        if (owner->contentMode_ == ContentMode::Icon && icon) {
            ICONINFO iconInfo{};
            BITMAP bitmapInfo{};
            int width = GetSystemMetrics(SM_CXSMICON);
            int height = GetSystemMetrics(SM_CYSMICON);
            if (GetIconInfo(icon, &iconInfo)) {
                HBITMAP colorBitmap = iconInfo.hbmColor ? iconInfo.hbmColor : iconInfo.hbmMask;
                if (colorBitmap && GetObjectW(colorBitmap, sizeof(bitmapInfo), &bitmapInfo)) {
                    width = bitmapInfo.bmWidth;
                    height = iconInfo.hbmColor ? bitmapInfo.bmHeight : bitmapInfo.bmHeight / 2;
                }
                if (iconInfo.hbmColor) DeleteObject(iconInfo.hbmColor);
                if (iconInfo.hbmMask) DeleteObject(iconInfo.hbmMask);
            }

            const int x = rect.left + ((rect.right - rect.left) - width) / 2;
            const int y = rect.top + ((rect.bottom - rect.top) - height) / 2;
            DrawIconEx(dc, x, y, icon, width, height, 0, nullptr, DI_NORMAL);
            return;
        }

        if (owner->contentMode_ == ContentMode::Bitmap && bitmap) {
            BITMAP bm{};
            if (GetObjectW(bitmap, sizeof(bm), &bm) == sizeof(bm) && bm.bmWidth > 0 && bm.bmHeight > 0) {
                HDC memoryDc = CreateCompatibleDC(dc);
                if (memoryDc) {
                    HGDIOBJ oldBitmap = SelectObject(memoryDc, bitmap);
                    const int x = rect.left + ((rect.right - rect.left) - bm.bmWidth) / 2;
                    const int y = rect.top + ((rect.bottom - rect.top) - bm.bmHeight) / 2;
                    BitBlt(dc, x, y, bm.bmWidth, bm.bmHeight, memoryDc, 0, 0, SRCCOPY);
                    SelectObject(memoryDc, oldBitmap);
                    DeleteDC(memoryDc);
                    return;
                }
            }
        }

        wchar_t text[2048] = {};
        GetWindowTextW(owner->staticHwnd_, text, _countof(text));

        RECT textRect = rect;
        textRect.left += owner->theme_.textPadding;
        textRect.right -= owner->theme_.textPadding;

        HFONT oldFont = font ? reinterpret_cast<HFONT>(SelectObject(dc, font)) : nullptr;
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, owner->textColor_);
        UINT format = owner->textFormat_ | DT_VCENTER | DT_NOPREFIX;
        if ((format & DT_SINGLELINE) == 0) {
            format |= DT_WORDBREAK;
        }
        if (owner->ellipsis_) {
            format |= DT_END_ELLIPSIS;
        }
        DrawTextW(dc, text, -1, &textRect, format);
        if (oldFont) {
            SelectObject(dc, oldFont);
        }
    }

    static LRESULT CALLBACK StaticSubclassProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassId, DWORD_PTR refData) {
        auto* self = reinterpret_cast<Impl*>(refData);
        if (!self) {
            return DefSubclassProc(window, message, wParam, lParam);
        }

        switch (message) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC dc = BeginPaint(window, &ps);
            RECT rect{};
            GetClientRect(window, &rect);
            self->DrawContent(dc, rect);
            EndPaint(window, &ps);
            return 0;
        }
        case WM_SETTEXT: {
            LRESULT result = DefSubclassProc(window, message, wParam, lParam);
            InvalidateRect(window, nullptr, TRUE);
            return result;
        }
        case WM_DESTROY:
            RemoveWindowSubclass(window, StaticSubclassProc, subclassId);
            break;
        default:
            break;
        }

        return DefSubclassProc(window, message, wParam, lParam);
    }
};

Static::Static() : impl_(std::make_unique<Impl>(this)) {}

Static::~Static() {
    Destroy();
}

bool Static::Create(HWND parent, int controlId, const Theme& theme, const Options& options) {
    Destroy();
    parentHwnd_ = parent;
    controlId_ = controlId;
    theme_ = ResolveTheme(theme);
    variant_ = options.variant;
    surfaceRole_ = ResolveStaticSurfaceRole(variant_, options.surfaceRole);
    hasCustomBackgroundColor_ = options.backgroundColor != CLR_INVALID;
    backgroundColor_ = hasCustomBackgroundColor_ ? options.backgroundColor : ResolveInheritedSurfaceColor(theme_, parent, surfaceRole_);
    textColor_ = ResolveStaticTextColor(theme_, variant_);
    impl_->instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(parent, GWLP_HINSTANCE));
    if (!impl_->instance) {
        impl_->instance = GetModuleHandleW(nullptr);
    }

    staticHwnd_ = CreateWindowExW(options.exStyle,
                                  L"STATIC",
                                  options.text.c_str(),
                                  options.style | WS_CHILD | WS_VISIBLE,
                                  0,
                                  0,
                                  0,
                                  0,
                                  parent,
                                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
                                  impl_->instance,
                                  nullptr);
    if (!staticHwnd_) {
        Destroy();
        return false;
    }

    if (!impl_->UpdateThemeResources()) {
        Destroy();
        return false;
    }
    SetWindowSubclass(staticHwnd_,
                      Impl::StaticSubclassProc,
                      reinterpret_cast<UINT_PTR>(this),
                      reinterpret_cast<DWORD_PTR>(impl_.get()));
    SetTextFormat(ResolveStaticTextFormat(variant_, options.textFormat));
    SetEllipsis(options.ellipsis);
    if (options.bitmap) {
        SetBitmap(options.bitmap);
    } else if (options.icon) {
        SetIcon(options.icon);
    }
    return true;
}

void Static::Destroy() {
    if (staticHwnd_) {
        RemoveWindowSubclass(staticHwnd_, Impl::StaticSubclassProc, reinterpret_cast<UINT_PTR>(this));
        DestroyWindow(staticHwnd_);
        staticHwnd_ = nullptr;
    }
    parentHwnd_ = nullptr;
    controlId_ = 0;
    contentMode_ = ContentMode::Text;
    if (impl_) {
        impl_->icon = nullptr;
        impl_->bitmap = nullptr;
    }
}

void Static::SetTheme(const Theme& theme) {
    const Theme previousTheme = theme_;
    const COLORREF previousBackgroundColor = backgroundColor_;
    const COLORREF previousTextColor = textColor_;
    theme_ = ResolveTheme(theme);
    if (!hasCustomBackgroundColor_) {
        backgroundColor_ = ResolveInheritedSurfaceColor(theme_, parentHwnd_, surfaceRole_);
    }
    textColor_ = ResolveStaticTextColor(theme_, variant_);
    if (!impl_->UpdateThemeResources()) {
        theme_ = previousTheme;
        backgroundColor_ = previousBackgroundColor;
        textColor_ = previousTextColor;
        impl_->UpdateThemeResources();
    }
}

void Static::SetText(const std::wstring& text) {
    contentMode_ = ContentMode::Text;
    if (staticHwnd_) {
        SetWindowTextW(staticHwnd_, text.c_str());
        InvalidateRect(staticHwnd_, nullptr, TRUE);
    }
}

std::wstring Static::GetText() const {
    if (!staticHwnd_) {
        return {};
    }
    const int length = GetWindowTextLengthW(staticHwnd_);
    if (length <= 0) {
        return {};
    }
    std::wstring text(length + 1, L'\0');
    GetWindowTextW(staticHwnd_, text.data(), length + 1);
    text.resize(length);
    return text;
}

void Static::SetIcon(HICON icon) {
    contentMode_ = ContentMode::Icon;
    impl_->icon = icon;
    impl_->bitmap = nullptr;
    if (staticHwnd_) {
        InvalidateRect(staticHwnd_, nullptr, TRUE);
    }
}

void Static::SetBitmap(HBITMAP bitmap) {
    contentMode_ = ContentMode::Bitmap;
    impl_->bitmap = bitmap;
    impl_->icon = nullptr;
    if (staticHwnd_) {
        InvalidateRect(staticHwnd_, nullptr, TRUE);
    }
}

void Static::ClearImage() {
    contentMode_ = ContentMode::Text;
    impl_->icon = nullptr;
    impl_->bitmap = nullptr;
    if (staticHwnd_) {
        InvalidateRect(staticHwnd_, nullptr, TRUE);
    }
}

void Static::SetBackgroundColor(COLORREF color) {
    const COLORREF previousBackgroundColor = backgroundColor_;
    hasCustomBackgroundColor_ = true;
    backgroundColor_ = color;
    if (!impl_->UpdateThemeResources()) {
        backgroundColor_ = previousBackgroundColor;
        impl_->UpdateThemeResources();
    }
}

void Static::SetTextFormat(UINT format) {
    textFormat_ = format;
    if (staticHwnd_) {
        InvalidateRect(staticHwnd_, nullptr, TRUE);
    }
}

void Static::SetEllipsis(bool enabled) {
    ellipsis_ = enabled;
    if (staticHwnd_) {
        InvalidateRect(staticHwnd_, nullptr, TRUE);
    }
}

}  // namespace darkui
