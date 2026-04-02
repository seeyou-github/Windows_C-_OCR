#include <windows.h>
#include <commctrl.h>

#include <string>

#include "darkui/darkui.h"

namespace {

constexpr wchar_t kDemoClassName[] = L"DarkUiTabDemoWindow";
constexpr wchar_t kDemoTitle[] = L"lib_darkui tab demo";

enum ControlId {
    ID_TAB = 7001,
    ID_PAGE_ONE = 7101,
    ID_PAGE_TWO,
    ID_PAGE_THREE
};

struct DemoState {
    darkui::ThemedWindowHost host;
    darkui::Tab tab;
    HWND pageOne = nullptr;
    HWND pageTwo = nullptr;
    HWND pageThree = nullptr;
};

darkui::Theme MakeTheme() {
    darkui::Theme theme;
    theme.background = RGB(18, 20, 24);
    theme.text = RGB(232, 236, 241);
    theme.mutedText = RGB(144, 151, 161);
    theme.tabBackground = RGB(24, 27, 31);
    theme.tabItem = RGB(48, 53, 60);
    theme.tabItemActive = RGB(78, 120, 184);
    theme.tabText = RGB(206, 211, 218);
    theme.tabTextActive = RGB(245, 247, 250);
    theme.uiFont.family = L"Segoe UI";
    theme.uiFont.height = -20;
    theme.tabHeight = 38;
    return theme;
}

void CleanupState(DemoState* state) {
    if (!state) {
        return;
    }
    delete state;
}

HWND CreatePage(HWND parent, int controlId, const wchar_t* text, HFONT font) {
    HWND page = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE,
                                0, 0, 0, 0, parent,
                                reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
                                reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(parent, GWLP_HINSTANCE)),
                                nullptr);
    if (page && font) {
        SendMessageW(page, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
    return page;
}

void Layout(HWND window, DemoState* state) {
    RECT client{};
    GetClientRect(window, &client);
    MoveWindow(state->tab.hwnd(), 32, 104, client.right - 64, client.bottom - 136, TRUE);
}

RECT GetDescriptionRect(HWND window) {
    RECT client{};
    GetClientRect(window, &client);
    return RECT{32, 64, client.right - 32, 118};
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

        darkui::Tab::Options tabOptions;
        tabOptions.items = {
            {L"Overview", 1},
            {L"Metrics", 2},
            {L"Notes", 3},
        };
        tabOptions.selection = 0;
        tabOptions.variant = darkui::TabVariant::Accent;
        if (!created->tab.Create(window, ID_TAB, created->host.theme(), tabOptions)) {
            CleanupState(created);
            return -1;
        }
        created->host.theme_manager().Bind(created->tab);

        created->pageOne = CreatePage(created->tab.hwnd(), ID_PAGE_ONE, L"Overview page content", created->host.body_font());
        created->pageTwo = CreatePage(created->tab.hwnd(), ID_PAGE_TWO, L"Metrics page content", created->host.body_font());
        created->pageThree = CreatePage(created->tab.hwnd(), ID_PAGE_THREE, L"Notes page content", created->host.body_font());
        if (!created->pageOne || !created->pageTwo || !created->pageThree) {
            CleanupState(created);
            return -1;
        }

        created->tab.AttachPage(0, created->pageOne);
        created->tab.AttachPage(1, created->pageTwo);
        created->tab.AttachPage(2, created->pageThree);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(created));
        Layout(window, created);
        return 0;
    }
    case WM_SIZE:
        if (state) {
            Layout(window, state);
        }
        return 0;
    case WM_NOTIFY:
        if (state) {
            auto* hdr = reinterpret_cast<NMHDR*>(lParam);
            if (hdr && hdr->hwndFrom == state->tab.hwnd() && hdr->code == TCN_SELCHANGE) {
                std::wstring title = L"Tab: " + state->tab.GetItem(state->tab.GetSelection()).text;
                SetWindowTextW(window, title.c_str());
                return 0;
            }
        }
        break;
    case WM_CTLCOLORSTATIC:
        if (state) {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, state->host.theme().text);
            SetBkColor(dc, state->host.theme().background);
            return reinterpret_cast<LRESULT>(state->host.background_brush());
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

            HFONT oldTitle = reinterpret_cast<HFONT>(SelectObject(dc, state->host.title_font()));
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, state->host.theme().text);
            DrawTextW(dc, L"Dark Tab Demo", -1, &titleRect, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
            SelectObject(dc, oldTitle);

            HFONT oldText = reinterpret_cast<HFONT>(SelectObject(dc, state->host.body_font()));
            SetTextColor(dc, state->host.theme().mutedText);
            DrawTextW(dc, L"Custom tab strip with external page windows and standard TCN_SELCHANGE notification.", -1, &descRect, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
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
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_STANDARD_CLASSES | ICC_TAB_CLASSES};
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
                                  780,
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
