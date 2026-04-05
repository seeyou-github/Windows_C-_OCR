#include "CaptureOverlay.h"

#include <algorithm>
#include <windowsx.h>

namespace app {

namespace {

constexpr wchar_t kOverlayClassName[] = L"Win32OcrCaptureOverlay";

HBITMAP CaptureDesktopSnapshot(const RECT& area) {
    const int width = area.right - area.left;
    const int height = area.bottom - area.top;
    HDC screenDc = GetDC(nullptr);
    HDC memoryDc = CreateCompatibleDC(screenDc);
    HBITMAP bitmap = CreateCompatibleBitmap(screenDc, width, height);
    if (!screenDc || !memoryDc || !bitmap) {
        if (bitmap) DeleteObject(bitmap);
        if (memoryDc) DeleteDC(memoryDc);
        if (screenDc) ReleaseDC(nullptr, screenDc);
        return nullptr;
    }
    HGDIOBJ oldObject = SelectObject(memoryDc, bitmap);
    BitBlt(memoryDc, 0, 0, width, height, screenDc, area.left, area.top, SRCCOPY | CAPTUREBLT);
    SelectObject(memoryDc, oldObject);
    DeleteDC(memoryDc);
    ReleaseDC(nullptr, screenDc);
    return bitmap;
}

}  // namespace

CaptureOverlay::CaptureOverlay(HINSTANCE instance)
    : instance_(instance) {
}

bool CaptureOverlay::SelectArea(HWND owner, RECT* selectedRect) {
    if (!EnsureClassRegistered()) {
        return false;
    }

    virtualScreen_.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    virtualScreen_.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    virtualScreen_.right = virtualScreen_.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
    virtualScreen_.bottom = virtualScreen_.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);

    if (backgroundBitmap_) {
        DeleteObject(backgroundBitmap_);
        backgroundBitmap_ = nullptr;
    }
    if (paintBufferBitmap_) {
        DeleteObject(paintBufferBitmap_);
        paintBufferBitmap_ = nullptr;
    }
    backgroundBitmap_ = CaptureDesktopSnapshot(virtualScreen_);

    selection_ = {};
    dragging_ = false;
    accepted_ = false;
    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        kOverlayClassName,
        L"",
        WS_POPUP,
        virtualScreen_.left,
        virtualScreen_.top,
        virtualScreen_.right - virtualScreen_.left,
        virtualScreen_.bottom - virtualScreen_.top,
        owner,
        nullptr,
        instance_,
        this);
    if (!hwnd_) {
        if (backgroundBitmap_) {
            DeleteObject(backgroundBitmap_);
            backgroundBitmap_ = nullptr;
        }
        return false;
    }

    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    SetWindowPos(hwnd_,
                 HWND_TOPMOST,
                 virtualScreen_.left,
                 virtualScreen_.top,
                 virtualScreen_.right - virtualScreen_.left,
                 virtualScreen_.bottom - virtualScreen_.top,
                 SWP_SHOWWINDOW | SWP_NOACTIVATE);
    SetCursor(LoadCursorW(nullptr, IDC_CROSS));

    MSG msg {};
    while (IsWindow(hwnd_)) {
        if (PeekMessageW(&msg, nullptr, WM_QUIT, WM_QUIT, PM_NOREMOVE)) {
            accepted_ = false;
            break;
        }
        const BOOL status = GetMessageW(&msg, nullptr, 0, 0);
        if (status <= 0) {
            accepted_ = false;
            break;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    ReleaseCapture();
    if (accepted_ && selectedRect) {
        *selectedRect = NormalizeRect(selection_);
    }
    return accepted_;
}

LRESULT CALLBACK CaptureOverlay::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<CaptureOverlay*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<CaptureOverlay*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    return self ? self->HandleMessage(hwnd, message, wParam, lParam) : DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT CaptureOverlay::HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_SETCURSOR:
        SetCursor(LoadCursorW(nullptr, IDC_CROSS));
        return TRUE;
    case WM_MOUSEACTIVATE:
        return MA_ACTIVATE;
    case WM_ERASEBKGND:
        return 1;
    case WM_LBUTTONDOWN:
        startPoint_ = {GET_X_LPARAM(lParam) + virtualScreen_.left, GET_Y_LPARAM(lParam) + virtualScreen_.top};
        selection_ = {startPoint_.x, startPoint_.y, startPoint_.x, startPoint_.y};
        dragging_ = true;
        SetCapture(hwnd);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_MOUSEMOVE:
        if (dragging_) {
            POINT pt {GET_X_LPARAM(lParam) + virtualScreen_.left, GET_Y_LPARAM(lParam) + virtualScreen_.top};
            UpdateSelection(pt);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_LBUTTONUP:
        if (dragging_) {
            POINT pt {GET_X_LPARAM(lParam) + virtualScreen_.left, GET_Y_LPARAM(lParam) + virtualScreen_.top};
            UpdateSelection(pt);
            dragging_ = false;
            ReleaseCapture();
            accepted_ = selection_.left != selection_.right && selection_.top != selection_.bottom;
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
        accepted_ = false;
        if (GetCapture() == hwnd) {
            ReleaseCapture();
        }
        DestroyWindow(hwnd);
        return 0;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            accepted_ = false;
            if (GetCapture() == hwnd) {
                ReleaseCapture();
            }
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_PAINT:
        Paint(hwnd);
        return 0;
    case WM_DESTROY:
        if (backgroundBitmap_) {
            DeleteObject(backgroundBitmap_);
            backgroundBitmap_ = nullptr;
        }
        if (paintBufferBitmap_) {
            DeleteObject(paintBufferBitmap_);
            paintBufferBitmap_ = nullptr;
        }
        hwnd_ = nullptr;
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

RECT CaptureOverlay::NormalizeRect(const RECT& rect) const {
    RECT normalized = rect;
    if (normalized.left > normalized.right) {
        std::swap(normalized.left, normalized.right);
    }
    if (normalized.top > normalized.bottom) {
        std::swap(normalized.top, normalized.bottom);
    }
    return normalized;
}

void CaptureOverlay::UpdateSelection(POINT point) {
    selection_.left = startPoint_.x;
    selection_.top = startPoint_.y;
    selection_.right = point.x;
    selection_.bottom = point.y;
}

void CaptureOverlay::Paint(HWND hwnd) {
    PAINTSTRUCT ps {};
    HDC dc = BeginPaint(hwnd, &ps);
    RECT client {};
    GetClientRect(hwnd, &client);

    HDC bufferDc = CreateCompatibleDC(dc);
    if (!bufferDc) {
        EndPaint(hwnd, &ps);
        return;
    }

    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    if (!paintBufferBitmap_) {
        paintBufferBitmap_ = CreateCompatibleBitmap(dc, width, height);
    }
    if (!paintBufferBitmap_) {
        DeleteDC(bufferDc);
        EndPaint(hwnd, &ps);
        return;
    }

    HGDIOBJ oldBuffer = SelectObject(bufferDc, paintBufferBitmap_);
    if (backgroundBitmap_) {
        HDC backgroundDc = CreateCompatibleDC(dc);
        HGDIOBJ oldBackground = SelectObject(backgroundDc, backgroundBitmap_);
        BitBlt(bufferDc, 0, 0, width, height, backgroundDc, 0, 0, SRCCOPY);
        SelectObject(backgroundDc, oldBackground);
        DeleteDC(backgroundDc);
    } else {
        HBRUSH brush = CreateSolidBrush(RGB(20, 20, 20));
        FillRect(bufferDc, &client, brush);
        DeleteObject(brush);
    }

    RECT selected = NormalizeRect(selection_);
    RECT local {
        selected.left - virtualScreen_.left,
        selected.top - virtualScreen_.top,
        selected.right - virtualScreen_.left,
        selected.bottom - virtualScreen_.top
    };
    if (local.left != local.right && local.top != local.bottom) {
        HPEN pen = CreatePen(PS_SOLID, 2, RGB(88, 156, 255));
        HGDIOBJ oldPen = SelectObject(bufferDc, pen);
        HGDIOBJ oldBrush = SelectObject(bufferDc, GetStockObject(HOLLOW_BRUSH));
        Rectangle(bufferDc, local.left, local.top, local.right, local.bottom);
        SelectObject(bufferDc, oldBrush);
        SelectObject(bufferDc, oldPen);
        DeleteObject(pen);
    }

    BitBlt(dc, 0, 0, width, height, bufferDc, 0, 0, SRCCOPY);
    SelectObject(bufferDc, oldBuffer);
    DeleteDC(bufferDc);
    EndPaint(hwnd, &ps);
}

bool CaptureOverlay::EnsureClassRegistered() {
    if (classRegistered_) {
        return true;
    }
    WNDCLASSW existing {};
    if (GetClassInfoW(instance_, kOverlayClassName, &existing) ||
        GetClassInfoW(GetModuleHandleW(nullptr), kOverlayClassName, &existing)) {
        classRegistered_ = true;
        return true;
    }
    WNDCLASSW wc {};
    wc.lpfnWndProc = CaptureOverlay::WindowProc;
    wc.hInstance = instance_;
    wc.hCursor = LoadCursorW(nullptr, IDC_CROSS);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = kOverlayClassName;
    SetLastError(ERROR_SUCCESS);
    const ATOM atom = RegisterClassW(&wc);
    if (atom != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS) {
        classRegistered_ = true;
        return true;
    }
    return classRegistered_;
}

}  // namespace app
