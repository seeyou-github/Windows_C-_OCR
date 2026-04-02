#include <windows.h>
#include <commctrl.h>

#include <string>

#include "darkui/button.h"
#include "darkui/darkui.h"

namespace {

constexpr wchar_t kDemoClassName[] = L"DarkUiButtonDemoWindow";
constexpr wchar_t kDemoTitle[] = L"lib_darkui button demo";

enum ControlId {
    ID_BUTTON_PRIMARY = 3001,
    ID_BUTTON_ALT = 3002
};

struct DemoState {
    darkui::ThemedWindowHost host;
    darkui::Button primary;
    darkui::Button secondary;
    int clickCount = 0;
};

void CleanupState(DemoState* state) {
    if (!state) {
        return;
    }
    delete state;
}

darkui::Theme MakeTheme() {
    darkui::Theme theme;
    theme.background = RGB(18, 20, 24);
    theme.button = RGB(52, 61, 72);
    theme.buttonHot = RGB(72, 86, 104);
    theme.border = RGB(84, 96, 112);
    theme.text = RGB(232, 236, 241);
    theme.mutedText = RGB(144, 151, 161);
    theme.uiFont.family = L"Segoe UI";
    theme.uiFont.height = -20;
    return theme;
}

void Layout(HWND window, DemoState* state) {
    RECT rc{};
    GetClientRect(window, &rc);
    const int left = 32;
    MoveWindow(state->primary.hwnd(), left, 120, 220, 46, TRUE);
    MoveWindow(state->secondary.hwnd(), left, 182, 220, 46, TRUE);
}

void DrawTextLine(HDC dc, HFONT font, COLORREF color, RECT rect, const wchar_t* text) {
    HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(dc, font));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, text, -1, &rect, DT_LEFT | DT_TOP | DT_NOPREFIX | DT_SINGLELINE);
    if (oldFont) {
        SelectObject(dc, oldFont);
    }
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<DemoState*>(GetWindowLongPtrW(window, GWLP_USERDATA));

    switch (message) {
    case WM_CREATE: {
        auto* created = new DemoState();
        darkui::ThemedWindowHost::Options hostOptions;
        hostOptions.theme = MakeTheme();
        hostOptions.titleBarStyle = darkui::TitleBarStyle::Black;
        if (!created->host.Attach(window, hostOptions)) {
            CleanupState(created);
            return -1;
        }

        darkui::Button::Options primaryOptions;
        primaryOptions.text = L"Primary Action";
        primaryOptions.variant = darkui::ButtonVariant::Primary;
        darkui::Button::Options secondaryOptions;
        secondaryOptions.text = L"Secondary Action";
        secondaryOptions.variant = darkui::ButtonVariant::Danger;
        if (!created->primary.Create(window, ID_BUTTON_PRIMARY, created->host.theme(), primaryOptions) ||
            !created->secondary.Create(window, ID_BUTTON_ALT, created->host.theme(), secondaryOptions)) {
            CleanupState(created);
            return -1;
        }
        created->host.theme_manager().Bind(created->primary, created->secondary);
        created->host.theme_manager().Apply();
        EnableWindow(created->secondary.hwnd(), FALSE);

        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(created));
        Layout(window, created);
        return 0;
    }
    case WM_SIZE:
        if (state) {
            Layout(window, state);
        }
        return 0;
    case WM_COMMAND:
        if (!state || HIWORD(wParam) != BN_CLICKED) {
            break;
        }
        if (LOWORD(wParam) == ID_BUTTON_PRIMARY) {
            ++state->clickCount;
            std::wstring title = L"Primary clicked " + std::to_wstring(state->clickCount) + L" times";
            SetWindowTextW(window, title.c_str());
            return 0;
        }
        if (LOWORD(wParam) == ID_BUTTON_ALT) {
            state->primary.SetText(L"Text Updated");
            return 0;
        }
        break;
    case WM_ERASEBKGND:
        if (state && state->host.HandleEraseBackground(reinterpret_cast<HDC>(wParam))) {
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
            RECT noteRect{32, 248, client.right - 32, 320};

            DrawTextLine(dc, state->host.title_font(), state->host.theme().text, titleRect, L"Dark Button Demo");
            DrawTextLine(dc, state->host.body_font(), state->host.theme().mutedText, descRect, L"Owner-draw dark buttons with semantic variants, hover, press animation, and disabled state.");
            DrawTextW(dc,
                      L"Primary Action uses ButtonVariant::Primary.\nSecondary Action uses ButtonVariant::Danger and stays disabled to show the resolved variant palette.\nMove the mouse over the primary button to see hover and click motion.",
                      -1,
                      &noteRect,
                      DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);

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
                                  760,
                                  420,
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
