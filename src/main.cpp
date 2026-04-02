#include "MainWindow.h"
#include "UiText.h"

#include <commctrl.h>
#include <windows.h>

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES | ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icc);

    app::SetResourceInstance(instance);

    app::MainWindow mainWindow;
    if (!mainWindow.Create(instance)) {
        return 0;
    }

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}
