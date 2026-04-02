#include <windows.h>
#include <commctrl.h>

#include <memory>
#include <string>

#include "darkui/darkui.h"

namespace {

constexpr wchar_t kDemoClassName[] = L"DarkUiDialogDemoWindow";
constexpr wchar_t kDemoTitle[] = L"lib_darkui dialog demo";

enum ControlId {
    ID_OPEN_MESSAGE = 9201,
    ID_OPEN_FORM = 9202,
    ID_DIALOG_FILL = 9301,
    ID_DIALOG_PANEL = 9306,
    ID_DIALOG_TITLE = 9302,
    ID_DIALOG_NOTES = 9303,
    ID_DIALOG_LABEL_TITLE = 9304,
    ID_DIALOG_LABEL_NOTES = 9305
};

struct CustomDialogSession {
    darkui::ThemeManager themeManager;
    darkui::Dialog dialog;
    darkui::Panel formPanel;
    darkui::Static titleLabel;
    darkui::Static notesLabel;
    darkui::Edit titleEdit;
    darkui::Button fillButton;
    darkui::Edit notesEdit;
};

struct DemoState {
    darkui::ThemedWindowHost host;
    darkui::Button messageButton;
    darkui::Button formButton;
    std::wstring status = L"Ready";
    std::unique_ptr<CustomDialogSession> activeDialog;
};

void CleanupState(DemoState* state) {
    if (!state) return;
    delete state;
}

darkui::Theme MakeTheme() {
    darkui::Theme theme;
    theme.background = RGB(18, 20, 24);
    theme.panel = RGB(26, 29, 34);
    theme.border = RGB(58, 64, 74);
    theme.text = RGB(236, 239, 244);
    theme.mutedText = RGB(150, 156, 166);
    theme.staticBackground = theme.panel;
    theme.staticText = theme.text;
    theme.editBackground = RGB(38, 42, 48);
    theme.editText = RGB(236, 239, 244);
    theme.editPlaceholder = RGB(126, 134, 144);
    theme.button = RGB(58, 86, 138);
    theme.buttonHover = RGB(68, 99, 158);
    theme.buttonHot = RGB(82, 114, 180);
    theme.buttonDisabled = RGB(45, 49, 56);
    theme.buttonDisabledText = RGB(120, 126, 136);
    theme.uiFont.family = L"Segoe UI";
    theme.uiFont.height = -20;
    theme.textPadding = 10;
    theme.scrollBarThickness = 14;
    return theme;
}

void Layout(HWND window, DemoState* state) {
    RECT client{};
    GetClientRect(window, &client);
    MoveWindow(state->messageButton.hwnd(), 32, 118, 210, 42, TRUE);
    MoveWindow(state->formButton.hwnd(), 258, 118, 210, 42, TRUE);
}

void DrawLine(HDC dc, HFONT font, COLORREF color, RECT rect, const wchar_t* text, UINT format) {
    HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(dc, font));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, text, -1, &rect, format | DT_NOPREFIX);
    if (oldFont) SelectObject(dc, oldFont);
}

void ShowMessageDialog(HWND window, DemoState* state) {
    darkui::Dialog::Options options;
    options.title = L"Confirm Delete";
    options.message = L"Delete the selected snapshot permanently?\nThis action cannot be undone.";
    options.confirmText = L"Delete";
    options.cancelText = L"Cancel";
    options.width = 460;
    options.height = 230;
    const darkui::Dialog::Result result = darkui::ShowMessageDialog(window, 4001, state->host.theme(), options);
    state->status = (result == darkui::Dialog::Result::Confirm) ? L"Message dialog confirmed" : L"Message dialog cancelled";
    InvalidateRect(window, nullptr, FALSE);
}

void ShowFormDialog(HWND window, DemoState* state) {
    state->activeDialog = std::make_unique<CustomDialogSession>();
    auto& session = *state->activeDialog;
    darkui::Dialog::Options dialogOptions;
    dialogOptions.title = L"Create Note Form";
    dialogOptions.width = 580;
    dialogOptions.height = 400;
    dialogOptions.messageVisible = false;
    dialogOptions.confirmText = L"Save";
    dialogOptions.cancelText = L"Cancel";
    session.themeManager.SetTheme(state->host.theme());
    if (!session.dialog.Create(window, 4002, state->host.theme(), dialogOptions)) {
        state->status = L"Form dialog creation failed";
        state->activeDialog.reset();
        InvalidateRect(window, nullptr, FALSE);
        return;
    }

    darkui::Panel::Options panelOptions;
    panelOptions.role = darkui::SurfaceRole::Panel;
    panelOptions.cornerRadius = 18;
    darkui::Static::Options titleLabelOptions;
    titleLabelOptions.text = L"Title";
    titleLabelOptions.variant = darkui::StaticVariant::PanelTitle;
    darkui::Static::Options notesLabelOptions = titleLabelOptions;
    notesLabelOptions.text = L"Notes";
    darkui::Edit::Options titleEditOptions;
    titleEditOptions.cueBanner = L"Enter a title";
    titleEditOptions.variant = darkui::FieldVariant::Panel;
    darkui::Button::Options fillButtonOptions;
    fillButtonOptions.text = L"Fill Sample";
    fillButtonOptions.variant = darkui::ButtonVariant::Secondary;
    darkui::Edit::Options notesEditOptions;
    notesEditOptions.cueBanner = L"Write your notes here";
    notesEditOptions.variant = darkui::FieldVariant::Panel;
    notesEditOptions.style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL;

    if (!session.formPanel.Create(session.dialog.content_hwnd(), ID_DIALOG_PANEL, state->host.theme(), panelOptions) ||
        !session.titleLabel.Create(session.formPanel.hwnd(), ID_DIALOG_LABEL_TITLE, state->host.theme(), titleLabelOptions) ||
        !session.notesLabel.Create(session.formPanel.hwnd(), ID_DIALOG_LABEL_NOTES, state->host.theme(), notesLabelOptions) ||
        !session.titleEdit.Create(session.formPanel.hwnd(), ID_DIALOG_TITLE, state->host.theme(), titleEditOptions) ||
        !session.fillButton.Create(session.formPanel.hwnd(), ID_DIALOG_FILL, state->host.theme(), fillButtonOptions) ||
        !session.notesEdit.Create(session.formPanel.hwnd(), ID_DIALOG_NOTES, state->host.theme(), notesEditOptions)) {
        state->status = L"Form controls creation failed";
        state->activeDialog.reset();
        InvalidateRect(window, nullptr, FALSE);
        return;
    }
    session.themeManager.Bind(session.dialog, session.formPanel, session.titleLabel, session.notesLabel, session.titleEdit, session.fillButton, session.notesEdit);

    MoveWindow(session.formPanel.hwnd(), 16, 14, 526, 254, TRUE);
    MoveWindow(session.titleLabel.hwnd(), 16, 14, 120, 22, TRUE);
    MoveWindow(session.titleEdit.hwnd(), 16, 40, 374, 38, TRUE);
    MoveWindow(session.fillButton.hwnd(), 406, 40, 104, 38, TRUE);
    MoveWindow(session.notesLabel.hwnd(), 16, 94, 120, 22, TRUE);
    MoveWindow(session.notesEdit.hwnd(), 16, 120, 494, 118, TRUE);

    const darkui::Dialog::Result result = session.dialog.ShowModal();
    if (result == darkui::Dialog::Result::Confirm) {
        state->status = L"Saved note: " + session.titleEdit.GetText() + L" | " + session.notesEdit.GetText();
    } else {
        state->status = L"Form dialog cancelled";
    }
    state->activeDialog.reset();
    InvalidateRect(window, nullptr, FALSE);
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

        darkui::Button::Options messageButtonOptions;
        messageButtonOptions.text = L"Open Message Dialog";
        messageButtonOptions.variant = darkui::ButtonVariant::Primary;
        darkui::Button::Options formButtonOptions = messageButtonOptions;
        formButtonOptions.text = L"Open Form Dialog";
        formButtonOptions.variant = darkui::ButtonVariant::Secondary;
        const darkui::Theme& theme = created->host.theme();
        if (!created->messageButton.Create(window, ID_OPEN_MESSAGE, theme, messageButtonOptions) ||
            !created->formButton.Create(window, ID_OPEN_FORM, theme, formButtonOptions)) {
            CleanupState(created);
            return -1;
        }
        created->host.theme_manager().Bind(created->messageButton, created->formButton);

        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(created));
        Layout(window, created);
        return 0;
    }
    case WM_SIZE:
        if (state) Layout(window, state);
        return 0;
    case WM_COMMAND:
        if (!state) break;
        if (HIWORD(wParam) == BN_CLICKED) {
            if (LOWORD(wParam) == ID_OPEN_MESSAGE) {
                ShowMessageDialog(window, state);
                return 0;
            }
            if (LOWORD(wParam) == ID_OPEN_FORM) {
                ShowFormDialog(window, state);
                return 0;
            }
            if (LOWORD(wParam) == ID_DIALOG_FILL && state->activeDialog) {
                state->activeDialog->titleEdit.SetText(L"Evening Review");
                state->activeDialog->notesEdit.SetText(L"1. Validate dark title bar\n2. Check confirm/cancel buttons\n3. Test multiline input in the custom body");
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
            RECT descRect{32, 60, client.right - 32, 96};
            RECT noteRect{32, 184, client.right - 32, 274};
            RECT statusRect{32, client.bottom - 56, client.right - 32, client.bottom - 24};

            DrawLine(dc, state->host.title_font(), state->host.theme().text, titleRect, L"Dark Dialog Demo", DT_LEFT | DT_TOP | DT_SINGLELINE);
            DrawLine(dc, state->host.body_font(), state->host.theme().mutedText, descRect, L"One button opens a simple confirmation popup. The other opens a custom dialog body containing Static, Edit, Button, and multiline Edit controls.", DT_LEFT | DT_TOP | DT_WORDBREAK);
            DrawTextW(dc,
                      L"`darkui::Dialog` provides a dark modal shell with a black title bar, bottom confirm/cancel buttons, and a content host for custom child controls.",
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
