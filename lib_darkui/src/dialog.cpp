#include "darkui/dialog.h"
#include "darkui/quick.h"

#include <commctrl.h>
#include <windowsx.h>

#include <algorithm>

namespace darkui {
namespace {

constexpr wchar_t kDialogClassName[] = L"DarkUiDialogWindow";
constexpr wchar_t kDialogContentClassName[] = L"DarkUiDialogContent";
constexpr int kDialogConfirmId = 0x6D10;
constexpr int kDialogCancelId = 0x6D11;
constexpr int kDialogMessageId = 0x6D12;
constexpr int kDialogContentId = 0x6D13;
constexpr int kDialogTitleId = 0x6D14;

ATOM EnsureDialogClassRegistered(HINSTANCE instance);
ATOM EnsureDialogContentClassRegistered(HINSTANCE instance);
LRESULT CALLBACK DialogWindowProcThunk(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK DialogContentWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

}  // namespace

struct Dialog::Impl {
    Dialog* owner = nullptr;
    HINSTANCE instance = nullptr;
    HBRUSH brushBackground = nullptr;
    HBRUSH brushTitle = nullptr;
    HBRUSH brushContent = nullptr;
    HBRUSH brushBorder = nullptr;
    HFONT titleFont = nullptr;
    HFONT bodyFont = nullptr;
    ThemeBinder builtInThemeBinder{};

    explicit Impl(Dialog* dialog) : owner(dialog) {}

    ~Impl() {
        if (brushBackground) DeleteObject(brushBackground);
        if (brushTitle) DeleteObject(brushTitle);
        if (brushContent) DeleteObject(brushContent);
        if (brushBorder) DeleteObject(brushBorder);
        if (titleFont) DeleteObject(titleFont);
        if (bodyFont) DeleteObject(bodyFont);
    }

    bool UpdateThemeResources() {
        HBRUSH newBackground = CreateSolidBrush(owner->theme_.background);
        HBRUSH newTitle = CreateSolidBrush(owner->theme_.panel);
        HBRUSH newContent = CreateSolidBrush(owner->theme_.panel);
        HBRUSH newBorder = CreateSolidBrush(owner->theme_.border);
        FontSpec titleSpec = owner->theme_.uiFont;
        titleSpec.height = std::min(titleSpec.height, -20);
        titleSpec.weight = FW_SEMIBOLD;
        HFONT newTitleFont = CreateFont(titleSpec);
        HFONT newBodyFont = CreateFont(owner->theme_.uiFont);
        if (!newBackground || !newTitle || !newContent || !newBorder || !newTitleFont || !newBodyFont) {
            if (newBackground) DeleteObject(newBackground);
            if (newTitle) DeleteObject(newTitle);
            if (newContent) DeleteObject(newContent);
            if (newBorder) DeleteObject(newBorder);
            if (newTitleFont) DeleteObject(newTitleFont);
            if (newBodyFont) DeleteObject(newBodyFont);
            return false;
        }

        if (brushBackground) DeleteObject(brushBackground);
        if (brushTitle) DeleteObject(brushTitle);
        if (brushContent) DeleteObject(brushContent);
        if (brushBorder) DeleteObject(brushBorder);
        if (titleFont) DeleteObject(titleFont);
        if (bodyFont) DeleteObject(bodyFont);

        brushBackground = newBackground;
        brushTitle = newTitle;
        brushContent = newContent;
        brushBorder = newBorder;
        titleFont = newTitleFont;
        bodyFont = newBodyFont;

        if (owner->dialogHwnd_) {
            InvalidateRect(owner->dialogHwnd_, nullptr, TRUE);
        }
        return true;
    }

    RECT ClientRect(HWND window) const {
        RECT rect{};
        GetClientRect(window, &rect);
        return rect;
    }

    RECT TitleRect(HWND window) const {
        RECT rect = ClientRect(window);
        rect.bottom = std::min(rect.bottom, rect.top + 42);
        return rect;
    }

    RECT FooterRect(HWND window) const {
        RECT rect = ClientRect(window);
        rect.top = std::max(rect.top, rect.bottom - 62);
        return rect;
    }

    RECT ContentRect(HWND window) const {
        RECT rect = ClientRect(window);
        rect.top += 42;
        rect.bottom -= 62;
        rect.left += 14;
        rect.right -= 14;
        rect.top += 10;
        rect.bottom -= 6;
        return rect;
    }

    void ApplyBuiltInControlSurfaces() {
        owner->confirmButton_.SetSurfaceColor(owner->theme_.panel);
        owner->cancelButton_.SetSurfaceColor(owner->theme_.panel);
        owner->titleLabel_.SetBackgroundColor(owner->theme_.panel);
        owner->messageLabel_.SetBackgroundColor(owner->theme_.panel);
    }

    void LayoutChildren() {
        if (!owner->dialogHwnd_ || !owner->contentHwnd_) {
            return;
        }

        RECT content = ContentRect(owner->dialogHwnd_);
        RECT footer = FooterRect(owner->dialogHwnd_);

        MoveWindow(owner->contentHwnd_,
                   content.left,
                   content.top,
                   std::max(1L, content.right - content.left),
                   std::max(1L, content.bottom - content.top),
                   TRUE);

        const int buttonWidth = 110;
        const int buttonHeight = 36;
        const int buttonGap = 12;
        const int top = footer.top + ((footer.bottom - footer.top) - buttonHeight) / 2;
        int right = footer.right - 18;

        if (owner->cancelVisible_) {
            MoveWindow(owner->cancelButton_.hwnd(), right - buttonWidth, top, buttonWidth, buttonHeight, TRUE);
            ShowWindow(owner->cancelButton_.hwnd(), SW_SHOW);
            right -= buttonWidth + buttonGap;
        } else if (owner->cancelButton_.hwnd()) {
            ShowWindow(owner->cancelButton_.hwnd(), SW_HIDE);
        }

        MoveWindow(owner->confirmButton_.hwnd(), right - buttonWidth, top, buttonWidth, buttonHeight, TRUE);

        RECT client = ClientRect(owner->dialogHwnd_);
        MoveWindow(owner->titleLabel_.hwnd(),
                   16,
                   8,
                   std::max(1L, client.right - client.left - 32),
                   24,
                   TRUE);
        SetWindowPos(owner->titleLabel_.hwnd(), HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

        RECT host{};
        GetClientRect(owner->contentHwnd_, &host);
        MoveWindow(owner->messageLabel_.hwnd(),
                   16,
                   12,
                   std::max(1L, host.right - host.left - 32),
                   std::max(1L, host.bottom - host.top - 24),
                   TRUE);
        ShowWindow(owner->messageLabel_.hwnd(), owner->messageVisible_ ? SW_SHOW : SW_HIDE);
    }

    void CenterToOwner() {
        if (!owner->dialogHwnd_) {
            return;
        }

        RECT target{};
        if (owner->ownerHwnd_ && IsWindow(owner->ownerHwnd_)) {
            GetWindowRect(owner->ownerHwnd_, &target);
        } else {
            SystemParametersInfoW(SPI_GETWORKAREA, 0, &target, 0);
        }

        const int width = owner->width_;
        const int height = owner->height_;
        const int x = target.left + ((target.right - target.left) - width) / 2;
        const int y = target.top + ((target.bottom - target.top) - height) / 2;
        SetWindowPos(owner->dialogHwnd_, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
    }

    void Paint(HDC dc, HWND window) {
        RECT client = ClientRect(window);
        RECT title = TitleRect(window);

        FillRect(dc, &client, brushContent ? brushContent : brushBackground);
        FillRect(dc, &title, brushTitle);
        FrameRect(dc, &client, brushBorder);
        RECT divider = title;
        divider.top = divider.bottom - 1;
        FillRect(dc, &divider, brushBorder);
    }

    void CloseWithResult(Result result) {
        owner->EndDialog(result);
    }

    static LRESULT CALLBACK DialogWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
        auto* self = reinterpret_cast<Impl*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            self = reinterpret_cast<Impl*>(create->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            return TRUE;
        }

        if (!self) {
            return DefWindowProcW(window, message, wParam, lParam);
        }

        switch (message) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC dc = BeginPaint(window, &ps);
            self->Paint(dc, window);
            EndPaint(window, &ps);
            return 0;
        }
        case WM_NCHITTEST: {
            LRESULT hit = DefWindowProcW(window, message, wParam, lParam);
            if (hit != HTCLIENT) {
                return hit;
            }
            POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ScreenToClient(window, &pt);
            RECT title = self->TitleRect(window);
            if (PtInRect(&title, pt)) {
                return HTCAPTION;
            }
            return HTCLIENT;
        }
        case WM_SIZE:
            self->LayoutChildren();
            return 0;
        case WM_COMMAND:
            if (LOWORD(wParam) == kDialogConfirmId && HIWORD(wParam) == BN_CLICKED) {
                self->CloseWithResult(Result::Confirm);
                return 0;
            }
            if (LOWORD(wParam) == kDialogCancelId && HIWORD(wParam) == BN_CLICKED) {
                self->CloseWithResult(Result::Cancel);
                return 0;
            }
            if (self->owner->ownerHwnd_) {
                SendMessageW(self->owner->ownerHwnd_, WM_COMMAND, wParam, lParam);
                return 0;
            }
            break;
        case WM_NOTIFY:
            if (self->owner->ownerHwnd_) {
                return SendMessageW(self->owner->ownerHwnd_, WM_NOTIFY, wParam, lParam);
            }
            break;
        case WM_CLOSE:
            self->CloseWithResult(Result::Cancel);
            return 0;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                self->CloseWithResult(Result::Cancel);
                return 0;
            }
            if (wParam == VK_RETURN && GetFocus() != self->owner->cancelButton_.hwnd()) {
                self->CloseWithResult(Result::Confirm);
                return 0;
            }
            break;
        case WM_DESTROY:
            SetWindowLongPtrW(window, GWLP_USERDATA, 0);
            break;
        default:
            break;
        }

        return DefWindowProcW(window, message, wParam, lParam);
    }
};

namespace {

LRESULT CALLBACK DialogWindowProcThunk(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    return Dialog::Impl::DialogWindowProc(window, message, wParam, lParam);
}

LRESULT CALLBACK DialogContentWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* brush = reinterpret_cast<HBRUSH>(GetPropW(window, L"DarkUiDialogContentBrush"));
    HWND parent = GetParent(window);
    switch (message) {
    case WM_ERASEBKGND:
        return 1;
    case WM_COMMAND:
        if (parent) {
            return SendMessageW(parent, WM_COMMAND, wParam, lParam);
        }
        return 0;
    case WM_NOTIFY:
        if (parent) {
            return SendMessageW(parent, WM_NOTIFY, wParam, lParam);
        }
        return 0;
    case WM_HSCROLL:
    case WM_VSCROLL:
        if (parent) {
            return SendMessageW(parent, message, wParam, lParam);
        }
        return 0;
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORDLG:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORSTATIC:
        if (parent) {
            return SendMessageW(parent, message, wParam, lParam);
        }
        break;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(window, &ps);
        RECT rect{};
        GetClientRect(window, &rect);
        FillRect(dc, &rect, brush ? brush : reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        EndPaint(window, &ps);
        return 0;
    }
    case WM_DESTROY:
        RemovePropW(window, L"DarkUiDialogContentBrush");
        return 0;
    default:
        break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

ATOM EnsureDialogClassRegistered(HINSTANCE instance) {
    static ATOM atom = 0;
    if (atom != 0) {
        return atom;
    }
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DialogWindowProcThunk;
    wc.hInstance = instance;
    wc.lpszClassName = kDialogClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    atom = RegisterClassExW(&wc);
    return atom;
}

ATOM EnsureDialogContentClassRegistered(HINSTANCE instance) {
    static ATOM atom = 0;
    if (atom != 0) {
        return atom;
    }
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DialogContentWindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = kDialogContentClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    atom = RegisterClassExW(&wc);
    return atom;
}

}  // namespace

Dialog::Dialog() : impl_(std::make_unique<Impl>(this)) {}

Dialog::~Dialog() {
    Destroy();
}

bool Dialog::Create(HWND owner, int controlId, const Theme& theme, const Options& options) {
    Destroy();
    ownerHwnd_ = owner;
    controlId_ = controlId;
    theme_ = ResolveTheme(theme);
    width_ = std::max(320, options.width);
    height_ = std::max(180, options.height);
    impl_->instance = owner ? reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(owner, GWLP_HINSTANCE)) : GetModuleHandleW(nullptr);
    if (!impl_->instance) {
        impl_->instance = GetModuleHandleW(nullptr);
    }

    if (!EnsureDialogClassRegistered(impl_->instance) || !EnsureDialogContentClassRegistered(impl_->instance)) {
        Destroy();
        return false;
    }
    if (!impl_->UpdateThemeResources()) {
        Destroy();
        return false;
    }

    dialogHwnd_ = CreateWindowExW(WS_EX_CONTROLPARENT,
                                  kDialogClassName,
                                  options.title.c_str(),
                                  WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                                  CW_USEDEFAULT,
                                  CW_USEDEFAULT,
                                  width_,
                                  height_,
                                  owner,
                                  nullptr,
                                  impl_->instance,
                                  impl_.get());
    if (!dialogHwnd_) {
        Destroy();
        return false;
    }

    contentHwnd_ = CreateWindowExW(0,
                                   kDialogContentClassName,
                                   L"",
                                   WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
                                   0,
                                   0,
                                   0,
                                   0,
                                   dialogHwnd_,
                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(kDialogContentId)),
                                   impl_->instance,
                                   nullptr);
    if (!contentHwnd_) {
        Destroy();
        return false;
    }
    SetPropW(contentHwnd_, L"DarkUiDialogContentBrush", impl_->brushContent);

    Static::Options titleOptions;
    titleOptions.text = options.title;
    titleOptions.style = WS_CHILD | WS_VISIBLE | SS_LEFT;
    titleOptions.backgroundColor = theme_.panel;
    titleOptions.textFormat = DT_LEFT | DT_VCENTER | DT_SINGLELINE;
    Button::Options confirmOptions;
    confirmOptions.text = options.confirmText;
    confirmOptions.cornerRadius = 12;
    confirmOptions.surfaceColor = theme_.panel;
    Button::Options cancelOptions = confirmOptions;
    cancelOptions.text = options.cancelText;
    Static::Options messageOptions;
    messageOptions.text = options.message;
    messageOptions.style = WS_CHILD | WS_VISIBLE | SS_CENTER;
    messageOptions.backgroundColor = theme_.panel;
    messageOptions.textFormat = DT_CENTER | DT_VCENTER | DT_WORDBREAK;
    messageOptions.ellipsis = false;
    if (!titleLabel_.Create(dialogHwnd_, kDialogTitleId, theme_, titleOptions) ||
        !messageLabel_.Create(contentHwnd_, kDialogMessageId, theme_, messageOptions) ||
        !confirmButton_.Create(dialogHwnd_, kDialogConfirmId, theme_, confirmOptions) ||
        !cancelButton_.Create(dialogHwnd_, kDialogCancelId, theme_, cancelOptions)) {
        Destroy();
        return false;
    }

    impl_->builtInThemeBinder.Clear();
    impl_->builtInThemeBinder.Bind(confirmButton_, cancelButton_, titleLabel_, messageLabel_);
    impl_->ApplyBuiltInControlSurfaces();

    titleLabel_.SetEllipsis(true);
    SetTitle(options.title);
    SetMessage(options.message);
    SetConfirmText(options.confirmText);
    SetCancelText(options.cancelText);
    SetMessageVisible(options.messageVisible);
    SetCancelVisible(options.cancelVisible);

    impl_->CenterToOwner();
    impl_->LayoutChildren();
    return true;
}

void Dialog::Destroy() {
    modalRunning_ = false;
    titleLabel_.Destroy();
    messageLabel_.Destroy();
    confirmButton_.Destroy();
    cancelButton_.Destroy();
    if (contentHwnd_) {
        DestroyWindow(contentHwnd_);
        contentHwnd_ = nullptr;
    }
    if (dialogHwnd_) {
        DestroyWindow(dialogHwnd_);
        dialogHwnd_ = nullptr;
    }
    ownerHwnd_ = nullptr;
    controlId_ = 0;
    modalResult_ = Result::None;
}

void Dialog::SetTheme(const Theme& theme) {
    const Theme previous = theme_;
    theme_ = ResolveTheme(theme);
    if (!impl_->UpdateThemeResources()) {
        theme_ = previous;
        impl_->UpdateThemeResources();
        return;
    }
    impl_->builtInThemeBinder.Apply(theme_);
    impl_->ApplyBuiltInControlSurfaces();
    if (contentHwnd_) {
        SetPropW(contentHwnd_, L"DarkUiDialogContentBrush", impl_->brushContent);
        InvalidateRect(contentHwnd_, nullptr, TRUE);
    }
    if (dialogHwnd_) {
        InvalidateRect(dialogHwnd_, nullptr, TRUE);
    }
}

void Dialog::SetTitle(const std::wstring& title) {
    if (dialogHwnd_) {
        SetWindowTextW(dialogHwnd_, title.c_str());
        titleLabel_.SetText(title);
        InvalidateRect(dialogHwnd_, nullptr, FALSE);
    }
}

void Dialog::SetMessage(const std::wstring& text) {
    messageVisible_ = true;
    messageLabel_.SetText(text);
    if (dialogHwnd_) {
        impl_->LayoutChildren();
    }
}

std::wstring Dialog::GetMessage() const {
    return messageLabel_.GetText();
}

void Dialog::SetConfirmText(const std::wstring& text) {
    confirmButton_.SetText(text);
}

void Dialog::SetCancelText(const std::wstring& text) {
    cancelButton_.SetText(text);
}

void Dialog::SetMessageVisible(bool visible) {
    messageVisible_ = visible;
    if (messageLabel_.hwnd()) {
        ShowWindow(messageLabel_.hwnd(), visible ? SW_SHOW : SW_HIDE);
    }
}

void Dialog::SetCancelVisible(bool visible) {
    cancelVisible_ = visible;
    if (dialogHwnd_) {
        impl_->LayoutChildren();
    }
}

void Dialog::EndDialog(Result result) {
    modalResult_ = result;
    modalRunning_ = false;
    if (ownerHwnd_ && IsWindow(ownerHwnd_)) {
        EnableWindow(ownerHwnd_, TRUE);
        SetWindowPos(ownerHwnd_,
                     HWND_TOP,
                     0,
                     0,
                     0,
                     0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_NOACTIVATE);
        SetActiveWindow(ownerHwnd_);
    }
    if (dialogHwnd_) {
        ShowWindow(dialogHwnd_, SW_HIDE);
    }
}

Dialog::Result Dialog::ShowModal() {
    if (!dialogHwnd_) {
        return Result::None;
    }

    modalResult_ = Result::None;
    modalRunning_ = true;
    impl_->CenterToOwner();
    ShowWindow(dialogHwnd_, SW_SHOW);
    UpdateWindow(dialogHwnd_);
    SetActiveWindow(dialogHwnd_);

    const bool hadOwner = ownerHwnd_ && IsWindow(ownerHwnd_);
    if (hadOwner) {
        EnableWindow(ownerHwnd_, FALSE);
    }

    MSG msg{};
    while (modalRunning_ && IsWindow(dialogHwnd_) && ::GetMessageW(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessageW(dialogHwnd_, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (hadOwner) {
        EnableWindow(ownerHwnd_, TRUE);
        SetActiveWindow(ownerHwnd_);
    }

    return modalResult_;
}

}  // namespace darkui
