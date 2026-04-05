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

}  // namespace

ThemedWindowHost::ThemedWindowHost() = default;

ThemedWindowHost::~ThemedWindowHost() {
    Detach();
}

bool ThemedWindowHost::Attach(HWND hwnd, const Options& options) {    Detach();
    if (!hwnd) {        return false;
    }

    hwnd_ = hwnd;
    titleBarStyle_ = options.titleBarStyle;
    eraseBackground_ = options.eraseBackground;
    titleFontOffset_ = options.titleFontOffset;
    subtitleFontOffset_ = options.subtitleFontOffset;
    sectionFontOffset_ = options.sectionFontOffset;
    bodyFontOffset_ = options.bodyFontOffset;

    const bool ok = RebuildResources(options.theme);    return ok;
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

bool ThemedWindowHost::RebuildResources(const Theme& theme) {    const Theme resolved = ResolveTheme(theme);    HBRUSH newBackgroundBrush = CreateSolidBrush(resolved.background);
    FontSpec titleSpec = resolved.uiFont;
    titleSpec.height += titleFontOffset_;
    titleSpec.weight = FW_SEMIBOLD;    HFONT newTitleFont = CreateFont(titleSpec);
    FontSpec subtitleSpec = resolved.uiFont;
    subtitleSpec.height += subtitleFontOffset_;    HFONT newSubtitleFont = CreateFont(subtitleSpec);
    FontSpec sectionSpec = resolved.uiFont;
    sectionSpec.height += sectionFontOffset_;
    sectionSpec.weight = FW_SEMIBOLD;    HFONT newSectionFont = CreateFont(sectionSpec);
    FontSpec bodySpec = resolved.uiFont;
    bodySpec.height += bodyFontOffset_;    HFONT newBodyFont = CreateFont(bodySpec);
    if (!newBackgroundBrush || !newTitleFont || !newSubtitleFont || !newSectionFont || !newBodyFont) {        if (newBackgroundBrush) DeleteObject(newBackgroundBrush);
        if (newTitleFont) DeleteObject(newTitleFont);
        if (newSubtitleFont) DeleteObject(newSubtitleFont);
        if (newSectionFont) DeleteObject(newSectionFont);
        if (newBodyFont) DeleteObject(newBodyFont);
        return false;
    }

    if (backgroundBrush_) DeleteObject(backgroundBrush_);
    if (titleFont_) DeleteObject(titleFont_);
    if (subtitleFont_) DeleteObject(subtitleFont_);
    if (sectionFont_) DeleteObject(sectionFont_);
    if (bodyFont_) DeleteObject(bodyFont_);

    backgroundBrush_ = newBackgroundBrush;
    titleFont_ = newTitleFont;
    subtitleFont_ = newSubtitleFont;
    sectionFont_ = newSectionFont;
    bodyFont_ = newBodyFont;
    theme_ = resolved;
    themeManager_.SetTheme(theme_);    ApplyTitleBarTheme();    return true;
}

void ThemedWindowHost::ApplyTitleBarTheme() const {    if (!hwnd_) {        return;
    }

    if (titleBarStyle_ == TitleBarStyle::System) {        return;
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

    const HRESULT hr1 = DwmSetWindowAttribute(hwnd_, DWMWA_USE_IMMERSIVE_DARK_MODE, &immersive, sizeof(immersive));
    const HRESULT hr2 = DwmSetWindowAttribute(hwnd_, DWMWA_CAPTION_COLOR, &captionBackground, sizeof(captionBackground));
    const HRESULT hr3 = DwmSetWindowAttribute(hwnd_, DWMWA_TEXT_COLOR, &captionText, sizeof(captionText));
    const HRESULT hr4 = DwmSetWindowAttribute(hwnd_, DWMWA_BORDER_COLOR, &captionBorder, sizeof(captionBorder));}

}  // namespace darkui


