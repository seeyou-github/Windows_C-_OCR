#pragma once

#include "darkui/quick.h"

namespace darkui {

// Title-bar styling mode for a themed top-level window host.
enum class TitleBarStyle {
    System = 0,
    Dark,
    Black,
    Theme
};

// Lightweight host that owns page-level theme resources for a top-level window.
// Usage:
// - Fill ThemedWindowHost::Options and call Attach(hwnd, options) in WM_CREATE.
// - Fill per-control Options and call Create(parent, id, theme(), options).
// - Bind page controls through theme_manager().
// - Call ApplyTheme(newTheme) when the whole window should switch theme.
// - Forward WM_ERASEBKGND to HandleEraseBackground() when you want uniform fill.
// - Read title/subtitle/section/body fonts from the host during WM_PAINT.
class ThemedWindowHost {
public:
    struct Options {
        Theme theme{};
        TitleBarStyle titleBarStyle = TitleBarStyle::Theme;
        bool eraseBackground = true;
        int titleFontOffset = -10;
        int subtitleFontOffset = 2;
        int sectionFontOffset = -2;
        int bodyFontOffset = 0;
    };

    ThemedWindowHost();
    ~ThemedWindowHost();

    ThemedWindowHost(const ThemedWindowHost&) = delete;
    ThemedWindowHost& operator=(const ThemedWindowHost&) = delete;

    bool Attach(HWND hwnd, const Options& options);
    void Detach();

    HWND hwnd() const { return hwnd_; }
    const Theme& theme() const { return theme_; }
    ThemeManager& theme_manager() { return themeManager_; }
    const ThemeManager& theme_manager() const { return themeManager_; }

    void SetTheme(const Theme& theme);
    void ApplyTheme(const Theme& theme);
    void Apply();

    HBRUSH background_brush() const { return backgroundBrush_; }
    HFONT title_font() const { return titleFont_; }
    HFONT subtitle_font() const { return subtitleFont_; }
    HFONT section_font() const { return sectionFont_; }
    HFONT body_font() const { return bodyFont_; }

    void SetTitleBarStyle(TitleBarStyle style);
    bool HandleEraseBackground(HDC dc) const;
    void FillBackground(HDC dc) const;
    void Invalidate(bool erase = true) const;

private:
    bool RebuildResources(const Theme& theme);
    void ApplyTitleBarTheme() const;

    HWND hwnd_ = nullptr;
    Theme theme_{};
    ThemeManager themeManager_{};
    TitleBarStyle titleBarStyle_ = TitleBarStyle::Theme;
    bool eraseBackground_ = true;
    int titleFontOffset_ = -10;
    int subtitleFontOffset_ = 2;
    int sectionFontOffset_ = -2;
    int bodyFontOffset_ = 0;
    HBRUSH backgroundBrush_ = nullptr;
    HFONT titleFont_ = nullptr;
    HFONT subtitleFont_ = nullptr;
    HFONT sectionFont_ = nullptr;
    HFONT bodyFont_ = nullptr;
};

}  // namespace darkui
