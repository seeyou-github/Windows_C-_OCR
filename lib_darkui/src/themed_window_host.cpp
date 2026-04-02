#include "darkui/themed_window_host.h"

#include <dwmapi.h>

namespace darkui {
namespace {

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_CAPTION_COLOR
#define DWMWA_CAPTION_COLOR 35
#endif
#ifndef DWMWA_TEXT_COLOR
#define DWMWA_TEXT_COLOR 36
#endif
#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif

HFONT CreateSafeFont(const FontSpec& requested) {
    FontSpec spec = requested;
    if (spec.height == 0) {
        spec.height = -18;
    }
    if (spec.family.empty()) {
        spec.family = L"Segoe UI";
    }

    HFONT font = CreateFont(spec);
    if (font) {
        return font;
    }

    NONCLIENTMETRICSW metrics{};
    metrics.cbSize = sizeof(metrics);
    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0)) {
        metrics.lfMessageFont.lfHeight = spec.height;
        metrics.lfMessageFont.lfWeight = spec.weight;
        metrics.lfMessageFont.lfItalic = spec.italic ? TRUE : FALSE;
        return CreateFontIndirectW(&metrics.lfMessageFont);
    }

    return nullptr;
}

}  // namespace

ThemedWindowHost::ThemedWindowHost() = default;

ThemedWindowHost::~ThemedWindowHost() {
    Detach();
}

bool ThemedWindowHost::Attach(HWND hwnd, const Options& options) {
    Detach();
    if (!hwnd) {
        return false;
    }

    hwnd_ = hwnd;
    titleBarStyle_ = options.titleBarStyle;
    eraseBackground_ = options.eraseBackground;
    titleFontOffset_ = options.titleFontOffset;
    subtitleFontOffset_ = options.subtitleFontOffset;
    sectionFontOffset_ = options.sectionFontOffset;
    bodyFontOffset_ = options.bodyFontOffset;

    return RebuildResources(options.theme);
}

void ThemedWindowHost::Detach() {
    if (backgroundBrush_) {
        DeleteObject(backgroundBrush_);
        backgroundBrush_ = nullptr;
    }
    if (titleFont_) {
        DeleteObject(titleFont_);
        titleFont_ = nullptr;
    }
    if (subtitleFont_) {
        DeleteObject(subtitleFont_);
        subtitleFont_ = nullptr;
    }
    if (sectionFont_) {
        DeleteObject(sectionFont_);
        sectionFont_ = nullptr;
    }
    if (bodyFont_) {
        DeleteObject(bodyFont_);
        bodyFont_ = nullptr;
    }
    hwnd_ = nullptr;
    themeManager_.Clear();
    themeManager_.SetTheme(Theme{});
    theme_ = Theme{};
}

void ThemedWindowHost::SetTheme(const Theme& theme) {
    theme_ = ResolveTheme(theme);
    themeManager_.SetTheme(theme_);
}

void ThemedWindowHost::ApplyTheme(const Theme& theme) {
    if (!RebuildResources(theme)) {
        return;
    }
    Apply();
}

void ThemedWindowHost::Apply() {
    ApplyTitleBarTheme();
    themeManager_.Apply();
    Invalidate(true);
}

void ThemedWindowHost::SetTitleBarStyle(TitleBarStyle style) {
    titleBarStyle_ = style;
    ApplyTitleBarTheme();
}

bool ThemedWindowHost::HandleEraseBackground(HDC dc) const {
    if (!eraseBackground_ || !dc) {
        return false;
    }
    FillBackground(dc);
    return true;
}

void ThemedWindowHost::FillBackground(HDC dc) const {
    if (!dc || !hwnd_) {
        return;
    }

    RECT rect{};
    GetClientRect(hwnd_, &rect);
    FillRect(dc, &rect, backgroundBrush_ ? backgroundBrush_ : reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
}

void ThemedWindowHost::Invalidate(bool erase) const {
    if (hwnd_) {
        InvalidateRect(hwnd_, nullptr, erase ? TRUE : FALSE);
    }
}

bool ThemedWindowHost::RebuildResources(const Theme& theme) {
    const Theme resolved = ResolveTheme(theme);

    HBRUSH newBackgroundBrush = CreateSolidBrush(resolved.background);
    if (!newBackgroundBrush) {
        newBackgroundBrush = CreateSolidBrush(RGB(18, 20, 24));
    }

    FontSpec titleSpec = resolved.uiFont;
    titleSpec.height += titleFontOffset_;
    titleSpec.weight = FW_SEMIBOLD;
    HFONT newTitleFont = CreateSafeFont(titleSpec);

    FontSpec subtitleSpec = resolved.uiFont;
    subtitleSpec.height += subtitleFontOffset_;
    HFONT newSubtitleFont = CreateSafeFont(subtitleSpec);

    FontSpec sectionSpec = resolved.uiFont;
    sectionSpec.height += sectionFontOffset_;
    sectionSpec.weight = FW_SEMIBOLD;
    HFONT newSectionFont = CreateSafeFont(sectionSpec);

    FontSpec bodySpec = resolved.uiFont;
    bodySpec.height += bodyFontOffset_;
    HFONT newBodyFont = CreateSafeFont(bodySpec);

    if (!newBackgroundBrush) {
        newBackgroundBrush = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    }
    if (!newTitleFont) {
        newTitleFont = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    }
    if (!newSubtitleFont) {
        newSubtitleFont = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    }
    if (!newSectionFont) {
        newSectionFont = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    }
    if (!newBodyFont) {
        newBodyFont = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    }

    if (backgroundBrush_ && backgroundBrush_ != GetStockObject(BLACK_BRUSH)) DeleteObject(backgroundBrush_);
    if (titleFont_ && titleFont_ != GetStockObject(DEFAULT_GUI_FONT)) DeleteObject(titleFont_);
    if (subtitleFont_ && subtitleFont_ != GetStockObject(DEFAULT_GUI_FONT)) DeleteObject(subtitleFont_);
    if (sectionFont_ && sectionFont_ != GetStockObject(DEFAULT_GUI_FONT)) DeleteObject(sectionFont_);
    if (bodyFont_ && bodyFont_ != GetStockObject(DEFAULT_GUI_FONT)) DeleteObject(bodyFont_);

    backgroundBrush_ = newBackgroundBrush;
    titleFont_ = newTitleFont;
    subtitleFont_ = newSubtitleFont;
    sectionFont_ = newSectionFont;
    bodyFont_ = newBodyFont;
    theme_ = resolved;
    themeManager_.SetTheme(theme_);
    ApplyTitleBarTheme();
    return true;
}

void ThemedWindowHost::ApplyTitleBarTheme() const {
    if (!hwnd_) {
        return;
    }

    if (titleBarStyle_ == TitleBarStyle::System) {
        return;
    }

    BOOL immersive = TRUE;
    COLORREF captionBackground = theme_.windowCaptionBackground;
    COLORREF captionText = theme_.windowCaptionText;
    COLORREF captionBorder = theme_.windowCaptionBorder;

    switch (titleBarStyle_) {
    case TitleBarStyle::Black:
        captionBackground = RGB(0, 0, 0);
        captionText = RGB(255, 255, 255);
        captionBorder = RGB(0, 0, 0);
        break;
    case TitleBarStyle::Dark:
        captionBackground = theme_.panel;
        captionText = theme_.text;
        captionBorder = theme_.border;
        break;
    case TitleBarStyle::Theme:
        break;
    case TitleBarStyle::System:
    default:
        return;
    }

    DwmSetWindowAttribute(hwnd_, DWMWA_USE_IMMERSIVE_DARK_MODE, &immersive, sizeof(immersive));
    DwmSetWindowAttribute(hwnd_, DWMWA_CAPTION_COLOR, &captionBackground, sizeof(captionBackground));
    DwmSetWindowAttribute(hwnd_, DWMWA_TEXT_COLOR, &captionText, sizeof(captionText));
    DwmSetWindowAttribute(hwnd_, DWMWA_BORDER_COLOR, &captionBorder, sizeof(captionBorder));
}

}  // namespace darkui
