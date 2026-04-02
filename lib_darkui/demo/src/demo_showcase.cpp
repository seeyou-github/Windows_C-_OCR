#include <windows.h>
#include <commctrl.h>
#include <algorithm>
#include <string>
#include <vector>

#include "darkui/darkui.h"

namespace {

constexpr wchar_t kDemoClassName[] = L"DarkUiShowcaseWindow";
constexpr wchar_t kPageClassName[] = L"DarkUiShowcasePage";
constexpr wchar_t kDemoTitle[] = L"lib_darkui showcase";

enum ControlId {
    ID_THEME_GRAPHITE = 9001,
    ID_THEME_EMBER,
    ID_THEME_GLACIER,
    ID_THEME_MOSS,
    ID_THEME_MONO,
    ID_MAIN_TAB,

    ID_OVERVIEW_PROFILE = 9101,
    ID_OVERVIEW_PRIMARY,
    ID_OVERVIEW_SECONDARY,
    ID_OVERVIEW_STATUS,
    ID_OVERVIEW_SYNC,
    ID_OVERVIEW_CPU,
    ID_OVERVIEW_PANEL_LEFT,
    ID_OVERVIEW_PANEL_RIGHT,
    ID_OVERVIEW_PANEL_BOTTOM,

    ID_CONTROLS_EXPOSURE = 9201,
    ID_CONTROLS_BALANCE,
    ID_CONTROLS_SCROLL_H,
    ID_CONTROLS_SCROLL_V,
    ID_CONTROLS_SEARCH,
    ID_CONTROLS_NOTES,

    ID_DATA_FILTER = 9301,
    ID_DATA_REFRESH,
    ID_DATA_TABLE,
    ID_DATA_QUEUE,

    ID_EXPANDED_STATIC_TITLE = 9401,
    ID_EXPANDED_STATIC_HELPER,
    ID_EXPANDED_CHECK_AUTOSAVE,
    ID_EXPANDED_CHECK_COMPACT,
    ID_EXPANDED_RADIO_GRID,
    ID_EXPANDED_RADIO_FOCUS,
    ID_EXPANDED_RADIO_FLOW,
    ID_EXPANDED_DIALOG,
    ID_EXPANDED_RESULT,
    ID_EXPANDED_PANEL_LEFT,
    ID_EXPANDED_PANEL_RIGHT,

    ID_PAGE_OVERVIEW = 9501,
    ID_PAGE_CONTROLS,
    ID_PAGE_DATA,
    ID_PAGE_EXPANDED
};

enum class PageKind { Overview, Controls, Data, Expanded };

struct OverviewPanel {
    darkui::Panel leftCard;
    darkui::Panel rightCard;
    darkui::Panel bottomCard;
    darkui::ComboBox profile;
    darkui::Button primary;
    darkui::Button secondary;
    darkui::Static status;
    darkui::ProgressBar sync;
    darkui::ProgressBar cpu;
};

struct ControlsPanel {
    darkui::Slider exposure;
    darkui::Slider balance;
    darkui::ScrollBar timeline;
    darkui::ScrollBar navigator;
    darkui::Edit search;
    darkui::Edit notes;
};

struct DataPanel {
    darkui::ComboBox filter;
    darkui::Button refresh;
    darkui::Table table;
    darkui::ListBox queue;
};

struct ExpandedPanel {
    darkui::Panel leftCard;
    darkui::Panel rightCard;
    darkui::Static headline;
    darkui::Static helper;
    darkui::CheckBox autoSave;
    darkui::CheckBox compact;
    darkui::RadioButton grid;
    darkui::RadioButton focus;
    darkui::RadioButton flow;
    darkui::Button dialogButton;
    darkui::Static result;
};

struct AppState {
    darkui::ThemedWindowHost host;
    darkui::Theme theme;
    int themeIndex = 0;
    darkui::ThemeManager themeManager;

    darkui::Tab tab;
    darkui::Button themeGraphite;
    darkui::Button themeEmber;
    darkui::Button themeGlacier;
    darkui::Button themeMoss;
    darkui::Button themeMono;

    HWND overviewPage = nullptr;
    HWND controlsPage = nullptr;
    HWND dataPage = nullptr;
    HWND expandedPage = nullptr;

    OverviewPanel overview;
    ControlsPanel controls;
    DataPanel data;
    ExpandedPanel expanded;
};

struct PageState {
    AppState* app = nullptr;
    PageKind kind = PageKind::Overview;
};

void ApplyThemeToControls(AppState* state);

RECT InsetRectCopy(const RECT& rect, int dx, int dy) {
    RECT rc = rect;
    rc.left += dx; rc.top += dy; rc.right -= dx; rc.bottom -= dy;
    return rc;
}

RECT SliceTop(const RECT& rect, int height) {
    RECT rc = rect;
    rc.bottom = std::min(rect.bottom, rect.top + height);
    return rc;
}

RECT SliceLeft(const RECT& rect, int width) {
    RECT rc = rect;
    rc.right = std::min(rect.right, rect.left + width);
    return rc;
}

void DrawLabel(HDC dc, HFONT font, COLORREF color, const RECT& rect, const wchar_t* text, UINT format) {
    HFONT oldFont = font ? reinterpret_cast<HFONT>(SelectObject(dc, font)) : nullptr;
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, text, -1, const_cast<RECT*>(&rect), format | DT_NOPREFIX);
    if (oldFont) SelectObject(dc, oldFont);
}

void FillRoundedRect(HDC dc, const RECT& rect, int radius, COLORREF fill, COLORREF border) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(brush);
    DeleteObject(pen);
}

darkui::Theme MakeThemeByIndex(int index) {
    switch (index) {
    case 1: return darkui::MakePresetTheme(darkui::ThemePreset::Ember);
    case 2: return darkui::MakePresetTheme(darkui::ThemePreset::Glacier);
    case 3: return darkui::MakePresetTheme(darkui::ThemePreset::Moss);
    case 4: return darkui::MakePresetTheme(darkui::ThemePreset::Mono);
    default: return darkui::MakePresetTheme(darkui::ThemePreset::Graphite);
    }
}

const wchar_t* ThemeName(int index) {
    switch (index) {
    case 1: return L"Ember";
    case 2: return L"Glacier";
    case 3: return L"Moss";
    case 4: return L"Mono";
    default: return L"Graphite";
    }
}

void UpdateExpandedSummary(AppState* state) {
    std::wstring text = L"Theme: ";
    text += ThemeName(state->themeIndex);
    text += L"\nLayout: ";
    if (state->expanded.grid.GetChecked()) text += L"Grid";
    else if (state->expanded.focus.GetChecked()) text += L"Focus";
    else text += L"Flow";
    text += L"\nAutosave: ";
    text += state->expanded.autoSave.GetChecked() ? L"On" : L"Off";
    text += L"\nCompact Rails: ";
    text += state->expanded.compact.GetChecked() ? L"On" : L"Off";
    state->expanded.result.SetText(text);
}

void RefreshDataRows(AppState* state) {
    const int filter = state->data.filter.GetSelection();
    std::vector<darkui::TableRow> rows{
        {L"Campaign Boards", L"Cloud", L"Ready", L"4.2 GB", L"12:44"},
        {L"Studio Takes", L"Local", L"Queued", L"2.1 GB", L"11:02"},
        {L"Audio Stems", L"NAS", L"Ready", L"918 MB", L"09:31"},
        {L"Launch Review", L"Archive", L"Locked", L"690 MB", L"Yesterday"},
        {L"Proxy Drafts", L"Cloud", L"Syncing", L"6.8 GB", L"Now"},
        {L"Final Captures", L"SSD", L"Ready", L"1.7 GB", L"14:08"}
    };
    if (filter == 1) rows.erase(std::remove_if(rows.begin(), rows.end(), [](const darkui::TableRow& r) { return r[2] != L"Ready"; }), rows.end());
    if (filter == 2) rows.erase(std::remove_if(rows.begin(), rows.end(), [](const darkui::TableRow& r) { return r[1] != L"Cloud"; }), rows.end());
    state->data.table.SetRows(rows);
}

void ShowExpandedDialog(HWND ownerWindow, AppState* state) {
    const auto result = darkui::ShowConfirmDialog(ownerWindow,
                                                  9701,
                                                  state->theme,
                                                  L"Publish Session",
                                                  L"Apply the current expanded controls profile to every editing station in the workspace?",
                                                  L"Publish",
                                                  L"Cancel");
    state->expanded.result.SetText(result == darkui::Dialog::Result::Confirm ? L"Dialog result: Published" : L"Dialog result: Cancelled");
}

void CleanupAppState(AppState* state) {
    if (!state) return;
    delete state;
}

void ApplyTheme(AppState* state, HWND window, int themeIndex) {
    state->themeIndex = themeIndex;
    state->theme = MakeThemeByIndex(themeIndex);
    state->host.ApplyTheme(state->theme);
    ApplyThemeToControls(state);
    InvalidateRect(window, nullptr, TRUE);
    if (state->overviewPage) InvalidateRect(state->overviewPage, nullptr, TRUE);
    if (state->controlsPage) InvalidateRect(state->controlsPage, nullptr, TRUE);
    if (state->dataPage) InvalidateRect(state->dataPage, nullptr, TRUE);
    if (state->expandedPage) InvalidateRect(state->expandedPage, nullptr, TRUE);
}

void ApplyThemeToControls(AppState* state) {
    state->themeManager.SetTheme(state->theme);
    state->themeManager.Apply();
    state->themeGraphite.SetSurfaceColor(state->themeIndex == 0 ? state->theme.panel : state->theme.background);
    state->themeEmber.SetSurfaceColor(state->themeIndex == 1 ? state->theme.panel : state->theme.background);
    state->themeGlacier.SetSurfaceColor(state->themeIndex == 2 ? state->theme.panel : state->theme.background);
    state->themeMoss.SetSurfaceColor(state->themeIndex == 3 ? state->theme.panel : state->theme.background);
    state->themeMono.SetSurfaceColor(state->themeIndex == 4 ? state->theme.panel : state->theme.background);
    state->data.refresh.SetSurfaceColor(state->theme.background);
    UpdateExpandedSummary(state);
}

void LayoutMainWindow(HWND window, AppState* state) {
    RECT client{};
    GetClientRect(window, &client);
    const int outer = 28;
    const int headerHeight = 104;
    const int buttonWidth = 102;
    const int buttonHeight = 36;
    const int gap = 10;
    const int right = client.right - outer;
    const int top = outer + 10;

    MoveWindow(state->themeMono.hwnd(), right - buttonWidth, top, buttonWidth, buttonHeight, TRUE);
    MoveWindow(state->themeMoss.hwnd(), right - buttonWidth * 2 - gap, top, buttonWidth, buttonHeight, TRUE);
    MoveWindow(state->themeGlacier.hwnd(), right - buttonWidth * 3 - gap * 2, top, buttonWidth, buttonHeight, TRUE);
    MoveWindow(state->themeEmber.hwnd(), right - buttonWidth * 4 - gap * 3, top, buttonWidth, buttonHeight, TRUE);
    MoveWindow(state->themeGraphite.hwnd(), right - buttonWidth * 5 - gap * 4, top, buttonWidth, buttonHeight, TRUE);

    MoveWindow(state->tab.hwnd(),
               outer,
               outer + headerHeight,
               std::max<LONG>(460, client.right - outer * 2),
               std::max<LONG>(460, client.bottom - outer * 2 - headerHeight),
               TRUE);
}

void LayoutOverviewPage(HWND page, AppState* state) {
    RECT client{}; GetClientRect(page, &client);
    RECT content = InsetRectCopy(client, 28, 28);
    RECT top = SliceTop(content, 258);
    RECT bottom{content.left, top.bottom + 20, content.right, content.bottom};
    RECT left = SliceLeft(top, ((top.right - top.left) - 20) / 2);
    RECT right{left.right + 20, top.top, top.right, top.bottom};
    MoveWindow(state->overview.leftCard.hwnd(), left.left, left.top, left.right - left.left, left.bottom - left.top, TRUE);
    MoveWindow(state->overview.rightCard.hwnd(), right.left, right.top, right.right - right.left, right.bottom - right.top, TRUE);
    MoveWindow(state->overview.bottomCard.hwnd(), bottom.left, bottom.top, bottom.right - bottom.left, bottom.bottom - bottom.top, TRUE);

    RECT li = InsetRectCopy(RECT{0, 0, left.right - left.left, left.bottom - left.top}, 24, 24);
    RECT ri = InsetRectCopy(RECT{0, 0, right.right - right.left, right.bottom - right.top}, 24, 24);
    RECT bi = InsetRectCopy(RECT{0, 0, bottom.right - bottom.left, bottom.bottom - bottom.top}, 24, 24);

    MoveWindow(state->overview.profile.hwnd(), li.left, li.top + 54, li.right - li.left, 34, TRUE);
    MoveWindow(state->overview.primary.hwnd(), ri.left, ri.top + 58, 168, 38, TRUE);
    MoveWindow(state->overview.secondary.hwnd(), ri.left + 184, ri.top + 58, 168, 38, TRUE);
    MoveWindow(state->overview.status.hwnd(), ri.left, ri.top + 120, ri.right - ri.left, 48, TRUE);
    MoveWindow(state->overview.sync.hwnd(), bi.left, bi.top + 56, bi.right - bi.left, 34, TRUE);
    MoveWindow(state->overview.cpu.hwnd(), bi.left, bi.top + 128, bi.right - bi.left, 34, TRUE);
}

void LayoutControlsPage(HWND page, AppState* state) {
    RECT client{}; GetClientRect(page, &client);
    RECT content = InsetRectCopy(client, 28, 28);
    RECT top = SliceTop(content, 260);
    RECT bottom{content.left, top.bottom + 20, content.right, content.bottom};
    RECT left = SliceLeft(bottom, ((bottom.right - bottom.left) - 20) / 2);
    RECT right{left.right + 20, bottom.top, bottom.right, bottom.bottom};
    RECT ti = InsetRectCopy(top, 24, 24);
    RECT li = InsetRectCopy(left, 24, 24);
    RECT ri = InsetRectCopy(right, 24, 24);

    MoveWindow(state->controls.exposure.hwnd(), ti.left, ti.top + 62, ti.right - ti.left, 42, TRUE);
    MoveWindow(state->controls.balance.hwnd(), ti.left, ti.top + 156, ti.right - ti.left, 42, TRUE);
    MoveWindow(state->controls.timeline.hwnd(), li.left, li.top + 78, li.right - li.left, state->theme.scrollBarThickness + 12, TRUE);
    MoveWindow(state->controls.navigator.hwnd(), li.right - 22, li.top + 126, 18, std::max(100L, li.bottom - li.top - 144), TRUE);
    MoveWindow(state->controls.search.hwnd(), ri.left, ri.top + 62, ri.right - ri.left, 40, TRUE);
    MoveWindow(state->controls.notes.hwnd(), ri.left, ri.top + 122, ri.right - ri.left, std::max(118L, ri.bottom - ri.top - 136), TRUE);
}

void LayoutDataPage(HWND page, AppState* state) {
    RECT client{}; GetClientRect(page, &client);
    RECT content = InsetRectCopy(client, 28, 28);
    RECT bar = SliceTop(content, 42);
    RECT bottom{content.left, bar.bottom + 20, content.right, content.bottom};
    RECT left = SliceLeft(bottom, ((bottom.right - bottom.left) - 20) / 2);
    RECT right{left.right + 20, bottom.top, bottom.right, bottom.bottom};

    MoveWindow(state->data.filter.hwnd(), bar.left, bar.top, 240, 42, TRUE);
    MoveWindow(state->data.refresh.hwnd(), bar.right - 146, bar.top, 146, 42, TRUE);
    MoveWindow(state->data.table.hwnd(), left.left, left.top, left.right - left.left, left.bottom - left.top, TRUE);
    MoveWindow(state->data.queue.hwnd(), right.left, right.top, right.right - right.left, right.bottom - right.top, TRUE);
}

void LayoutExpandedPage(HWND page, AppState* state) {
    RECT client{}; GetClientRect(page, &client);
    RECT content = InsetRectCopy(client, 28, 28);
    RECT left = SliceLeft(content, ((content.right - content.left) - 20) / 2);
    RECT right{left.right + 20, content.top, content.right, content.bottom};
    RECT leftCard{left.left, left.top + 60, left.right, left.bottom};
    RECT rightCard{right.left, right.top + 60, right.right, right.bottom};
    MoveWindow(state->expanded.leftCard.hwnd(), leftCard.left, leftCard.top, leftCard.right - leftCard.left, leftCard.bottom - leftCard.top, TRUE);
    MoveWindow(state->expanded.rightCard.hwnd(), rightCard.left, rightCard.top, rightCard.right - rightCard.left, rightCard.bottom - rightCard.top, TRUE);

    RECT li = InsetRectCopy(RECT{0, 0, leftCard.right - leftCard.left, leftCard.bottom - leftCard.top}, 24, 24);
    RECT ri = InsetRectCopy(RECT{0, 0, rightCard.right - rightCard.left, rightCard.bottom - rightCard.top}, 24, 24);

    MoveWindow(state->expanded.headline.hwnd(), li.left, li.top + 12, li.right - li.left, 28, TRUE);
    MoveWindow(state->expanded.helper.hwnd(), li.left, li.top + 52, li.right - li.left, 62, TRUE);
    MoveWindow(state->expanded.autoSave.hwnd(), li.left, li.top + 138, li.right - li.left, 28, TRUE);
    MoveWindow(state->expanded.compact.hwnd(), li.left, li.top + 176, li.right - li.left, 28, TRUE);
    MoveWindow(state->expanded.grid.hwnd(), li.left, li.top + 232, li.right - li.left, 28, TRUE);
    MoveWindow(state->expanded.focus.hwnd(), li.left, li.top + 268, li.right - li.left, 28, TRUE);
    MoveWindow(state->expanded.flow.hwnd(), li.left, li.top + 304, li.right - li.left, 28, TRUE);
    MoveWindow(state->expanded.dialogButton.hwnd(), ri.left, ri.top + 20, 196, 40, TRUE);
    MoveWindow(state->expanded.result.hwnd(), ri.left, ri.top + 88, ri.right - ri.left, std::max(120L, ri.bottom - ri.top - 104), TRUE);
}

void LayoutPage(HWND page, AppState* state, PageKind kind) {
    switch (kind) {
    case PageKind::Overview: LayoutOverviewPage(page, state); break;
    case PageKind::Controls: LayoutControlsPage(page, state); break;
    case PageKind::Data: LayoutDataPage(page, state); break;
    case PageKind::Expanded: LayoutExpandedPage(page, state); break;
    }
}

void PaintMainWindow(HWND window, HDC dc, AppState* state) {
    RECT client{}; GetClientRect(window, &client);
    state->host.FillBackground(dc);
    DrawLabel(dc, state->host.title_font(), state->theme.text, RECT{28, 24, client.right - 28, 58}, L"DarkUI Semantic Showcase", DT_LEFT | DT_TOP | DT_SINGLELINE);
    DrawLabel(dc,
              state->host.subtitle_font(),
              state->theme.mutedText,
              RECT{28, 62, client.right - 560, 92},
              L"Five semantic palettes drive every control from one theme entry. The new controls live in the Expanded page, and the window title bar stays black.",
              DT_LEFT | DT_TOP | DT_WORDBREAK);
}

void PaintOverviewPage(HWND page, HDC dc, PageState* state) {
    RECT client{}; GetClientRect(page, &client); FillRect(dc, &client, state->app->host.background_brush());
    RECT content = InsetRectCopy(client, 28, 28);
    RECT top = SliceTop(content, 258);
    RECT bottom{content.left, top.bottom + 20, content.right, content.bottom};
    RECT left = SliceLeft(top, ((top.right - top.left) - 20) / 2);
    RECT right{left.right + 20, top.top, top.right, top.bottom};
    RECT li = InsetRectCopy(left, 24, 24);
    RECT ri = InsetRectCopy(right, 24, 24);
    RECT bi = InsetRectCopy(bottom, 24, 24);
    DrawLabel(dc, state->app->host.section_font(), state->app->theme.text, SliceTop(li, 26), L"Theme Inputs", DT_LEFT | DT_TOP | DT_SINGLELINE);
    DrawLabel(dc, state->app->host.body_font(), state->app->theme.mutedText, RECT{li.left, li.top + 30, li.right, li.top + 52}, L"Latest semantic theme entry through one shared Theme object.", DT_LEFT | DT_TOP | DT_WORDBREAK);
    DrawLabel(dc, state->app->host.section_font(), state->app->theme.text, SliceTop(ri, 26), L"Actions And Status", DT_LEFT | DT_TOP | DT_SINGLELINE);
    DrawLabel(dc, state->app->host.body_font(), state->app->theme.mutedText, RECT{ri.left, ri.top + 30, ri.right, ri.top + 52}, L"Buttons, static text, and progress cards all follow the same palette.", DT_LEFT | DT_TOP | DT_WORDBREAK);
    DrawLabel(dc, state->app->host.section_font(), state->app->theme.text, SliceTop(bi, 26), L"Live Progress", DT_LEFT | DT_TOP | DT_SINGLELINE);
    DrawLabel(dc, state->app->host.body_font(), state->app->theme.mutedText, RECT{bi.left, bi.top + 32, bi.right, bi.top + 50}, L"Background sync", DT_LEFT | DT_TOP | DT_SINGLELINE);
    DrawLabel(dc, state->app->host.body_font(), state->app->theme.mutedText, RECT{bi.left, bi.top + 104, bi.right, bi.top + 122}, L"Render capacity", DT_LEFT | DT_TOP | DT_SINGLELINE);
}

void PaintControlsPage(HWND page, HDC dc, PageState* state) {
    RECT client{}; GetClientRect(page, &client); FillRect(dc, &client, state->app->host.background_brush());
    RECT content = InsetRectCopy(client, 28, 28);
    RECT top = SliceTop(content, 260);
    RECT bottom{content.left, top.bottom + 20, content.right, content.bottom};
    RECT left = SliceLeft(bottom, ((bottom.right - bottom.left) - 20) / 2);
    RECT right{left.right + 20, bottom.top, bottom.right, bottom.bottom};
    FillRoundedRect(dc, top, 24, state->app->theme.panel, state->app->theme.border);
    FillRoundedRect(dc, left, 24, state->app->theme.panel, state->app->theme.border);
    FillRoundedRect(dc, right, 24, state->app->theme.panel, state->app->theme.border);
    RECT ti = InsetRectCopy(top, 24, 24);
    RECT li = InsetRectCopy(left, 24, 24);
    RECT ri = InsetRectCopy(right, 24, 24);
    DrawLabel(dc, state->app->host.section_font(), state->app->theme.text, SliceTop(ti, 26), L"Sliders And Motion", DT_LEFT | DT_TOP | DT_SINGLELINE);
    DrawLabel(dc, state->app->host.body_font(), state->app->theme.mutedText, RECT{ti.left, ti.top + 34, ti.right, ti.top + 52}, L"Exposure", DT_LEFT | DT_TOP | DT_SINGLELINE);
    DrawLabel(dc, state->app->host.body_font(), state->app->theme.mutedText, RECT{ti.left, ti.top + 128, ti.right, ti.top + 146}, L"Balance", DT_LEFT | DT_TOP | DT_SINGLELINE);
    DrawLabel(dc, state->app->host.section_font(), state->app->theme.text, SliceTop(li, 26), L"Custom Scrollbars", DT_LEFT | DT_TOP | DT_SINGLELINE);
    DrawLabel(dc, state->app->host.body_font(), state->app->theme.mutedText, RECT{li.left, li.top + 34, li.right, li.top + 56}, L"Horizontal timeline and vertical navigation rail.", DT_LEFT | DT_TOP | DT_WORDBREAK);
    DrawLabel(dc, state->app->host.section_font(), state->app->theme.text, SliceTop(ri, 26), L"Dark Edit Surfaces", DT_LEFT | DT_TOP | DT_SINGLELINE);
    DrawLabel(dc, state->app->host.body_font(), state->app->theme.mutedText, RECT{ri.left, ri.top + 34, ri.right, ri.top + 56}, L"Single-line search plus multiline notes.", DT_LEFT | DT_TOP | DT_WORDBREAK);
}

void PaintDataPage(HWND page, HDC dc, PageState* state) {
    RECT client{}; GetClientRect(page, &client); FillRect(dc, &client, state->app->host.background_brush());
    RECT content = InsetRectCopy(client, 28, 28);
    RECT bar = SliceTop(content, 42);
    RECT bottom{content.left, bar.bottom + 20, content.right, content.bottom};
    RECT left = SliceLeft(bottom, ((bottom.right - bottom.left) - 20) / 2);
    RECT right{left.right + 20, bottom.top, bottom.right, bottom.bottom};
    DrawLabel(dc, state->app->host.section_font(), state->app->theme.text, RECT{content.left, content.top - 4, content.right, content.top + 22}, L"Data Density", DT_LEFT | DT_TOP | DT_SINGLELINE);
    DrawLabel(dc, state->app->host.body_font(), state->app->theme.mutedText, RECT{content.left, content.top + 24, content.right, content.top + 46}, L"Custom table and list box using the same semantic palette and font system.", DT_LEFT | DT_TOP | DT_WORDBREAK);
    FillRoundedRect(dc, left, 24, state->app->theme.panel, state->app->theme.border);
    FillRoundedRect(dc, right, 24, state->app->theme.panel, state->app->theme.border);
}

void PaintExpandedPage(HWND page, HDC dc, PageState* state) {
    RECT client{}; GetClientRect(page, &client); FillRect(dc, &client, state->app->host.background_brush());
    RECT content = InsetRectCopy(client, 28, 28);
    RECT left = SliceLeft(content, ((content.right - content.left) - 20) / 2);
    RECT right{left.right + 20, content.top, content.right, content.bottom};
    RECT li = InsetRectCopy(left, 24, 24);
    RECT ri = InsetRectCopy(right, 24, 24);
    DrawLabel(dc, state->app->host.section_font(), state->app->theme.text, SliceTop(li, 26), L"New Controls", DT_LEFT | DT_TOP | DT_SINGLELINE);
    DrawLabel(dc, state->app->host.body_font(), state->app->theme.mutedText, RECT{li.left, li.top + 34, li.right, li.top + 56}, L"Static, CheckBox, RadioButton, and a semantic-state summary.", DT_LEFT | DT_TOP | DT_WORDBREAK);
    DrawLabel(dc, state->app->host.section_font(), state->app->theme.text, SliceTop(ri, 26), L"Dialog Trigger", DT_LEFT | DT_TOP | DT_SINGLELINE);
    DrawLabel(dc, state->app->host.body_font(), state->app->theme.mutedText, RECT{ri.left, ri.top + 34, ri.right, ri.top + 56}, L"Open the custom dark dialog without leaving the showcase.", DT_LEFT | DT_TOP | DT_WORDBREAK);
}

void PaintPage(HWND page, HDC dc, PageState* state) {
    switch (state->kind) {
    case PageKind::Overview: PaintOverviewPage(page, dc, state); break;
    case PageKind::Controls: PaintControlsPage(page, dc, state); break;
    case PageKind::Data: PaintDataPage(page, dc, state); break;
    case PageKind::Expanded: PaintExpandedPage(page, dc, state); break;
    }
}

bool CreatePageWindow(HWND parent, int controlId, PageKind kind, AppState* app, HWND* outPage) {
    auto* pageState = new PageState();
    pageState->app = app;
    pageState->kind = kind;
    HWND page = CreateWindowExW(0, kPageClassName, L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0, 0, 0, 0, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)), reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(parent, GWLP_HINSTANCE)), pageState);
    if (!page) { delete pageState; return false; }
    *outPage = page;
    return true;
}

bool CreateOverviewControls(AppState* app) {
    darkui::Panel::Options panelOptions;
    panelOptions.role = darkui::SurfaceRole::Panel;
    panelOptions.cornerRadius = 24;
    darkui::ComboBox::Options profileOptions;
    profileOptions.items = {{L"Studio Executive", 1, true}, {L"Asset Review", 2, false}, {L"Broadcast Board", 3, false}};
    profileOptions.selection = 0;
    profileOptions.variant = darkui::FieldVariant::Panel;
    darkui::Button::Options primaryOptions;
    primaryOptions.text = L"Sync Cluster";
    primaryOptions.variant = darkui::ButtonVariant::Primary;
    darkui::Button::Options secondaryOptions = primaryOptions;
    secondaryOptions.text = L"Share Preview";
    secondaryOptions.variant = darkui::ButtonVariant::Secondary;
    darkui::Static::Options statusOptions;
    statusOptions.text = L"Semantic palette active across every page";
    statusOptions.variant = darkui::StaticVariant::Muted;
    darkui::ProgressBar::Options syncOptions;
    syncOptions.minimum = 0; syncOptions.maximum = 100; syncOptions.value = 74;
    syncOptions.variant = darkui::ProgressVariant::Emphasis;
    darkui::ProgressBar::Options cpuOptions = syncOptions;
    cpuOptions.value = 61;
    cpuOptions.showPercentage = false;

    if (!app->overview.leftCard.Create(app->overviewPage, ID_OVERVIEW_PANEL_LEFT, app->theme, panelOptions)) return false;
    if (!app->overview.rightCard.Create(app->overviewPage, ID_OVERVIEW_PANEL_RIGHT, app->theme, panelOptions)) return false;
    if (!app->overview.bottomCard.Create(app->overviewPage, ID_OVERVIEW_PANEL_BOTTOM, app->theme, panelOptions)) return false;
    if (!app->overview.profile.Create(app->overview.leftCard.hwnd(), ID_OVERVIEW_PROFILE, app->theme, profileOptions)) return false;
    if (!app->overview.primary.Create(app->overview.rightCard.hwnd(), ID_OVERVIEW_PRIMARY, app->theme, primaryOptions)) return false;
    if (!app->overview.secondary.Create(app->overview.rightCard.hwnd(), ID_OVERVIEW_SECONDARY, app->theme, secondaryOptions)) return false;
    if (!app->overview.status.Create(app->overview.rightCard.hwnd(), ID_OVERVIEW_STATUS, app->theme, statusOptions)) return false;
    if (!app->overview.sync.Create(app->overview.bottomCard.hwnd(), ID_OVERVIEW_SYNC, app->theme, syncOptions)) return false;
    if (!app->overview.cpu.Create(app->overview.bottomCard.hwnd(), ID_OVERVIEW_CPU, app->theme, cpuOptions)) return false;
    return true;
}

bool CreateControlsPanel(AppState* app) {
    darkui::Slider::Options exposureOptions;
    exposureOptions.minimum = 0; exposureOptions.maximum = 100; exposureOptions.value = 68; exposureOptions.showTicks = true; exposureOptions.tickCount = 9;
    exposureOptions.variant = darkui::SliderVariant::Emphasis;
    darkui::Slider::Options balanceOptions = exposureOptions;
    balanceOptions.value = 42; balanceOptions.tickCount = 7;
    balanceOptions.variant = darkui::SliderVariant::Dense;
    darkui::ScrollBar::Options timelineOptions;
    timelineOptions.vertical = false; timelineOptions.minimum = 0; timelineOptions.maximum = 100; timelineOptions.pageSize = 20; timelineOptions.value = 34;
    darkui::ScrollBar::Options navigatorOptions;
    navigatorOptions.vertical = true; navigatorOptions.minimum = 0; navigatorOptions.maximum = 100; navigatorOptions.pageSize = 18; navigatorOptions.value = 26;
    darkui::Edit::Options searchOptions;
    searchOptions.cueBanner = L"Search sessions";
    searchOptions.variant = darkui::FieldVariant::Default;
    darkui::Edit::Options notesOptions;
    notesOptions.text = L"Dark multiline edit\nwith the custom dark vertical scrollbar\nlinked to the shared theme.";
    notesOptions.cueBanner = L"Write notes here";
    notesOptions.variant = darkui::FieldVariant::Panel;
    notesOptions.style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL;

    if (!app->controls.exposure.Create(app->controlsPage, ID_CONTROLS_EXPOSURE, app->theme, exposureOptions)) return false;
    if (!app->controls.balance.Create(app->controlsPage, ID_CONTROLS_BALANCE, app->theme, balanceOptions)) return false;
    if (!app->controls.timeline.Create(app->controlsPage, ID_CONTROLS_SCROLL_H, app->theme, timelineOptions)) return false;
    if (!app->controls.navigator.Create(app->controlsPage, ID_CONTROLS_SCROLL_V, app->theme, navigatorOptions)) return false;
    if (!app->controls.search.Create(app->controlsPage, ID_CONTROLS_SEARCH, app->theme, searchOptions)) return false;
    if (!app->controls.notes.Create(app->controlsPage, ID_CONTROLS_NOTES, app->theme, notesOptions)) return false;
    return true;
}

bool CreateDataPanel(AppState* app) {
    darkui::ComboBox::Options filterOptions;
    filterOptions.items = {{L"All transfers", 0, true}, {L"Ready only", 1, false}, {L"Cloud only", 2, false}};
    filterOptions.selection = 0;
    filterOptions.variant = darkui::FieldVariant::Default;
    darkui::Button::Options refreshOptions;
    refreshOptions.text = L"Refresh";
    refreshOptions.variant = darkui::ButtonVariant::Secondary;
    darkui::Table::Options tableOptions;
    tableOptions.columns = {{L"Collection", 220, LVCFMT_LEFT}, {L"Source", 100, LVCFMT_LEFT}, {L"State", 110, LVCFMT_LEFT}, {L"Size", 100, LVCFMT_RIGHT}, {L"Updated", 110, LVCFMT_LEFT}};
    tableOptions.drawEmptyGrid = true;
    darkui::ListBox::Options queueOptions;
    queueOptions.variant = darkui::FieldVariant::Panel;
    queueOptions.items = {{L"Queued color pass", 1}, {L"Archive sync", 2}, {L"Proxy rebuild", 3}, {L"Review package", 4}, {L"Audio conform", 5}};
    queueOptions.selection = 0;

    if (!app->data.filter.Create(app->dataPage, ID_DATA_FILTER, app->theme, filterOptions)) return false;
    if (!app->data.refresh.Create(app->dataPage, ID_DATA_REFRESH, app->theme, refreshOptions)) return false;
    if (!app->data.table.Create(app->dataPage, ID_DATA_TABLE, app->theme, tableOptions)) return false;
    if (!app->data.queue.Create(app->dataPage, ID_DATA_QUEUE, app->theme, queueOptions)) return false;
    RefreshDataRows(app);
    return true;
}

bool CreateExpandedPanel(AppState* app) {
    darkui::Panel::Options panelOptions;
    panelOptions.role = darkui::SurfaceRole::Panel;
    panelOptions.cornerRadius = 24;
    darkui::Static::Options titleOptions;
    titleOptions.text = L"Extended Components";
    titleOptions.variant = darkui::StaticVariant::PanelTitle;
    darkui::Static::Options helperOptions;
    helperOptions.text = L"These controls were added after the original showcase and now participate in the same semantic theme system.";
    helperOptions.variant = darkui::StaticVariant::PanelBody;
    darkui::CheckBox::Options autoSaveOptions;
    autoSaveOptions.text = L"Enable automatic recovery snapshots";
    autoSaveOptions.checked = true;
    autoSaveOptions.variant = darkui::SelectionVariant::Accent;
    darkui::CheckBox::Options compactOptions;
    compactOptions.text = L"Compact side rails";
    compactOptions.variant = darkui::SelectionVariant::Panel;
    darkui::RadioButton::Options gridOptions;
    gridOptions.text = L"Grid layout";
    gridOptions.checked = true;
    gridOptions.style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_GROUP;
    gridOptions.variant = darkui::SelectionVariant::Accent;
    darkui::RadioButton::Options focusOptions;
    focusOptions.text = L"Focus layout";
    focusOptions.variant = darkui::SelectionVariant::Panel;
    darkui::RadioButton::Options flowOptions;
    flowOptions.text = L"Flow layout";
    flowOptions.variant = darkui::SelectionVariant::Panel;
    darkui::Button::Options dialogButtonOptions;
    dialogButtonOptions.text = L"Open Dialog";
    dialogButtonOptions.variant = darkui::ButtonVariant::Primary;
    darkui::Static::Options resultOptions;
    resultOptions.text = L"";
    resultOptions.variant = darkui::StaticVariant::PanelBody;

    if (!app->expanded.leftCard.Create(app->expandedPage, ID_EXPANDED_PANEL_LEFT, app->theme, panelOptions)) return false;
    if (!app->expanded.rightCard.Create(app->expandedPage, ID_EXPANDED_PANEL_RIGHT, app->theme, panelOptions)) return false;
    if (!app->expanded.headline.Create(app->expanded.leftCard.hwnd(), ID_EXPANDED_STATIC_TITLE, app->theme, titleOptions)) return false;
    if (!app->expanded.helper.Create(app->expanded.leftCard.hwnd(), ID_EXPANDED_STATIC_HELPER, app->theme, helperOptions)) return false;
    if (!app->expanded.autoSave.Create(app->expanded.leftCard.hwnd(), ID_EXPANDED_CHECK_AUTOSAVE, app->theme, autoSaveOptions)) return false;
    if (!app->expanded.compact.Create(app->expanded.leftCard.hwnd(), ID_EXPANDED_CHECK_COMPACT, app->theme, compactOptions)) return false;
    if (!app->expanded.grid.Create(app->expanded.leftCard.hwnd(), ID_EXPANDED_RADIO_GRID, app->theme, gridOptions)) return false;
    if (!app->expanded.focus.Create(app->expanded.leftCard.hwnd(), ID_EXPANDED_RADIO_FOCUS, app->theme, focusOptions)) return false;
    if (!app->expanded.flow.Create(app->expanded.leftCard.hwnd(), ID_EXPANDED_RADIO_FLOW, app->theme, flowOptions)) return false;
    if (!app->expanded.dialogButton.Create(app->expanded.rightCard.hwnd(), ID_EXPANDED_DIALOG, app->theme, dialogButtonOptions)) return false;
    if (!app->expanded.result.Create(app->expanded.rightCard.hwnd(), ID_EXPANDED_RESULT, app->theme, resultOptions)) return false;
    UpdateExpandedSummary(app);
    return true;
}

bool CreateShowcase(AppState* state, HWND window) {
    darkui::Button::Options graphiteOptions;
    graphiteOptions.text = L"Graphite"; graphiteOptions.variant = darkui::ButtonVariant::Subtle;
    darkui::Button::Options emberOptions = graphiteOptions; emberOptions.text = L"Ember";
    darkui::Button::Options glacierOptions = graphiteOptions; glacierOptions.text = L"Glacier";
    darkui::Button::Options mossOptions = graphiteOptions; mossOptions.text = L"Moss";
    darkui::Button::Options monoOptions = graphiteOptions; monoOptions.text = L"Mono";
    darkui::Tab::Options tabOptions;
    tabOptions.items = {{L"Overview", 1}, {L"Controls", 2}, {L"Data", 3}, {L"Expanded", 4}};
    tabOptions.selection = 0;
    tabOptions.variant = darkui::TabVariant::Accent;

    if (!state->themeGraphite.Create(window, ID_THEME_GRAPHITE, state->theme, graphiteOptions)) return false;
    if (!state->themeEmber.Create(window, ID_THEME_EMBER, state->theme, emberOptions)) return false;
    if (!state->themeGlacier.Create(window, ID_THEME_GLACIER, state->theme, glacierOptions)) return false;
    if (!state->themeMoss.Create(window, ID_THEME_MOSS, state->theme, mossOptions)) return false;
    if (!state->themeMono.Create(window, ID_THEME_MONO, state->theme, monoOptions)) return false;
    if (!state->tab.Create(window, ID_MAIN_TAB, state->theme, tabOptions)) return false;

    if (!CreatePageWindow(state->tab.hwnd(), ID_PAGE_OVERVIEW, PageKind::Overview, state, &state->overviewPage)) return false;
    if (!CreatePageWindow(state->tab.hwnd(), ID_PAGE_CONTROLS, PageKind::Controls, state, &state->controlsPage)) return false;
    if (!CreatePageWindow(state->tab.hwnd(), ID_PAGE_DATA, PageKind::Data, state, &state->dataPage)) return false;
    if (!CreatePageWindow(state->tab.hwnd(), ID_PAGE_EXPANDED, PageKind::Expanded, state, &state->expandedPage)) return false;

    state->tab.AttachPage(0, state->overviewPage);
    state->tab.AttachPage(1, state->controlsPage);
    state->tab.AttachPage(2, state->dataPage);
    state->tab.AttachPage(3, state->expandedPage);
    if (!CreateOverviewControls(state)) return false;
    if (!CreateControlsPanel(state)) return false;
    if (!CreateDataPanel(state)) return false;
    if (!CreateExpandedPanel(state)) return false;

    state->themeManager.Bind(state->tab,
                             state->themeGraphite,
                             state->themeEmber,
                             state->themeGlacier,
                             state->themeMoss,
                             state->themeMono,
                             state->overview.leftCard,
                             state->overview.rightCard,
                             state->overview.bottomCard,
                             state->overview.profile,
                             state->overview.primary,
                             state->overview.secondary,
                             state->overview.status,
                             state->overview.sync,
                             state->overview.cpu,
                             state->controls.exposure,
                             state->controls.balance,
                             state->controls.timeline,
                             state->controls.navigator,
                             state->controls.search,
                             state->controls.notes,
                             state->data.filter,
                             state->data.refresh,
                             state->data.table,
                             state->data.queue,
                             state->expanded.leftCard,
                             state->expanded.rightCard,
                             state->expanded.headline,
                             state->expanded.helper,
                             state->expanded.autoSave,
                             state->expanded.compact,
                             state->expanded.grid,
                             state->expanded.focus,
                             state->expanded.flow,
                             state->expanded.dialogButton,
                             state->expanded.result);

    ApplyThemeToControls(state);
    LayoutMainWindow(window, state);
    return true;
}

LRESULT CALLBACK ShowcasePageProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<PageState*>(GetWindowLongPtrW(window, GWLP_USERDATA));

    switch (message) {
    case WM_CREATE: {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* created = reinterpret_cast<PageState*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(created));
        return 0;
    }
    case WM_SIZE:
        if (state && state->app) LayoutPage(window, state->app, state->kind);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        if (state && state->app) {
            PAINTSTRUCT ps{};
            HDC dc = BeginPaint(window, &ps);
            PaintPage(window, dc, state);
            EndPaint(window, &ps);
            return 0;
        }
        break;
    case WM_COMMAND:
        if (state && state->app && state->app->tab.parent()) {
            return SendMessageW(state->app->tab.parent(), WM_COMMAND, wParam, lParam);
        }
        break;
    case WM_NOTIFY:
        if (state && state->app && state->app->tab.parent()) {
            return SendMessageW(state->app->tab.parent(), WM_NOTIFY, wParam, lParam);
        }
        break;
    case WM_NCDESTROY:
        delete state;
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        return 0;
    default:
        break;
    }

    return DefWindowProcW(window, message, wParam, lParam);
}

LRESULT CALLBACK ShowcaseWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<AppState*>(GetWindowLongPtrW(window, GWLP_USERDATA));

    switch (message) {
    case WM_CREATE: {
        auto* created = new AppState();
        created->theme = MakeThemeByIndex(0);
        created->themeIndex = 0;
        darkui::ThemedWindowHost::Options hostOptions;
        hostOptions.theme = created->theme;
        hostOptions.titleBarStyle = darkui::TitleBarStyle::Black;
        if (!created->host.Attach(window, hostOptions)) {
            CleanupAppState(created);
            return -1;
        }
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(created));
        if (!CreateShowcase(created, window)) {
            CleanupAppState(created);
            SetWindowLongPtrW(window, GWLP_USERDATA, 0);
            return -1;
        }
        return 0;
    }
    case WM_SIZE:
        if (state) LayoutMainWindow(window, state);
        return 0;
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        if (info) {
            info->ptMinTrackSize.x = 1180;
            info->ptMinTrackSize.y = 760;
            return 0;
        }
        break;
    }
    case WM_COMMAND:
        if (!state) break;
        if (HIWORD(wParam) == BN_CLICKED) {
            switch (LOWORD(wParam)) {
            case ID_THEME_GRAPHITE: ApplyTheme(state, window, 0); return 0;
            case ID_THEME_EMBER: ApplyTheme(state, window, 1); return 0;
            case ID_THEME_GLACIER: ApplyTheme(state, window, 2); return 0;
            case ID_THEME_MOSS: ApplyTheme(state, window, 3); return 0;
            case ID_THEME_MONO: ApplyTheme(state, window, 4); return 0;
            case ID_DATA_REFRESH: RefreshDataRows(state); return 0;
            case ID_EXPANDED_CHECK_AUTOSAVE:
            case ID_EXPANDED_CHECK_COMPACT:
            case ID_EXPANDED_RADIO_GRID:
            case ID_EXPANDED_RADIO_FOCUS:
            case ID_EXPANDED_RADIO_FLOW:
                UpdateExpandedSummary(state);
                return 0;
            case ID_EXPANDED_DIALOG:
                ShowExpandedDialog(window, state);
                return 0;
            default:
                break;
            }
        }
        if (HIWORD(wParam) == CBN_SELCHANGE && LOWORD(wParam) == ID_DATA_FILTER) {
            RefreshDataRows(state);
            return 0;
        }
        break;
    case WM_NOTIFY:
        if (state) {
            auto* hdr = reinterpret_cast<NMHDR*>(lParam);
            if (hdr && hdr->hwndFrom == state->tab.hwnd() && hdr->code == TCN_SELCHANGE) {
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
        }
        break;
    case WM_ERASEBKGND:
        if (state && state->host.HandleEraseBackground(reinterpret_cast<HDC>(wParam))) return 1;
        break;
    case WM_PAINT:
        if (state) {
            PAINTSTRUCT ps{};
            HDC dc = BeginPaint(window, &ps);
            PaintMainWindow(window, dc, state);
            EndPaint(window, &ps);
            return 0;
        }
        break;
    case WM_DESTROY:
        CleanupAppState(state);
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }

    return DefWindowProcW(window, message, wParam, lParam);
}

}  // namespace

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_STANDARD_CLASSES | ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES};
    InitCommonControlsEx(&icc);

    WNDCLASSEXW pageClass{};
    pageClass.cbSize = sizeof(pageClass);
    pageClass.lpfnWndProc = ShowcasePageProc;
    pageClass.hInstance = instance;
    pageClass.lpszClassName = kPageClassName;
    pageClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    pageClass.hbrBackground = nullptr;
    if (!RegisterClassExW(&pageClass)) return 0;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = ShowcaseWindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = kDemoClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    if (!RegisterClassExW(&wc)) return 0;

    HWND window = CreateWindowExW(0,
                                  kDemoClassName,
                                  kDemoTitle,
                                  WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                                  CW_USEDEFAULT,
                                  CW_USEDEFAULT,
                                  1320,
                                  860,
                                  nullptr,
                                  nullptr,
                                  instance,
                                  nullptr);
    if (!window) return 0;

    ShowWindow(window, showCommand);
    UpdateWindow(window);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
