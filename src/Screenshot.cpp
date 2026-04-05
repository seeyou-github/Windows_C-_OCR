#include "Screenshot.h"

#include <gdiplus.h>
#include <objidl.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace app {

namespace {

std::string Base64Encode(const std::vector<BYTE>& data) {
    static const char kTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string output;
    output.reserve(((data.size() + 2) / 3) * 4);
    for (std::size_t i = 0; i < data.size(); i += 3) {
        const unsigned int b0 = data[i];
        const unsigned int b1 = (i + 1 < data.size()) ? data[i + 1] : 0;
        const unsigned int b2 = (i + 2 < data.size()) ? data[i + 2] : 0;
        const unsigned int triple = (b0 << 16) | (b1 << 8) | b2;
        output.push_back(kTable[(triple >> 18) & 0x3F]);
        output.push_back(kTable[(triple >> 12) & 0x3F]);
        output.push_back((i + 1 < data.size()) ? kTable[(triple >> 6) & 0x3F] : '=');
        output.push_back((i + 2 < data.size()) ? kTable[triple & 0x3F] : '=');
    }
    return output;
}

int GetEncoderClsid(const wchar_t* mimeType, CLSID* clsid) {
    UINT count = 0;
    UINT size = 0;
    Gdiplus::GetImageEncodersSize(&count, &size);
    if (size == 0) {
        return -1;
    }
    std::vector<BYTE> buffer(size);
    auto* codecs = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buffer.data());
    if (Gdiplus::GetImageEncoders(count, size, codecs) != Gdiplus::Ok) {
        return -1;
    }
    for (UINT i = 0; i < count; ++i) {
        if (wcscmp(codecs[i].MimeType, mimeType) == 0) {
            *clsid = codecs[i].Clsid;
            return static_cast<int>(i);
        }
    }
    return -1;
}

class GdiplusScope {
public:
    GdiplusScope() {
        Gdiplus::GdiplusStartupInput input;
        started_ = (Gdiplus::GdiplusStartup(&token_, &input, nullptr) == Gdiplus::Ok);
    }
    ~GdiplusScope() {
        if (started_) {
            Gdiplus::GdiplusShutdown(token_);
        }
    }
    bool started() const { return started_; }

private:
    ULONG_PTR token_ = 0;
    bool started_ = false;
};

bool ExtractBitmapPixels(HBITMAP bitmap, BITMAPINFOHEADER& info, std::vector<BYTE>& pixels) {
    BITMAP bm {};
    if (GetObjectW(bitmap, sizeof(bm), &bm) == 0) {
        return false;
    }
    info = {};
    info.biSize = sizeof(BITMAPINFOHEADER);
    info.biWidth = bm.bmWidth;
    info.biHeight = bm.bmHeight;
    info.biPlanes = 1;
    info.biBitCount = 32;
    info.biCompression = BI_RGB;

    const DWORD imageSize = static_cast<DWORD>(bm.bmWidth * bm.bmHeight * 4);
    pixels.resize(imageSize);
    BITMAPINFO dibInfo {};
    dibInfo.bmiHeader = info;
    HDC dc = GetDC(nullptr);
    const int lines = GetDIBits(dc, bitmap, 0, static_cast<UINT>(bm.bmHeight), pixels.data(), &dibInfo, DIB_RGB_COLORS);
    ReleaseDC(nullptr, dc);
    return lines != 0;
}

}  // namespace

ScreenshotResult ScreenshotService::CaptureArea(const RECT& rect) const {
    ScreenshotResult result {};
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0) {
        return result;
    }

    HDC screenDc = GetDC(nullptr);
    HDC memoryDc = CreateCompatibleDC(screenDc);
    HBITMAP bitmap = CreateCompatibleBitmap(screenDc, width, height);
    if (!screenDc || !memoryDc || !bitmap) {
        if (bitmap) DeleteObject(bitmap);
        if (memoryDc) DeleteDC(memoryDc);
        if (screenDc) ReleaseDC(nullptr, screenDc);
        return result;
    }

    HGDIOBJ oldObject = SelectObject(memoryDc, bitmap);
    const BOOL copied = BitBlt(memoryDc, 0, 0, width, height, screenDc, rect.left, rect.top, SRCCOPY | CAPTUREBLT);
    SelectObject(memoryDc, oldObject);
    DeleteDC(memoryDc);
    ReleaseDC(nullptr, screenDc);

    if (!copied) {
        DeleteObject(bitmap);
        return result;
    }

    result.bitmap = bitmap;
    result.captureRect = rect;
    return result;
}

bool ScreenshotService::SaveBitmapToBmp(HBITMAP bitmap, const std::wstring& filePath) const {
    if (!bitmap) {
        return false;
    }

    BITMAPINFOHEADER info {};
    std::vector<BYTE> pixels;
    if (!ExtractBitmapPixels(bitmap, info, pixels)) {
        return false;
    }
    const DWORD imageSize = static_cast<DWORD>(pixels.size());

    BITMAPFILEHEADER fileHeader {};
    fileHeader.bfType = 0x4D42;  // BM
    fileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    fileHeader.bfSize = fileHeader.bfOffBits + imageSize;

    std::ofstream out(std::filesystem::path(filePath), std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }
    out.write(reinterpret_cast<const char*>(&fileHeader), sizeof(fileHeader));
    out.write(reinterpret_cast<const char*>(&info), sizeof(info));
    out.write(reinterpret_cast<const char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
    return out.good();
}

bool ScreenshotService::BitmapToDataUrl(HBITMAP bitmap, std::string& dataUrl) const {
    if (!bitmap) {
        return false;
    }

    GdiplusScope gdiplus;
    if (!gdiplus.started()) {
        return false;
    }

    CLSID encoderClsid {};
    if (GetEncoderClsid(L"image/jpeg", &encoderClsid) < 0) {
        return false;
    }

    Gdiplus::Bitmap image(bitmap, nullptr);
    if (image.GetLastStatus() != Gdiplus::Ok) {
        return false;
    }

    IStream* stream = nullptr;
    if (CreateStreamOnHGlobal(nullptr, TRUE, &stream) != S_OK || !stream) {
        return false;
    }

    Gdiplus::EncoderParameters params {};
    params.Count = 1;
    params.Parameter[0].Guid = Gdiplus::EncoderQuality;
    params.Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
    params.Parameter[0].NumberOfValues = 1;
    ULONG quality = 92;
    params.Parameter[0].Value = &quality;

    const bool saveOk = (image.Save(stream, &encoderClsid, &params) == Gdiplus::Ok);
    if (!saveOk) {
        stream->Release();
        return false;
    }

    HGLOBAL hGlobal = nullptr;
    if (GetHGlobalFromStream(stream, &hGlobal) != S_OK || !hGlobal) {
        stream->Release();
        return false;
    }
    const SIZE_T size = GlobalSize(hGlobal);
    const BYTE* data = static_cast<const BYTE*>(GlobalLock(hGlobal));
    if (!data || size == 0) {
        if (data) {
            GlobalUnlock(hGlobal);
        }
        stream->Release();
        return false;
    }
    std::vector<BYTE> jpeg(data, data + size);
    GlobalUnlock(hGlobal);
    stream->Release();

    dataUrl = "data:image/jpeg;base64," + Base64Encode(jpeg);
    return true;
}

}  // namespace app
