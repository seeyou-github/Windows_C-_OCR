#include "AppTheme.h"

#include <algorithm>

namespace app {

darkui::Theme MakeAppTheme(const std::wstring& themeName, int fontSize) {
    darkui::Theme theme;
    if (themeName == L"moss") {
        theme.background = RGB(22, 28, 24);
        theme.panel = RGB(32, 41, 35);
        theme.border = RGB(58, 74, 62);
        theme.text = RGB(231, 239, 233);
        theme.mutedText = RGB(150, 165, 154);
        theme.button = RGB(49, 68, 56);
        theme.buttonHover = RGB(60, 82, 67);
        theme.buttonHot = RGB(85, 120, 96);
        theme.editBackground = RGB(38, 53, 44);
        theme.editText = RGB(238, 246, 240);
        theme.editPlaceholder = RGB(132, 149, 139);
        theme.toolbarBackground = RGB(20, 26, 22);
        theme.toolbarItem = RGB(42, 57, 48);
        theme.toolbarItemHot = RGB(56, 74, 62);
        theme.toolbarItemActive = RGB(80, 111, 89);
        theme.toolbarText = RGB(232, 239, 234);
        theme.toolbarTextActive = RGB(248, 250, 248);
        theme.toolbarSeparator = RGB(70, 86, 74);
        theme.tabBackground = RGB(22, 29, 24);
        theme.tabItem = RGB(39, 53, 44);
        theme.tabItemActive = RGB(75, 105, 84);
        theme.tabText = RGB(214, 221, 216);
        theme.tabTextActive = RGB(246, 249, 247);
        theme.listBoxBackground = theme.background;
        theme.listBoxPanel = theme.editBackground;
        theme.listBoxText = theme.text;
        theme.listBoxItemSelected = RGB(76, 104, 83);
        theme.listBoxItemSelectedText = RGB(247, 250, 248);
    } else if (themeName == L"mono") {
        theme.background = RGB(20, 20, 22);
        theme.panel = RGB(31, 32, 35);
        theme.border = RGB(62, 64, 69);
        theme.text = RGB(237, 238, 241);
        theme.mutedText = RGB(152, 155, 162);
        theme.button = RGB(53, 56, 61);
        theme.buttonHover = RGB(67, 70, 76);
        theme.buttonHot = RGB(95, 99, 108);
        theme.editBackground = RGB(38, 40, 44);
        theme.editText = RGB(243, 244, 247);
        theme.editPlaceholder = RGB(133, 137, 145);
        theme.toolbarBackground = RGB(18, 18, 20);
        theme.toolbarItem = RGB(44, 46, 50);
        theme.toolbarItemHot = RGB(60, 63, 69);
        theme.toolbarItemActive = RGB(87, 91, 99);
        theme.toolbarText = RGB(235, 236, 239);
        theme.toolbarTextActive = RGB(250, 250, 251);
        theme.toolbarSeparator = RGB(72, 74, 80);
        theme.tabBackground = RGB(21, 21, 23);
        theme.tabItem = RGB(39, 41, 45);
        theme.tabItemActive = RGB(80, 83, 90);
        theme.tabText = RGB(215, 217, 221);
        theme.tabTextActive = RGB(247, 248, 250);
        theme.listBoxBackground = theme.background;
        theme.listBoxPanel = theme.editBackground;
        theme.listBoxText = theme.text;
        theme.listBoxItemSelected = RGB(83, 87, 94);
        theme.listBoxItemSelectedText = RGB(248, 249, 250);
    } else {
        theme.background = RGB(20, 22, 26);
        theme.panel = RGB(31, 34, 40);
        theme.border = RGB(50, 55, 64);
        theme.text = RGB(236, 239, 244);
        theme.mutedText = RGB(147, 155, 166);
        theme.button = RGB(58, 64, 74);
        theme.buttonHover = RGB(69, 76, 87);
        theme.buttonHot = RGB(83, 110, 150);
        theme.editBackground = RGB(37, 41, 48);
        theme.editText = RGB(239, 242, 247);
        theme.editPlaceholder = RGB(127, 136, 148);
        theme.toolbarBackground = RGB(24, 27, 31);
        theme.toolbarItem = RGB(46, 51, 58);
        theme.toolbarItemHot = RGB(64, 71, 82);
        theme.toolbarItemActive = RGB(78, 120, 184);
        theme.toolbarText = RGB(228, 232, 238);
        theme.toolbarTextActive = RGB(248, 250, 252);
        theme.toolbarSeparator = RGB(70, 76, 86);
        theme.tabBackground = RGB(24, 27, 31);
        theme.tabItem = RGB(48, 53, 60);
        theme.tabItemActive = RGB(78, 120, 184);
        theme.tabText = RGB(206, 211, 218);
        theme.tabTextActive = RGB(245, 247, 250);
        theme.listBoxBackground = theme.background;
        theme.listBoxPanel = theme.editBackground;
        theme.listBoxText = theme.text;
        theme.listBoxItemSelected = RGB(78, 120, 184);
        theme.listBoxItemSelectedText = RGB(245, 247, 250);
    }

    theme.staticBackground = theme.background;
    theme.staticText = theme.text;
    theme.tableBackground = theme.editBackground;
    theme.tableText = theme.editText;
    theme.tableHeaderBackground = theme.panel;
    theme.tableHeaderText = theme.text;
    theme.tableGrid = theme.border;
    theme.uiFont.family = L"Segoe UI";
    theme.uiFont.height = -std::max(16, fontSize);
    theme.textPadding = 12;
    theme.toolbarHeight = 36;
    theme.tabHeight = 38;
    theme.tabWidth = 168;
    theme.tableRowHeight = 28;
    theme.tableHeaderHeight = 32;
    theme.scrollBarThickness = 14;
    return theme;
}

}  // namespace app
