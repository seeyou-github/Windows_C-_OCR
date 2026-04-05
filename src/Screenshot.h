#pragma once

#include <string>
#include <windows.h>

namespace app {

struct ScreenshotResult {
    HBITMAP bitmap = nullptr;
    RECT captureRect {};
};

class ScreenshotService {
public:
    ScreenshotResult CaptureArea(const RECT& rect) const;
    bool SaveBitmapToBmp(HBITMAP bitmap, const std::wstring& filePath) const;
    bool BitmapToDataUrl(HBITMAP bitmap, std::string& dataUrl) const;
};

}  // namespace app
