#include <windows.h>
#include <commctrl.h>

#include <string>

#include "darkui/darkui.h"

namespace {

constexpr wchar_t kDemoClassName[] = L"DarkUiScrollBarDemoWindow";
constexpr wchar_t kDemoTitle[] = L"lib_darkui scrollbar demo";

enum ControlId {
    ID_SCROLL_H = 6001,
    ID_SCROLL_V = 6002
};

struct DemoState {
    darkui::ThemedWindowHost host;
    darkui::ScrollBar horizontal;
    darkui::ScrollBar vertical;
    int horizontalValue = 24;
    int verticalValue = 56;
};

darkui::Theme MakeTheme() {
    darkui::Theme theme;
    theme.background = RGB(18, 20, 24);
    theme.text = RGB(232, 236, 241);
    theme.mutedText = RGB(144, 151, 161);
    theme.scrollBarBackground = RGB(24, 27, 31);
    theme.scrollBarTrack = RGB(48, 53, 60);
    theme.scrollBarThumb = RGB(120, 128, 140);
    theme.scrollBarThumbHot = RGB(160, 170, 184);
    theme.uiFont.family = L"Segoe UI";
    theme.uiFont.height = -20;
    return theme;
}

RECT GetInfoRect(HWND window) {
    RECT client{};
    GetClientRect(window, &client);
    return RECT{32, 328, client.right - 32, 378};
}

RECT GetDescriptionRect(HWND window) {
    RECT client{};
    GetClientRect(window, &client);
    return RECT{32, 64, client.right - 32, 116};
}

void Layout(HWND window, DemoState* state) {
    RECT client{};
    GetClientRect(window, &client);
    MoveWindow(state->horizontal.hwnd(), 32, 118, client.right - 120, state->host.theme().scrollBarThickness, TRUE);
    MoveWindow(state->vertical.hwnd(), client.right - 56, 118, state->host.theme().scrollBarThickness, 160, TRUE);
}

void CleanupState(DemoState* state) {
    if (!state) {
        return;
    }
    delete state;
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

        darkui::ScrollBar::Options horizontalOptions;
        horizontalOptions.vertical = false;
        horizontalOptions.minimum = 0;
        horizontalOptions.maximum = 100;
        horizontalOptions.pageSize = 20;
        horizontalOptions.value = created->horizontalValue;
        darkui::ScrollBar::Options verticalOptions;
        verticalOptions.vertical = true;
        verticalOptions.minimum = 0;
        verticalOptions.maximum = 100;
        verticalOptions.pageSize = 24;
        verticalOptions.value = created->verticalValue;
        if (!created->horizontal.Create(window, ID_SCROLL_H, created->host.theme(), horizontalOptions) ||
            !created->vertical.Create(window, ID_SCROLL_V, created->host.theme(), verticalOptions)) {
            CleanupState(created);
            return -1;
        }
        created->host.theme_manager().Bind(created->horizontal, created->vertical);

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
        if (state && reinterpret_cast<HWND>(lParam) == state->horizontal.hwnd()) {
            state->horizontalValue = state->horizontal.GetValue();
            const RECT infoRect = GetInfoRect(window);
            InvalidateRect(window, &infoRect, FALSE);
            return 0;
        }
        break;
    case WM_VSCROLL:
        if (state && reinterpret_cast<HWND>(lParam) == state->vertical.hwnd()) {
            state->verticalValue = state->vertical.GetValue();
            const RECT infoRect = GetInfoRect(window);
            InvalidateRect(window, &infoRect, FALSE);
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
            RECT descRect = GetDescriptionRect(window);
            RECT infoRect = GetInfoRect(window);

            HFONT oldTitle = reinterpret_cast<HFONT>(SelectObject(dc, state->host.title_font()));
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, state->host.theme().text);
            DrawTextW(dc, L"Dark ScrollBar Demo", -1, &titleRect, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
            SelectObject(dc, oldTitle);

            HFONT oldText = reinterpret_cast<HFONT>(SelectObject(dc, state->host.body_font()));
            SetTextColor(dc, state->host.theme().mutedText);
            DrawTextW(dc, L"Custom horizontal and vertical scrollbars with drag, page jump, keyboard, and standard scroll notifications.", -1, &descRect, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);

            std::wstring info = L"Horizontal: " + std::to_wstring(state->horizontalValue) +
                                L"    Vertical: " + std::to_wstring(state->verticalValue);
            SetTextColor(dc, state->host.theme().text);
            DrawTextW(dc, info.c_str(), -1, &infoRect, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
            SelectObject(dc, oldText);

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
                                  460,
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
