#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>

#include <algorithm>
#include <array>
#include <string>

#include "darkui/button.h"
#include "darkui/darkui.h"

namespace {

constexpr wchar_t kDemoClassName[] = L"DarkUiSliderDemoWindow";
constexpr wchar_t kDemoTitle[] = L"lib_darkui slider demo";

constexpr int kPanelX = 590;
constexpr int kPanelY = 110;
constexpr int kPanelWidth = 260;
constexpr int kPanelButtonWidth = 140;
constexpr int kPanelButtonHeight = 32;
constexpr int kPanelGap = 14;

enum ControlId {
    ID_SLIDER = 4001,
    ID_PICK_BACKGROUND = 4100,
    ID_PICK_TRACK = 4101,
    ID_PICK_FILL,
    ID_PICK_THUMB,
    ID_PICK_THUMB_HOT,
    ID_TRACK_MINUS,
    ID_TRACK_PLUS,
    ID_RADIUS_MINUS,
    ID_RADIUS_PLUS,
    ID_RESET_THEME
};

struct DemoState {
    darkui::ThemedWindowHost host;
    darkui::Theme theme;
    darkui::Theme defaultTheme;
    darkui::ThemeManager themeManager;
    darkui::Slider slider;
    std::array<COLORREF, 16> customColors{};
    darkui::Button buttonBackground;
    darkui::Button buttonTrack;
    darkui::Button buttonFill;
    darkui::Button buttonThumb;
    darkui::Button buttonThumbHot;
    darkui::Button buttonTrackMinus;
    darkui::Button buttonTrackPlus;
    darkui::Button buttonRadiusMinus;
    darkui::Button buttonRadiusPlus;
    darkui::Button buttonReset;
};

void CleanupState(DemoState* state);

darkui::Theme MakeTheme() {
    darkui::Theme theme;
    theme.background = RGB(18, 20, 24);
    theme.text = RGB(232, 236, 241);
    theme.mutedText = RGB(144, 151, 161);
    theme.border = RGB(70, 78, 90);
    theme.button = RGB(52, 57, 66);
    theme.buttonHover = RGB(62, 68, 79);
    theme.buttonHot = RGB(74, 82, 95);
    theme.sliderBackground = RGB(28, 31, 36);
    theme.sliderTrack = RGB(52, 56, 62);
    theme.sliderFill = RGB(78, 120, 184);
    theme.sliderThumb = RGB(224, 227, 232);
    theme.sliderThumbHot = RGB(245, 247, 250);
    theme.sliderTick = RGB(102, 110, 122);
    theme.uiFont.family = L"Segoe UI";
    theme.uiFont.height = -20;
    theme.sliderTrackHeight = 6;
    theme.sliderThumbRadius = 10;
    return theme;
}

void CleanupState(DemoState* state) {
    if (!state) {
        return;
    }
    delete state;
}

void Layout(HWND window, DemoState* state) {
    RECT rc{};
    GetClientRect(window, &rc);
    MoveWindow(state->slider.hwnd(), 32, 118, 520, 84, TRUE);

    int y = kPanelY + 34;
    MoveWindow(state->buttonBackground.hwnd(), kPanelX + 88, y, kPanelButtonWidth, kPanelButtonHeight, TRUE);
    y += kPanelButtonHeight + kPanelGap;
    MoveWindow(state->buttonTrack.hwnd(), kPanelX + 88, y, kPanelButtonWidth, kPanelButtonHeight, TRUE);
    y += kPanelButtonHeight + kPanelGap;
    MoveWindow(state->buttonFill.hwnd(), kPanelX + 88, y, kPanelButtonWidth, kPanelButtonHeight, TRUE);
    y += kPanelButtonHeight + kPanelGap;
    MoveWindow(state->buttonThumb.hwnd(), kPanelX + 88, y, kPanelButtonWidth, kPanelButtonHeight, TRUE);
    y += kPanelButtonHeight + kPanelGap;
    MoveWindow(state->buttonThumbHot.hwnd(), kPanelX + 88, y, kPanelButtonWidth, kPanelButtonHeight, TRUE);
    y += kPanelButtonHeight + 18;
    MoveWindow(state->buttonTrackMinus.hwnd(), kPanelX + 88, y, 44, kPanelButtonHeight, TRUE);
    MoveWindow(state->buttonTrackPlus.hwnd(), kPanelX + 138, y, 44, kPanelButtonHeight, TRUE);
    y += kPanelButtonHeight + kPanelGap;
    MoveWindow(state->buttonRadiusMinus.hwnd(), kPanelX + 88, y, 44, kPanelButtonHeight, TRUE);
    MoveWindow(state->buttonRadiusPlus.hwnd(), kPanelX + 138, y, 44, kPanelButtonHeight, TRUE);
    y += kPanelButtonHeight + 20;
    MoveWindow(state->buttonReset.hwnd(), kPanelX, y, kPanelWidth, 38, TRUE);
}

RECT GetValueRect(HWND window) {
    RECT client{};
    GetClientRect(window, &client);
    return RECT{32, 214, client.right - 32, 242};
}

void ApplySliderTheme(HWND window, DemoState* state, bool repaintAll = true) {
    state->host.ApplyTheme(state->theme);
    state->themeManager.SetTheme(state->theme);
    state->themeManager.Apply();
    if (repaintAll) {
        InvalidateRect(window, nullptr, TRUE);
    } else {
        const RECT valueRect = GetValueRect(window);
        InvalidateRect(window, &valueRect, TRUE);
    }
}

bool PickColor(HWND window, DemoState* state, COLORREF* color) {
    CHOOSECOLORW cc{};
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = window;
    cc.rgbResult = *color;
    cc.lpCustColors = state->customColors.data();
    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
    if (!ChooseColorW(&cc)) {
        return false;
    }
    *color = cc.rgbResult;
    return true;
}

void DrawTextLine(HDC dc, HFONT font, COLORREF color, RECT rect, const wchar_t* text, UINT format = DT_LEFT) {
    HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(dc, font));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, text, -1, &rect, format | DT_TOP | DT_NOPREFIX | DT_SINGLELINE);
    if (oldFont) {
        SelectObject(dc, oldFont);
    }
}

void DrawColorSwatch(HDC dc, int x, int y, COLORREF color, COLORREF border) {
    RECT swatch{x, y, x + 18, y + 18};
    HBRUSH fill = CreateSolidBrush(color);
    HBRUSH oldBrush = reinterpret_cast<HBRUSH>(SelectObject(dc, fill));
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HPEN oldPen = reinterpret_cast<HPEN>(SelectObject(dc, pen));
    Rectangle(dc, swatch.left, swatch.top, swatch.right, swatch.bottom);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(pen);
    DeleteObject(fill);
}

void DrawStylePanel(HDC dc, DemoState* state) {
    RECT title{kPanelX, kPanelY, kPanelX + kPanelWidth, kPanelY + 22};
    DrawTextLine(dc, state->host.body_font(), state->theme.text, title, L"Live Style Controls");

    struct ColorRow {
        const wchar_t* label;
        COLORREF color;
    };
    const ColorRow rows[] = {
        {L"Area", state->theme.sliderBackground},
        {L"Track", state->theme.sliderTrack},
        {L"Fill", state->theme.sliderFill},
        {L"Thumb", state->theme.sliderThumb},
        {L"Thumb Hot", state->theme.sliderThumbHot},
    };

    int y = kPanelY + 40;
    for (const auto& row : rows) {
        RECT label{kPanelX, y + 6, kPanelX + 96, y + 28};
        DrawTextLine(dc, state->host.body_font(), state->theme.mutedText, label, row.label);
        DrawColorSwatch(dc, kPanelX + 238, y + 6, row.color, state->theme.border);
        y += kPanelButtonHeight + kPanelGap;
    }

    RECT trackLabel{kPanelX, y + 6, kPanelX + 96, y + 28};
    DrawTextLine(dc, state->host.body_font(), state->theme.mutedText, trackLabel, L"Track H");
    RECT trackValue{kPanelX + 186, y + 6, kPanelX + 234, y + 28};
    std::wstring trackText = std::to_wstring(state->theme.sliderTrackHeight);
    DrawTextLine(dc, state->host.body_font(), state->theme.text, trackValue, trackText.c_str(), DT_RIGHT);
    y += kPanelButtonHeight + kPanelGap;

    RECT radiusLabel{kPanelX, y + 6, kPanelX + 96, y + 28};
    DrawTextLine(dc, state->host.body_font(), state->theme.mutedText, radiusLabel, L"Thumb R");
    RECT radiusValue{kPanelX + 186, y + 6, kPanelX + 234, y + 28};
    std::wstring radiusText = std::to_wstring(state->theme.sliderThumbRadius);
    DrawTextLine(dc, state->host.body_font(), state->theme.text, radiusValue, radiusText.c_str(), DT_RIGHT);
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<DemoState*>(GetWindowLongPtrW(window, GWLP_USERDATA));

    switch (message) {
    case WM_CREATE: {
        auto* created = new DemoState();
        created->defaultTheme = MakeTheme();
        created->theme = created->defaultTheme;
        darkui::ThemedWindowHost::Options hostOptions;
        hostOptions.theme = created->theme;
        hostOptions.titleBarStyle = darkui::TitleBarStyle::Black;
        if (!created->host.Attach(window, hostOptions)) {
            CleanupState(created);
            return -1;
        }

        darkui::Slider::Options sliderOptions;
        sliderOptions.minimum = 0;
        sliderOptions.maximum = 100;
        sliderOptions.value = 38;
        sliderOptions.showTicks = true;
        sliderOptions.tickCount = 11;
        sliderOptions.variant = darkui::SliderVariant::Emphasis;
        created->slider.Create(window, ID_SLIDER, created->theme, sliderOptions);

        darkui::Button::Options pickButtonOptions;
        pickButtonOptions.text = L"Pick Color";
        pickButtonOptions.variant = darkui::ButtonVariant::Secondary;
        darkui::Button::Options stepButtonOptions;
        stepButtonOptions.variant = darkui::ButtonVariant::Ghost;
        darkui::Button::Options resetButtonOptions;
        resetButtonOptions.text = L"Reset Style";
        resetButtonOptions.variant = darkui::ButtonVariant::Primary;

        created->buttonBackground.Create(window, ID_PICK_BACKGROUND, created->theme, pickButtonOptions);
        created->buttonTrack.Create(window, ID_PICK_TRACK, created->theme, pickButtonOptions);
        created->buttonFill.Create(window, ID_PICK_FILL, created->theme, pickButtonOptions);
        created->buttonThumb.Create(window, ID_PICK_THUMB, created->theme, pickButtonOptions);
        created->buttonThumbHot.Create(window, ID_PICK_THUMB_HOT, created->theme, pickButtonOptions);

        darkui::Button::Options minusOptions = stepButtonOptions;
        minusOptions.text = L"-";
        darkui::Button::Options plusOptions = stepButtonOptions;
        plusOptions.text = L"+";
        created->buttonTrackMinus.Create(window, ID_TRACK_MINUS, created->theme, minusOptions);
        created->buttonTrackPlus.Create(window, ID_TRACK_PLUS, created->theme, plusOptions);
        created->buttonRadiusMinus.Create(window, ID_RADIUS_MINUS, created->theme, minusOptions);
        created->buttonRadiusPlus.Create(window, ID_RADIUS_PLUS, created->theme, plusOptions);
        created->buttonReset.Create(window, ID_RESET_THEME, created->theme, resetButtonOptions);

        created->themeManager.Bind(created->slider,
                                   created->buttonBackground,
                                   created->buttonTrack,
                                   created->buttonFill,
                                   created->buttonThumb,
                                   created->buttonThumbHot,
                                   created->buttonTrackMinus,
                                   created->buttonTrackPlus,
                                   created->buttonRadiusMinus,
                                   created->buttonRadiusPlus,
                                   created->buttonReset);

        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(created));
        Layout(window, created);
        return 0;
    }
    case WM_SIZE:
        if (state) {
            Layout(window, state);
        }
        return 0;
    case WM_HSCROLL:
        if (state && reinterpret_cast<HWND>(lParam) == state->slider.hwnd()) {
            std::wstring title = L"Slider Value: " + std::to_wstring(state->slider.GetValue());
            SetWindowTextW(window, title.c_str());
            const RECT valueRect = GetValueRect(window);
            InvalidateRect(window, &valueRect, TRUE);
            return 0;
        }
        break;
    case WM_COMMAND:
        if (!state || HIWORD(wParam) != BN_CLICKED) {
            break;
        }
        switch (LOWORD(wParam)) {
        case ID_PICK_BACKGROUND:
            if (PickColor(window, state, &state->theme.sliderBackground)) {
                ApplySliderTheme(window, state);
            }
            return 0;
        case ID_PICK_TRACK:
            if (PickColor(window, state, &state->theme.sliderTrack)) {
                ApplySliderTheme(window, state);
            }
            return 0;
        case ID_PICK_FILL:
            if (PickColor(window, state, &state->theme.sliderFill)) {
                ApplySliderTheme(window, state);
            }
            return 0;
        case ID_PICK_THUMB:
            if (PickColor(window, state, &state->theme.sliderThumb)) {
                ApplySliderTheme(window, state);
            }
            return 0;
        case ID_PICK_THUMB_HOT:
            if (PickColor(window, state, &state->theme.sliderThumbHot)) {
                ApplySliderTheme(window, state);
            }
            return 0;
        case ID_TRACK_MINUS:
            state->theme.sliderTrackHeight = std::max(2, state->theme.sliderTrackHeight - 1);
            ApplySliderTheme(window, state);
            return 0;
        case ID_TRACK_PLUS:
            state->theme.sliderTrackHeight = std::min(20, state->theme.sliderTrackHeight + 1);
            ApplySliderTheme(window, state);
            return 0;
        case ID_RADIUS_MINUS:
            state->theme.sliderThumbRadius = std::max(4, state->theme.sliderThumbRadius - 1);
            ApplySliderTheme(window, state);
            return 0;
        case ID_RADIUS_PLUS:
            state->theme.sliderThumbRadius = std::min(24, state->theme.sliderThumbRadius + 1);
            ApplySliderTheme(window, state);
            return 0;
        case ID_RESET_THEME:
            state->theme = state->defaultTheme;
            ApplySliderTheme(window, state);
            return 0;
        default:
            break;
        }
        break;
    case WM_ERASEBKGND:
        if (state) {
            RECT rect{};
            GetClientRect(window, &rect);
            FillRect(reinterpret_cast<HDC>(wParam), &rect, state->host.background_brush());
            return 1;
        }
        break;
    case WM_PAINT:
        if (state) {
            PAINTSTRUCT ps{};
            HDC dc = BeginPaint(window, &ps);

            RECT client{};
            GetClientRect(window, &client);
            state->host.FillBackground(dc);

            RECT titleRect{32, 24, client.right - 32, 56};
            RECT descRect{32, 64, client.right - 32, 92};
            RECT valueRect{32, 214, client.right - 32, 242};
            RECT noteRect{32, 256, 552, 360};

            DrawTextLine(dc, state->host.title_font(), state->theme.text, titleRect, L"Dark Slider Demo");
            DrawTextLine(dc, state->host.body_font(), state->theme.mutedText, descRect, L"Drag the thumb, click the track, or use keyboard arrows after focus.");

            std::wstring valueText = L"Current value: " + std::to_wstring(state->slider.GetValue()) + L" / 100";
            DrawTextLine(dc, state->host.body_font(), state->theme.text, valueRect, valueText.c_str());
            DrawTextW(dc,
                      L"Notifications: the slider sends WM_HSCROLL to the parent window.\nThis demo enables tick marks on the custom slider.",
                      -1,
                      &noteRect,
                      DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
            DrawStylePanel(dc, state);

            EndPaint(window, &ps);
            return 0;
        }
        break;
    case WM_DESTROY:
        CleanupState(state);
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
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = kDemoClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    if (!RegisterClassExW(&wc)) {
        return 0;
    }

    HWND window = CreateWindowExW(0,
                                  kDemoClassName,
                                  kDemoTitle,
                                  WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                                  CW_USEDEFAULT,
                                  CW_USEDEFAULT,
                                  940,
                                  520,
                                  nullptr,
                                  nullptr,
                                  instance,
                                  nullptr);
    if (!window) {
        return 0;
    }

    ShowWindow(window, showCommand);
    UpdateWindow(window);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
