#pragma once

#include "darkui/dialog.h"

#include <functional>
#include <string>
#include <vector>

namespace darkui {

// Semantic theme presets for callers who want a ready-to-use dark palette.
// Usage:
// - Pick a preset when the page does not need a fully custom palette.
// - Pass the returned Theme into ThemedWindowHost::Options::theme or directly into control Create(...).
enum class ThemePreset {
    Graphite = 0,
    Ember = 1,
    Glacier = 2,
    Moss = 3,
    Mono = 4
};

// Creates a semantic dark theme from a compact set of palette tokens.
// Usage:
// - Prefer this when the caller wants one compact semantic palette entry instead of filling many Theme fields.
// - Use the returned Theme with ThemedWindowHost for the recommended top-level workflow.
inline Theme MakeSemanticTheme(COLORREF background,
                               COLORREF panel,
                               COLORREF text,
                               COLORREF highlightText,
                               COLORREF accent,
                               COLORREF accentSecondary,
                               const std::wstring& fontFamily = L"Segoe UI",
                               int fontSize = 20,
                               int secondaryFontSize = 18) {
    Theme theme;
    theme.useSemanticPalette = true;
    theme.primaryBackground = background;
    theme.secondaryBackground = panel;
    theme.primaryText = text;
    theme.highlightText = highlightText;
    theme.accent = accent;
    theme.accentSecondary = accentSecondary;
    theme.fontFamily = fontFamily;
    theme.fontSize = fontSize;
    theme.secondaryFontSize = secondaryFontSize;
    return ResolveTheme(theme);
}

// Returns a ready-made semantic preset theme.
// Usage:
// - Prefer this when the caller wants a consistent built-in palette with no additional setup.
// - Use the returned Theme with ThemedWindowHost::Options::theme for the shortest recommended startup path.
inline Theme MakePresetTheme(ThemePreset preset = ThemePreset::Graphite) {
    switch (preset) {
    case ThemePreset::Ember:
        return MakeSemanticTheme(RGB(22, 16, 15), RGB(38, 27, 24), RGB(244, 232, 224), RGB(255, 247, 240), RGB(226, 112, 74), RGB(146, 69, 48));
    case ThemePreset::Glacier:
        return MakeSemanticTheme(RGB(14, 21, 28), RGB(24, 38, 48), RGB(226, 238, 244), RGB(247, 251, 255), RGB(96, 188, 224), RGB(51, 110, 146));
    case ThemePreset::Moss:
        return MakeSemanticTheme(RGB(16, 22, 18), RGB(28, 38, 30), RGB(230, 238, 226), RGB(248, 252, 246), RGB(108, 176, 118), RGB(58, 108, 66));
    case ThemePreset::Mono:
        return MakeSemanticTheme(RGB(10, 10, 10), RGB(22, 22, 22), RGB(230, 230, 230), RGB(252, 252, 252), RGB(190, 190, 190), RGB(110, 110, 110));
    case ThemePreset::Graphite:
    default:
        return MakeSemanticTheme(RGB(16, 18, 22), RGB(28, 32, 38), RGB(232, 236, 241), RGB(248, 250, 252), RGB(92, 137, 210), RGB(58, 88, 144));
    }
}

// Lightweight binder that remembers controls and reapplies one theme to all of them.
// Usage:
// - Use ThemeBinder when the caller only wants a remembered binding list.
// - Prefer ThemedWindowHost::theme_manager() when the page also needs a stored current theme.
// - Bind controls once, then call Apply(theme) whenever the page palette changes.
class ThemeBinder {
public:
    ThemeBinder() = default;

    template <typename Control>
    void Bind(Control& control) {
        bindings_.push_back([&control](const Theme& theme) { control.SetTheme(theme); });
    }

    template <typename First, typename... Rest>
    void Bind(First& first, Rest&... rest) {
        Bind(first);
        (Bind(rest), ...);
    }

    void Clear() {
        bindings_.clear();
    }

    void Apply(const Theme& theme) const {
        for (const auto& binding : bindings_) {
            binding(theme);
        }
    }

private:
    std::vector<std::function<void(const Theme&)>> bindings_;
};

// Stores the current theme together with a binding list so callers can update all registered controls at once.
// Usage:
// - This is the preferred repeated theme-switch entry for pages that already have a current Theme.
// - In normal window code, access it through ThemedWindowHost::theme_manager().
// - Bind controls once, call SetTheme(newTheme), then Apply() to push the updated theme through the page.
class ThemeManager {
public:
    ThemeManager() = default;
    explicit ThemeManager(const Theme& theme) : theme_(theme) {}

    void SetTheme(const Theme& theme) {
        theme_ = theme;
    }

    const Theme& theme() const {
        return theme_;
    }

    template <typename... Controls>
    void Bind(Controls&... controls) {
        binder_.Bind(controls...);
    }

    void Clear() {
        binder_.Clear();
    }

    void Apply() const {
        binder_.Apply(theme_);
    }

private:
    Theme theme_{};
    ThemeBinder binder_{};
};

// Shared alias for dialog quick helpers.
// Usage:
// - Fill DialogOptions / QuickDialogOptions, then pass it into ShowMessageDialog(...).
using DialogOptions = Dialog::Options;
using QuickDialogOptions = Dialog::Options;

// Opens a simple dark message dialog with one helper call.
// Usage:
// - Prefer this when the caller only needs a message-style popup and does not want to hold a Dialog object.
// - For custom embedded controls, keep using darkui::Dialog directly.
inline Dialog::Result ShowMessageDialog(HWND owner, int controlId, const Theme& theme, const Dialog::Options& options) {
    Dialog dialog;
    if (!dialog.Create(owner, controlId, theme, options)) {
        return Dialog::Result::None;
    }
    return dialog.ShowModal();
}

// Opens a confirm/cancel dialog with conventional button labels.
// Usage:
// - Prefer this for common confirmation prompts with standard confirm/cancel flow.
inline Dialog::Result ShowConfirmDialog(HWND owner,
                                        int controlId,
                                        const Theme& theme,
                                        const std::wstring& title,
                                        const std::wstring& message,
                                        const std::wstring& confirmText = L"Confirm",
                                        const std::wstring& cancelText = L"Cancel") {
    Dialog::Options options;
    options.title = title;
    options.message = message;
    options.confirmText = confirmText;
    options.cancelText = cancelText;
    options.cancelVisible = true;
    return ShowMessageDialog(owner, controlId, theme, options);
}

// Opens an information dialog with a single acknowledge button.
// Usage:
// - Prefer this for simple notification-style popups where cancel is not needed.
inline Dialog::Result ShowInfoDialog(HWND owner,
                                     int controlId,
                                     const Theme& theme,
                                     const std::wstring& title,
                                     const std::wstring& message,
                                     const std::wstring& confirmText = L"OK") {
    Dialog::Options options;
    options.title = title;
    options.message = message;
    options.confirmText = confirmText;
    options.cancelText = L"";
    options.cancelVisible = false;
    return ShowMessageDialog(owner, controlId, theme, options);
}

}  // namespace darkui
