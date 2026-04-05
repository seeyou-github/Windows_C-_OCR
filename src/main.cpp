#include "MainWindow.h"
#include "UiText.h"
#include "res/resource.h"

#include <commctrl.h>
#include <windows.h>

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES | ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icc);

    app::SetResourceInstance(instance);

    HANDLE singleInstance = CreateMutexW(nullptr, FALSE, L"Global\\Win32OCR_SingleInstance_Mutex");
    if (!singleInstance) {
        MessageBoxW(nullptr,
                    app::LoadStringResource(IDS_MSG_SINGLE_INSTANCE_CREATE_FAILED).c_str(),
                    app::LoadStringResource(IDS_APP_TITLE).c_str(),
                    MB_OK | MB_ICONERROR);
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr,
                    app::LoadStringResource(IDS_MSG_ALREADY_RUNNING).c_str(),
                    app::LoadStringResource(IDS_APP_TITLE).c_str(),
                    MB_OK | MB_ICONINFORMATION);
        CloseHandle(singleInstance);
        return 0;
    }

    app::MainWindow mainWindow;
    if (!mainWindow.Create(instance)) {
        MessageBoxW(nullptr,
                    app::LoadStringResource(IDS_MSG_APP_INIT_FAILED).c_str(),
                    app::LoadStringResource(IDS_APP_TITLE).c_str(),
                    MB_OK | MB_ICONERROR);
        if (singleInstance) {
            CloseHandle(singleInstance);
        }
        return 1;
    }

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    if (singleInstance) {
        CloseHandle(singleInstance);
    }
    return static_cast<int>(message.wParam);
}


