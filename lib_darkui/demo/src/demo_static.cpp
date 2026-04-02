#include <windows.h>
#include <commctrl.h>

#include "darkui/darkui.h"

namespace {

constexpr wchar_t kDemoClassName[] = L"DarkUiStaticDemoWindow";
constexpr wchar_t kDemoTitle[] = L"lib_darkui static demo";

enum ControlId {
    ID_PANEL_CARD = 5100,
    ID_STATIC_TITLE = 5101,
    ID_STATIC_ICON = 5102,
    ID_STATIC_BITMAP = 5103
};

struct DemoState {
    darkui::ThemedWindowHost host;
    darkui::Panel card;
    darkui::Static title;
    darkui::Static iconView;
    darkui::Static bitmapView;
    HBITMAP previewBitmap = nullptr;
};

void CleanupState(DemoState* state) {
    if (!state) return;
    if (state->previewBitmap) DeleteObject(state->previewBitmap);
    delete state;
}

darkui::Theme MakeTheme() {
    darkui::Theme theme;
    theme.background = RGB(18, 20, 24);
    theme.panel = RGB(30, 33, 38);
    theme.border = RGB(58, 64, 74);
    theme.staticBackground = theme.panel;
    theme.staticText = RGB(236, 239, 244);
    theme.text = RGB(236, 239, 244);
    theme.mutedText = RGB(150, 156, 166);
    theme.uiFont.family = L"Segoe UI";
    theme.uiFont.height = -20;
    theme.textPadding = 12;
    return theme;
}

HBITMAP CreatePreviewBitmap(HWND window) {
    HDC windowDc = GetDC(window);
    if (!windowDc) return nullptr;
    HDC memoryDc = CreateCompatibleDC(windowDc);
    HBITMAP bitmap = CreateCompatibleBitmap(windowDc, 96, 64);
    if (memoryDc && bitmap) {
        HGDIOBJ oldBitmap = SelectObject(memoryDc, bitmap);
        HBRUSH bg = CreateSolidBrush(RGB(40, 46, 54));
        HBRUSH accent = CreateSolidBrush(RGB(78, 120, 184));
        RECT rect{0, 0, 96, 64};
        FillRect(memoryDc, &rect, bg);
        RECT bar1{10, 14, 86, 24};
        RECT bar2{10, 30, 72, 40};
        RECT bar3{10, 46, 56, 56};
        FillRect(memoryDc, &bar1, accent);
        FillRect(memoryDc, &bar2, accent);
        FillRect(memoryDc, &bar3, accent);
        DeleteObject(bg);
        DeleteObject(accent);
        SelectObject(memoryDc, oldBitmap);
    }
    if (memoryDc) DeleteDC(memoryDc);
    ReleaseDC(window, windowDc);
    return bitmap;
}

void Layout(HWND, DemoState* state) {
    MoveWindow(state->card.hwnd(), 32, 108, 340, 200, TRUE);
    MoveWindow(state->title.hwnd(), 20, 18, 300, 44, TRUE);
    MoveWindow(state->iconView.hwnd(), 20, 86, 96, 96, TRUE);
    MoveWindow(state->bitmapView.hwnd(), 140, 86, 140, 96, TRUE);
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
        created->previewBitmap = CreatePreviewBitmap(window);
        if (!created->previewBitmap) {
            CleanupState(created);
            return -1;
        }

        darkui::Panel::Options panelOptions;
        panelOptions.role = darkui::SurfaceRole::Panel;
        panelOptions.cornerRadius = 22;
        darkui::Static::Options titleOptions;
        titleOptions.text = L"Dark static text block";
        titleOptions.variant = darkui::StaticVariant::PanelTitle;
        darkui::Static::Options iconOptions;
        iconOptions.icon = LoadIconW(nullptr, IDI_INFORMATION);
        iconOptions.variant = darkui::StaticVariant::PanelBody;
        darkui::Static::Options bitmapOptions;
        bitmapOptions.bitmap = created->previewBitmap;
        bitmapOptions.variant = darkui::StaticVariant::PanelBody;

        if (!created->card.Create(window, ID_PANEL_CARD, created->host.theme(), panelOptions) ||
            !created->title.Create(created->card.hwnd(), ID_STATIC_TITLE, created->host.theme(), titleOptions) ||
            !created->iconView.Create(created->card.hwnd(), ID_STATIC_ICON, created->host.theme(), iconOptions) ||
            !created->bitmapView.Create(created->card.hwnd(), ID_STATIC_BITMAP, created->host.theme(), bitmapOptions)) {
            CleanupState(created);
            return -1;
        }
        created->host.theme_manager().Bind(created->card, created->title, created->iconView, created->bitmapView);

        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(created));
        Layout(window, created);
        return 0;
    }
    case WM_SIZE:
        if (state) Layout(window, state);
        return 0;
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
            RECT descRect{32, 60, client.right - 32, 92};
            RECT noteRect{32, 290, client.right - 32, 350};

            DrawLine(dc, state->host.title_font(), state->host.theme().text, titleRect, L"Dark Static Demo", DT_LEFT | DT_TOP | DT_SINGLELINE);
            DrawLine(dc, state->host.body_font(), state->host.theme().mutedText, descRect, L"One text label, one icon surface, and one bitmap surface using the same dark static wrapper.", DT_LEFT | DT_TOP | DT_WORDBREAK);
            DrawTextW(dc,
                      L"`darkui::Static` is useful for helper text, status badges, icons, and lightweight preview areas.\nText, icon, and bitmap presentation can be switched at runtime.",
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
