#include <windows.h>
#include <commctrl.h>

#include <algorithm>
#include <cwctype>
#include <string>
#include <vector>

#include "darkui/darkui.h"

namespace {

constexpr wchar_t kDemoClassName[] = L"DarkUiListViewDemoWindow";
constexpr wchar_t kDemoTitle[] = L"lib_darkui listview demo";

enum ControlId {
    ID_LISTVIEW = 8101,
    ID_EDIT_ROWS = 8102,
    ID_EDIT_HEADER_TEXT = 8103,
    ID_EDIT_HEADER_BACKGROUND = 8104,
    ID_EDIT_ITEM_TEXT = 8105,
    ID_EDIT_ITEM_BACKGROUND = 8106,
    ID_EDIT_GRID = 8107,
    ID_BUTTON_APPLY = 8108,
    ID_LABEL_ROWS = 8109,
    ID_LABEL_HEADER_TEXT = 8110,
    ID_LABEL_HEADER_BACKGROUND = 8111,
    ID_LABEL_ITEM_TEXT = 8112,
    ID_LABEL_ITEM_BACKGROUND = 8113,
    ID_LABEL_GRID = 8114
};

struct DemoState {
    darkui::ThemedWindowHost host;
    darkui::ListView listView;
    darkui::Static rowsLabel;
    darkui::Static headerTextLabel;
    darkui::Static headerBackgroundLabel;
    darkui::Static itemTextLabel;
    darkui::Static itemBackgroundLabel;
    darkui::Static gridLabel;
    darkui::Edit rowsEdit;
    darkui::Edit headerTextEdit;
    darkui::Edit headerBackgroundEdit;
    darkui::Edit itemTextEdit;
    darkui::Edit itemBackgroundEdit;
    darkui::Edit gridEdit;
    darkui::Button applyButton;
    int rowCount = 48;
    std::wstring status = L"Native selection style is active. Use Ctrl/Shift, Ctrl+C, or right-click Copy/Select All.";
};

darkui::ListViewRow MakeRow(int index) {
    static const wchar_t* levels[] = {L"Verbose", L"Debug", L"Info", L"Warn", L"Error"};
    static const wchar_t* tags[] = {L"Renderer", L"ADB", L"Network", L"Storage", L"Theme", L"Worker"};
    static const wchar_t* messages[] = {
        L"Frame graph rebuilt after layout update.",
        L"Buffered device output merged into the visible viewport.",
        L"Heartbeat acknowledged from relay endpoint.",
        L"Write queue is approaching the configured watermark.",
        L"Semantic palette applied to the diagnostics page.",
        L"Background export completed without blocking the UI thread."
    };

    const int hour = 12 + ((index / 1800) % 10);
    const int minute = 30 + ((index / 60) % 20);
    const int second = 10 + (index % 50);
    const int millis = 100 + ((index * 137) % 900);

    wchar_t timeText[32]{};
    wchar_t pidText[16]{};
    swprintf_s(timeText, L"%02d:%02d:%02d.%03d", hour, minute % 60, second % 60, millis);
    swprintf_s(pidText, L"%d", 4000 + (index % 31));

    const std::wstring message = std::wstring(messages[index % static_cast<int>(std::size(messages))]) +
                                 L" Row " + std::to_wstring(index + 1) + L" keeps the last column wide for resize testing.";

    return {
        timeText,
        levels[index % static_cast<int>(std::size(levels))],
        pidText,
        tags[index % static_cast<int>(std::size(tags))],
        message
    };
}

std::vector<darkui::ListViewRow> BuildRows(int count) {
    std::vector<darkui::ListViewRow> rows;
    rows.reserve(std::max(0, count));
    for (int index = 0; index < count; ++index) {
        rows.push_back(MakeRow(index));
    }
    return rows;
}

std::wstring Trim(const std::wstring& text) {
    std::size_t start = 0;
    while (start < text.size() && iswspace(text[start])) {
        ++start;
    }
    std::size_t end = text.size();
    while (end > start && iswspace(text[end - 1])) {
        --end;
    }
    return text.substr(start, end - start);
}

std::wstring ColorToHex(COLORREF color) {
    wchar_t buffer[16]{};
    swprintf_s(buffer, L"#%02X%02X%02X", GetRValue(color), GetGValue(color), GetBValue(color));
    return buffer;
}

bool ParseHexColor(const std::wstring& text, COLORREF* outColor) {
    if (!outColor) {
        return false;
    }

    std::wstring value = Trim(text);
    if (!value.empty() && value.front() == L'#') {
        value.erase(value.begin());
    }
    if (value.size() != 6) {
        return false;
    }

    for (wchar_t ch : value) {
        if (!iswxdigit(ch)) {
            return false;
        }
    }

    const unsigned long packed = wcstoul(value.c_str(), nullptr, 16);
    *outColor = RGB((packed >> 16) & 0xFF, (packed >> 8) & 0xFF, packed & 0xFF);
    return true;
}

bool ParseRowCount(const std::wstring& text, int* outCount) {
    if (!outCount) {
        return false;
    }

    const std::wstring trimmed = Trim(text);
    if (trimmed.empty()) {
        return false;
    }

    wchar_t* end = nullptr;
    const long value = wcstol(trimmed.c_str(), &end, 10);
    if (!end || *end != L'\0') {
        return false;
    }
    *outCount = std::clamp(static_cast<int>(value), 1, 5000);
    return true;
}

RECT GetWorkArea() {
    RECT workArea{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    return workArea;
}

void UpdateListViewColumns(DemoState* state, int listWidth) {
    if (!state) {
        return;
    }

    const int fixed = 180 + 90 + 90 + 180;
    const int messageWidth = std::max(280, listWidth - fixed - 10);
    state->listView.SetColumnWidth(0, 180);
    state->listView.SetColumnWidth(1, 90);
    state->listView.SetColumnWidth(2, 90);
    state->listView.SetColumnWidth(3, 180);
    state->listView.SetColumnWidth(4, messageWidth);
}

void Layout(HWND window, DemoState* state) {
    if (!state) {
        return;
    }

    RECT client{};
    GetClientRect(window, &client);

    const int margin = 24;
    const int gap = 12;
    const int top = 88;
    const int panelHeight = 154;
    const int buttonWidth = 120;
    const int labelHeight = 20;
    const int editHeight = 38;
    const int configTop = client.bottom - margin - panelHeight;
    const int listHeight = std::max(220, configTop - top - gap);
    const int contentWidth = client.right - margin * 2;

    MoveWindow(state->listView.hwnd(), margin, top, contentWidth, listHeight, TRUE);
    UpdateListViewColumns(state, contentWidth - 2);

    const int fieldsWidth = contentWidth - buttonWidth - gap;
    const int groupWidth = std::max(150, (fieldsWidth - gap * 2) / 3);

    const int col0 = margin;
    const int col1 = col0 + groupWidth + gap;
    const int col2 = col1 + groupWidth + gap;
    const int buttonLeft = margin + fieldsWidth + gap;
    const int row1LabelTop = configTop;
    const int row1EditTop = row1LabelTop + labelHeight + 6;
    const int row2LabelTop = row1EditTop + editHeight + 14;
    const int row2EditTop = row2LabelTop + labelHeight + 6;

    MoveWindow(state->rowsLabel.hwnd(), col0, row1LabelTop, groupWidth, labelHeight, TRUE);
    MoveWindow(state->headerTextLabel.hwnd(), col1, row1LabelTop, groupWidth, labelHeight, TRUE);
    MoveWindow(state->headerBackgroundLabel.hwnd(), col2, row1LabelTop, groupWidth, labelHeight, TRUE);
    MoveWindow(state->rowsEdit.hwnd(), col0, row1EditTop, groupWidth, editHeight, TRUE);
    MoveWindow(state->headerTextEdit.hwnd(), col1, row1EditTop, groupWidth, editHeight, TRUE);
    MoveWindow(state->headerBackgroundEdit.hwnd(), col2, row1EditTop, groupWidth, editHeight, TRUE);

    MoveWindow(state->itemTextLabel.hwnd(), col0, row2LabelTop, groupWidth, labelHeight, TRUE);
    MoveWindow(state->itemBackgroundLabel.hwnd(), col1, row2LabelTop, groupWidth, labelHeight, TRUE);
    MoveWindow(state->gridLabel.hwnd(), col2, row2LabelTop, groupWidth, labelHeight, TRUE);
    MoveWindow(state->itemTextEdit.hwnd(), col0, row2EditTop, groupWidth, editHeight, TRUE);
    MoveWindow(state->itemBackgroundEdit.hwnd(), col1, row2EditTop, groupWidth, editHeight, TRUE);
    MoveWindow(state->gridEdit.hwnd(), col2, row2EditTop, groupWidth, editHeight, TRUE);

    MoveWindow(state->applyButton.hwnd(), buttonLeft, row1EditTop, buttonWidth, editHeight * 2 + labelHeight + 20, TRUE);
}

void SyncControlsFromTheme(DemoState* state) {
    if (!state) {
        return;
    }

    const darkui::Theme& theme = state->host.theme();
    state->rowsEdit.SetText(std::to_wstring(state->rowCount));
    state->headerTextEdit.SetText(ColorToHex(theme.tableHeaderText));
    state->headerBackgroundEdit.SetText(ColorToHex(theme.tableHeaderBackground));
    state->itemTextEdit.SetText(ColorToHex(theme.tableText));
    state->itemBackgroundEdit.SetText(ColorToHex(theme.tableBackground));
    state->gridEdit.SetText(ColorToHex(theme.tableGrid));
}

void ApplyConfig(HWND window, DemoState* state) {
    if (!state) {
        return;
    }

    int rowCount = 0;
    if (!ParseRowCount(state->rowsEdit.GetText(), &rowCount)) {
        state->status = L"Rows must be an integer between 1 and 5000.";
        InvalidateRect(window, nullptr, FALSE);
        return;
    }

    COLORREF headerText = 0;
    COLORREF headerBackground = 0;
    COLORREF itemText = 0;
    COLORREF itemBackground = 0;
    COLORREF grid = 0;
    if (!ParseHexColor(state->headerTextEdit.GetText(), &headerText) ||
        !ParseHexColor(state->headerBackgroundEdit.GetText(), &headerBackground) ||
        !ParseHexColor(state->itemTextEdit.GetText(), &itemText) ||
        !ParseHexColor(state->itemBackgroundEdit.GetText(), &itemBackground) ||
        !ParseHexColor(state->gridEdit.GetText(), &grid)) {
        state->status = L"Colors must use #RRGGBB format.";
        InvalidateRect(window, nullptr, FALSE);
        return;
    }

    darkui::Theme theme = state->host.theme();
    theme.useSemanticPalette = false;
    theme.tableHeaderText = headerText;
    theme.tableHeaderBackground = headerBackground;
    theme.tableText = itemText;
    theme.tableBackground = itemBackground;
    theme.tableGrid = grid;
    theme.border = grid;
    state->host.ApplyTheme(theme);

    state->rowCount = rowCount;
    state->listView.SetRows(BuildRows(state->rowCount));
    RECT client{};
    GetClientRect(window, &client);
    UpdateListViewColumns(state, client.right - 48 - 2);

    state->status = L"Applied " + std::to_wstring(state->rowCount) + L" rows. Native selection style and multi-select stay active.";
    InvalidateRect(window, nullptr, FALSE);
}

void DrawLine(HDC dc, HFONT font, COLORREF color, RECT rect, const wchar_t* text, UINT format) {
    HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(dc, font));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, text, -1, &rect, format | DT_NOPREFIX);
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
        hostOptions.theme = darkui::MakePresetTheme(darkui::ThemePreset::Graphite);
        hostOptions.titleBarStyle = darkui::TitleBarStyle::Black;
        if (!created->host.Attach(window, hostOptions)) {
            delete created;
            return -1;
        }

        darkui::Static::Options labelOptions;
        labelOptions.variant = darkui::StaticVariant::Muted;
        darkui::Edit::Options editOptions;
        editOptions.variant = darkui::FieldVariant::Dense;
        darkui::Button::Options applyOptions;
        applyOptions.text = L"Apply";
        applyOptions.variant = darkui::ButtonVariant::Primary;

        darkui::ListView::Options options;
        options.columns = {
            {L"Time", 180},
            {L"Level", 90},
            {L"PID", 90},
            {L"Tag", 180},
            {L"Message", 760},
        };
        options.style &= ~static_cast<DWORD>(LVS_SINGLESEL);
        options.rows = BuildRows(created->rowCount);

        const darkui::Theme& theme = created->host.theme();
        if (!created->listView.Create(window, ID_LISTVIEW, theme, options) ||
            !created->rowsLabel.Create(window, ID_LABEL_ROWS, theme, {.text = L"Rows", .variant = darkui::StaticVariant::Muted}) ||
            !created->headerTextLabel.Create(window, ID_LABEL_HEADER_TEXT, theme, {.text = L"Header Text", .variant = darkui::StaticVariant::Muted}) ||
            !created->headerBackgroundLabel.Create(window, ID_LABEL_HEADER_BACKGROUND, theme, {.text = L"Header Background", .variant = darkui::StaticVariant::Muted}) ||
            !created->itemTextLabel.Create(window, ID_LABEL_ITEM_TEXT, theme, {.text = L"Item Text", .variant = darkui::StaticVariant::Muted}) ||
            !created->itemBackgroundLabel.Create(window, ID_LABEL_ITEM_BACKGROUND, theme, {.text = L"Item Background", .variant = darkui::StaticVariant::Muted}) ||
            !created->gridLabel.Create(window, ID_LABEL_GRID, theme, {.text = L"Grid", .variant = darkui::StaticVariant::Muted}) ||
            !created->rowsEdit.Create(window, ID_EDIT_ROWS, theme, editOptions) ||
            !created->headerTextEdit.Create(window, ID_EDIT_HEADER_TEXT, theme, editOptions) ||
            !created->headerBackgroundEdit.Create(window, ID_EDIT_HEADER_BACKGROUND, theme, editOptions) ||
            !created->itemTextEdit.Create(window, ID_EDIT_ITEM_TEXT, theme, editOptions) ||
            !created->itemBackgroundEdit.Create(window, ID_EDIT_ITEM_BACKGROUND, theme, editOptions) ||
            !created->gridEdit.Create(window, ID_EDIT_GRID, theme, editOptions) ||
            !created->applyButton.Create(window, ID_BUTTON_APPLY, theme, applyOptions)) {
            delete created;
            return -1;
        }

        created->host.theme_manager().Bind(created->listView,
                                           created->rowsLabel,
                                           created->headerTextLabel,
                                           created->headerBackgroundLabel,
                                           created->itemTextLabel,
                                           created->itemBackgroundLabel,
                                           created->gridLabel,
                                           created->rowsEdit,
                                           created->headerTextEdit,
                                           created->headerBackgroundEdit,
                                           created->itemTextEdit,
                                           created->itemBackgroundEdit,
                                           created->gridEdit,
                                           created->applyButton);
        created->host.theme_manager().Apply();
        SyncControlsFromTheme(created);

        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(created));
        Layout(window, created);
        return 0;
    }
    case WM_SIZE:
        Layout(window, state);
        return 0;
    case WM_COMMAND:
        if (!state) {
            break;
        }
        if (LOWORD(wParam) == ID_BUTTON_APPLY && HIWORD(wParam) == BN_CLICKED) {
            ApplyConfig(window, state);
            return 0;
        }
        if ((LOWORD(wParam) == ID_EDIT_ROWS || LOWORD(wParam) == ID_EDIT_HEADER_TEXT || LOWORD(wParam) == ID_EDIT_HEADER_BACKGROUND ||
             LOWORD(wParam) == ID_EDIT_ITEM_TEXT || LOWORD(wParam) == ID_EDIT_ITEM_BACKGROUND || LOWORD(wParam) == ID_EDIT_GRID) &&
            HIWORD(wParam) == EN_CHANGE) {
            state->status = L"Pending changes. Click Apply to refresh colors and row count.";
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

            RECT titleRect{24, 20, client.right - 24, 50};
            RECT descRect{24, 50, client.right - 24, 78};
            RECT statusRect{24, client.bottom - 32, client.right - 24, client.bottom - 12};

            DrawLine(dc, state->host.title_font(), state->host.theme().text, titleRect, L"Native Dark ListView Demo", DT_LEFT | DT_TOP | DT_SINGLELINE);
            DrawLine(dc,
                     state->host.body_font(),
                     state->host.theme().mutedText,
                     descRect,
                     L"Bottom controls let you rebuild rows, tune header/body/grid colors, and test native multi-select, Ctrl+C, and the context menu.",
                     DT_LEFT | DT_TOP | DT_WORDBREAK);
            DrawLine(dc, state->host.body_font(), state->host.theme().text, statusRect, state->status.c_str(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            EndPaint(window, &ps);
            return 0;
        }
        break;
    case WM_DESTROY:
        delete state;
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
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES;
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

    const RECT workArea = GetWorkArea();
    const int workWidth = workArea.right - workArea.left;
    const int workHeight = workArea.bottom - workArea.top;
    const int width = std::max(1080, (workWidth * 8) / 10);
    const int height = std::max(760, (workHeight * 8) / 10);
    const int x = workArea.left + (workWidth - width) / 2;
    const int y = workArea.top + (workHeight - height) / 2;

    HWND window = CreateWindowExW(0,
                                  kDemoClassName,
                                  kDemoTitle,
                                  WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                                  x,
                                  y,
                                  width,
                                  height,
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
