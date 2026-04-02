@echo off
setlocal

chcp 65001 >nul
echo ===== Build Win32OCR =====

if not exist build mkdir build

set CXX=g++
set RC=windres
set CXXFLAGS=-std=c++17 -DUNICODE -D_UNICODE -finput-charset=UTF-8 -fexec-charset=UTF-8 -Isrc -Ilib_darkui\include
set LDFLAGS=-municode -mwindows -lcomdlg32 -lwininet -lcomctl32 -lshell32 -lole32 -loleaut32 -luuid -ldwmapi -luxtheme -lgdi32

%CXX% -c src\main.cpp %CXXFLAGS% -o build\main.o
if errorlevel 1 goto error
%CXX% -c src\MainWindow.cpp %CXXFLAGS% -o build\MainWindow.o
if errorlevel 1 goto error
%CXX% -c src\SettingsWindow.cpp %CXXFLAGS% -o build\SettingsWindow.o
if errorlevel 1 goto error
%CXX% -c src\AppTheme.cpp %CXXFLAGS% -o build\AppTheme.o
if errorlevel 1 goto error
%CXX% -c src\UiText.cpp %CXXFLAGS% -o build\UiText.o
if errorlevel 1 goto error
%CXX% -c src\AppConfig.cpp %CXXFLAGS% -o build\AppConfig.o
if errorlevel 1 goto error
%CXX% -c src\OcrService.cpp %CXXFLAGS% -o build\OcrService.o
if errorlevel 1 goto error

%CXX% -c lib_darkui\src\button.cpp %CXXFLAGS% -o build\dark_button.o
if errorlevel 1 goto error
%CXX% -c lib_darkui\src\checkbox.cpp %CXXFLAGS% -o build\dark_checkbox.o
if errorlevel 1 goto error
%CXX% -c lib_darkui\src\combobox.cpp %CXXFLAGS% -o build\dark_combobox.o
if errorlevel 1 goto error
%CXX% -c lib_darkui\src\dialog.cpp %CXXFLAGS% -o build\dark_dialog.o
if errorlevel 1 goto error
%CXX% -c lib_darkui\src\edit.cpp %CXXFLAGS% -o build\dark_edit.o
if errorlevel 1 goto error
%CXX% -c lib_darkui\src\listbox.cpp %CXXFLAGS% -o build\dark_listbox.o
if errorlevel 1 goto error
%CXX% -c lib_darkui\src\panel.cpp %CXXFLAGS% -o build\dark_panel.o
if errorlevel 1 goto error
%CXX% -c lib_darkui\src\progress.cpp %CXXFLAGS% -o build\dark_progress.o
if errorlevel 1 goto error
%CXX% -c lib_darkui\src\radiobutton.cpp %CXXFLAGS% -o build\dark_radiobutton.o
if errorlevel 1 goto error
%CXX% -c lib_darkui\src\scrollbar.cpp %CXXFLAGS% -o build\dark_scrollbar.o
if errorlevel 1 goto error
%CXX% -c lib_darkui\src\slider.cpp %CXXFLAGS% -o build\dark_slider.o
if errorlevel 1 goto error
%CXX% -c lib_darkui\src\static.cpp %CXXFLAGS% -o build\dark_static.o
if errorlevel 1 goto error
%CXX% -c lib_darkui\src\tab.cpp %CXXFLAGS% -o build\dark_tab.o
if errorlevel 1 goto error
%CXX% -c lib_darkui\src\themed_window_host.cpp %CXXFLAGS% -o build\dark_host.o
if errorlevel 1 goto error
%CXX% -c lib_darkui\src\toolbar.cpp %CXXFLAGS% -o build\dark_toolbar.o
if errorlevel 1 goto error

%RC% src\res\resource.rc -O coff -o build\resource.o
if errorlevel 1 goto error

echo Linking...
%CXX% ^
 build\main.o build\MainWindow.o build\SettingsWindow.o build\AppTheme.o build\UiText.o build\AppConfig.o build\OcrService.o ^
 build\dark_button.o build\dark_checkbox.o build\dark_combobox.o build\dark_dialog.o build\dark_edit.o build\dark_listbox.o build\dark_panel.o build\dark_progress.o build\dark_radiobutton.o build\dark_scrollbar.o build\dark_slider.o build\dark_static.o build\dark_tab.o build\dark_host.o build\dark_toolbar.o build\resource.o ^
 -o build\Win32OCR.exe %LDFLAGS%
if errorlevel 1 goto error

echo ===== Build succeeded =====
echo Output: build\Win32OCR.exe
goto end

:error
echo ===== Build failed =====
exit /b 1

:end
endlocal
