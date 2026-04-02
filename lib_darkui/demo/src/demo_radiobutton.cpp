#include <windows.h>
#include <commctrl.h>

#include "darkui/darkui.h"

namespace {

constexpr wchar_t kDemoClassName[] = L"DarkUiRadioButtonDemoWindow";
constexpr wchar_t kDemoTitle[] = L"lib_darkui radiobutton demo";

enum ControlId {
    ID_RADIO_A = 8101,
    ID_RADIO_B = 8102,
    ID_RADIO_C = 8103
};

struct DemoState {
    darkui::ThemedWindowHost host;
    darkui::RadioButton radioA;
    darkui::RadioButton radioB;
    darkui::RadioButton radioC;
    std::wstring status = L"Selected mode: Compact";
};

void CleanupState(DemoState* state) {
    if (!state) return;
    delete state;
}

darkui::Theme MakeTheme() {
    darkui::Theme theme;
    theme.background = RGB(18, 20, 24);
    theme.panel = RGB(30, 33, 38);
    theme.text = RGB(236, 239, 244);
    theme.mutedText = RGB(150, 156, 166);
    theme.radioBackground = RGB(43, 47, 54);
    theme.radioBackgroundHot = RGB(55, 60, 68);
    theme.radioAccent = RGB(82, 132, 204);
    theme.radioBorder = RGB(64, 72, 84);
    theme.radioText = RGB(236, 239, 244);
    theme.radioDisabledText = RGB(128, 136, 146);
    theme.uiFont.family = L"Segoe UI";
    theme.uiFont.height = -20;
    return theme;
}

void Layout(HWND, DemoState* state) {
    MoveWindow(state->radioA.hwnd(), 32, 126, 320, 28, TRUE);
    MoveWindow(state->radioB.hwnd(), 32, 166, 320, 28, TRUE);
    MoveWindow(state->radioC.hwnd(), 32, 206, 320, 28, TRUE);
}

void DrawLine(HDC dc, HFONT font, COLORREF color, RECT rect, const wchar_t* text, UINT format) {
    HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(dc, font));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, text, -1, &rect, format | DT_NOPREFIX);
    if (oldFont) SelectObject(dc, oldFont);
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

        darkui::RadioButton::Options radioAOptions;
        radioAOptions.text = L"Compact mode";
        radioAOptions.checked = true;
        radioAOptions.variant = darkui::SelectionVariant::Accent;
        radioAOptions.style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_GROUP;
        darkui::RadioButton::Options radioBOptions;
        radioBOptions.text = L"Balanced mode";
        radioBOptions.variant = darkui::SelectionVariant::Default;
        darkui::RadioButton::Options radioCOptions;
        radioCOptions.text = L"Detailed mode";
        radioCOptions.variant = darkui::SelectionVariant::Panel;

        if (!created->radioA.Create(window, ID_RADIO_A, created->host.theme(), radioAOptions) ||
            !created->radioB.Create(window, ID_RADIO_B, created->host.theme(), radioBOptions) ||
            !created->radioC.Create(window, ID_RADIO_C, created->host.theme(), radioCOptions)) {
            CleanupState(created);
            return -1;
        }
        created->host.theme_manager().Bind(created->radioA, created->radioB, created->radioC);

        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(created));
        Layout(window, created);
        return 0;
    }
    case WM_SIZE:
        if (state) Layout(window, state);
        return 0;
    case WM_COMMAND:
        if (state && HIWORD(wParam) == BN_CLICKED) {
            if (state->radioA.GetChecked()) state->status = L"Selected mode: Compact";
            if (state->radioB.GetChecked()) state->status = L"Selected mode: Balanced";
            if (state->radioC.GetChecked()) state->status = L"Selected mode: Detailed";
            InvalidateRect(window, nullptr, FALSE);
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

            RECT titleRect{32, 24, client.right - 32, 58};
            RECT descRect{32, 60, client.right - 32, 94};
            RECT noteRect{32, 252, client.right - 32, 330};
            RECT statusRect{32, client.bottom - 56, client.right - 32, client.bottom - 24};

            DrawLine(dc, state->host.title_font(), state->host.theme().text, titleRect, L"Dark RadioButton Demo", DT_LEFT | DT_TOP | DT_SINGLELINE);
            DrawLine(dc, state->host.body_font(), state->host.theme().mutedText, descRect, L"Three radio buttons share native auto-radio grouping while using a dark owner-drawn circle and accent dot.", DT_LEFT | DT_TOP | DT_WORDBREAK);
            DrawTextW(dc,
                      L"The first radio uses WS_GROUP to start the group. Clicking any option updates the status line below through normal BN_CLICKED handling.",
                      -1,
                      &noteRect,
                      DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
            DrawLine(dc, state->host.body_font(), state->host.theme().text, statusRect, state->status.c_str(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);

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
