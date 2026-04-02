#include <windows.h>
#include <commctrl.h>

#include <string>
#include <vector>

#include "darkui/darkui.h"

namespace {

constexpr wchar_t kDemoClassName[] = L"DarkUiListBoxDemoWindow";
constexpr wchar_t kDemoTitle[] = L"lib_darkui listbox demo";

enum ControlId {
    ID_LIST_SINGLE = 6101,
    ID_LIST_MULTI = 6102
};

struct DemoState {
    darkui::ThemedWindowHost host;
    darkui::ListBox singleList;
    darkui::ListBox multiList;
    std::wstring status = L"Ready";
};

void CleanupState(DemoState* state) {
    if (!state) return;
    delete state;
}

darkui::Theme MakeTheme() {
    darkui::Theme theme;
    theme.background = RGB(18, 20, 24);
    theme.panel = RGB(30, 33, 38);
    theme.border = RGB(58, 64, 74);
    theme.text = RGB(236, 239, 244);
    theme.mutedText = RGB(150, 156, 166);
    theme.listBoxBackground = RGB(30, 33, 38);
    theme.listBoxPanel = RGB(40, 44, 50);
    theme.listBoxText = RGB(236, 239, 244);
    theme.listBoxItemSelected = RGB(82, 132, 204);
    theme.listBoxItemSelectedText = RGB(248, 250, 252);
    theme.uiFont.family = L"Segoe UI";
    theme.uiFont.height = -20;
    theme.textPadding = 10;
    theme.listBoxItemHeight = 28;
    return theme;
}

void Layout(HWND window, DemoState* state) {
    RECT client{};
    GetClientRect(window, &client);
    const int width = (client.right - 84) / 2;
    MoveWindow(state->singleList.hwnd(), 32, 110, width, client.bottom - 190, TRUE);
    MoveWindow(state->multiList.hwnd(), 52 + width, 110, width, client.bottom - 190, TRUE);
}

void DrawLine(HDC dc, HFONT font, COLORREF color, RECT rect, const wchar_t* text, UINT format) {
    HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(dc, font));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, text, -1, &rect, format | DT_NOPREFIX);
    if (oldFont) SelectObject(dc, oldFont);
}

std::wstring JoinSelections(const darkui::ListBox& listBox) {
    std::wstring result;
    const std::vector<int> selected = listBox.GetSelections();
    for (std::size_t i = 0; i < selected.size(); ++i) {
        if (i != 0) result += L", ";
        result += std::to_wstring(selected[i]);
    }
    return result.empty() ? L"(none)" : result;
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

        darkui::ListBox::Options singleOptions;
        singleOptions.variant = darkui::FieldVariant::Default;
        singleOptions.items = {
            {L"Overview", 1},
            {L"Transfers", 2},
            {L"Snapshots", 3},
            {L"Exports", 4},
            {L"Audit Trail", 5}
        };
        singleOptions.selection = 0;
        darkui::ListBox::Options multiOptions;
        multiOptions.variant = darkui::FieldVariant::Dense;
        multiOptions.style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_VSCROLL | LBS_EXTENDEDSEL;
        multiOptions.items = {
            {L"Render queue", 10},
            {L"Cloud sync", 11},
            {L"Archive jobs", 12},
            {L"Review links", 13},
            {L"Local cache", 14},
            {L"Proxy build", 15}
        };

        if (!created->singleList.Create(window, ID_LIST_SINGLE, created->host.theme(), singleOptions) ||
            !created->multiList.Create(window, ID_LIST_MULTI, created->host.theme(), multiOptions)) {
            CleanupState(created);
            return -1;
        }
        created->host.theme_manager().Bind(created->singleList, created->multiList);

        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(created));
        Layout(window, created);
        return 0;
    }
    case WM_SIZE:
        if (state) {
            Layout(window, state);
            RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
        }
        return 0;
    case WM_COMMAND:
        if (!state) break;
        if (HIWORD(wParam) == LBN_SELCHANGE) {
            if (LOWORD(wParam) == ID_LIST_SINGLE) {
                state->status = L"Single-select index: " + std::to_wstring(state->singleList.GetSelection());
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
            if (LOWORD(wParam) == ID_LIST_MULTI) {
                state->status = L"Multi-select indices: " + JoinSelections(state->multiList);
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
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
            RECT descRect{32, 60, client.right - 32, 90};
            RECT leftRect{32, 92, client.right / 2 - 16, 112};
            RECT rightRect{client.right / 2 + 16, 92, client.right - 32, 112};
            RECT statusRect{32, client.bottom - 54, client.right - 32, client.bottom - 22};

            DrawLine(dc, state->host.title_font(), state->host.theme().text, titleRect, L"Dark ListBox Demo", DT_LEFT | DT_TOP | DT_SINGLELINE);
            DrawLine(dc, state->host.body_font(), state->host.theme().mutedText, descRect, L"Left: single-select list. Right: extended multi-select list. Both use a dark rounded host and native keyboard navigation.", DT_LEFT | DT_TOP | DT_WORDBREAK);
            DrawLine(dc, state->host.body_font(), state->host.theme().text, leftRect, L"Single Select", DT_LEFT | DT_TOP | DT_SINGLELINE);
            DrawLine(dc, state->host.body_font(), state->host.theme().text, rightRect, L"Extended Multi Select", DT_LEFT | DT_TOP | DT_SINGLELINE);
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
                                  860,
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
