# Win32 OCR Demo

Minimal `Win32 API + C++` demo for SiliconFlow vision OCR.

UI:

- input `API Key`
- input `Model`
- choose a local image
- click `OCR`
- show recognized text

Build with MinGW-w64:

```bat
build.bat
```

or:

```bat
g++ -std=c++17 -municode -mwindows main.cpp -o Win32OCR.exe -lcomdlg32 -lwininet
```

Notes:

- The app sends a request to `https://api.siliconflow.cn/v1/chat/completions`
- Local images are converted to `data:image/...;base64,...`
- Default model in UI: `zai-org/GLM-4.5V`

Agent lessons:

- [供应商页切换闪烁复盘](docs/agent-lessons/provider-tab-flicker.md)
- [托盘图标无法点击复盘](docs/agent-lessons/tray-icon-not-clickable.md)
