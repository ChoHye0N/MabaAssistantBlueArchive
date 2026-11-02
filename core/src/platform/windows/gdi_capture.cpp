// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Derived from the project owner's earlier project (BitBlt/PrintWindow capture with a reused
// memory DC); see docs/SOURCES.md.
// Changes: the client origin comes from ClientToScreen instead of assuming symmetric borders,
// BitBlt copies only the client area, no global state, errors are reported.
#ifdef _WIN32

#include "gdi_capture.h"

#include <opencv2/imgproc.hpp>

#include "maba/geometry.h"

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

namespace maba::win {

namespace {

// PrintWindow flag: render DirectX/DWM content too (Windows 8.1+). Not always in older SDK headers.
constexpr UINT kPwRenderFullContent = 0x00000002;

}  // namespace

const char* ToString(CaptureMethod method) {
    return method == CaptureMethod::PrintWindow ? "printwindow" : "bitblt";
}

GdiCapture::~GdiCapture() { Release(); }

void GdiCapture::Release() {
    if (memory_dc_ && old_bitmap_) SelectObject(memory_dc_, old_bitmap_);
    if (bitmap_) DeleteObject(bitmap_);
    if (memory_dc_) DeleteDC(memory_dc_);
    memory_dc_ = nullptr;
    bitmap_ = nullptr;
    old_bitmap_ = nullptr;
    width_ = height_ = 0;
}

bool GdiCapture::EnsureResources(HDC reference, CaptureMethod method, int width, int height) {
    if (memory_dc_ && method_ == method && width_ == width && height_ == height) return true;
    Release();
    memory_dc_ = CreateCompatibleDC(reference);
    bitmap_ = CreateCompatibleBitmap(reference, width, height);
    if (!memory_dc_ || !bitmap_) {
        Release();
        return false;
    }
    old_bitmap_ = SelectObject(memory_dc_, bitmap_);
    method_ = method;
    width_ = width;
    height_ = height;
    return true;
}

bool GdiCapture::Capture(HWND hwnd, CaptureMethod method, cv::Mat& out, std::string& error) {
    RECT window_rect{};
    RECT client_rect{};
    POINT client_origin{0, 0};
    if (!GetWindowRect(hwnd, &window_rect) || !GetClientRect(hwnd, &client_rect) ||
        !ClientToScreen(hwnd, &client_origin)) {
        error = "cannot read window geometry";
        return false;
    }
    const Size client{client_rect.right - client_rect.left, client_rect.bottom - client_rect.top};
    const Rect window{window_rect.left, window_rect.top, window_rect.right - window_rect.left,
                      window_rect.bottom - window_rect.top};
    if (client.width <= 0 || client.height <= 0 || !IsValid(window)) {
        error = "window has no client area";
        return false;
    }

    // BitBlt copies the client area straight from the screen; PrintWindow renders the whole
    // window into a window-sized bitmap and the client area is cut out afterwards.
    const bool print = method == CaptureMethod::PrintWindow;
    const int bmp_w = print ? window.width : client.width;
    const int bmp_h = print ? window.height : client.height;

    HDC screen_dc = GetDC(nullptr);
    if (!screen_dc) {
        error = "GetDC failed";
        return false;
    }
    if (!EnsureResources(screen_dc, method, bmp_w, bmp_h)) {
        ReleaseDC(nullptr, screen_dc);
        error = "cannot create capture bitmap";
        return false;
    }

    BOOL ok = FALSE;
    if (print) {
        ok = PrintWindow(hwnd, memory_dc_, kPwRenderFullContent);
    } else {
        ok = BitBlt(memory_dc_, 0, 0, bmp_w, bmp_h, screen_dc, client_origin.x, client_origin.y,
                    SRCCOPY);
    }
    ReleaseDC(nullptr, screen_dc);
    if (!ok) {
        // The bitmap still holds the previous frame; never return it as a new one.
        Release();
        error = print ? "PrintWindow failed" : "BitBlt failed";
        return false;
    }

    cv::Mat bgra(bmp_h, bmp_w, CV_8UC4);
    BITMAPINFOHEADER bi{};
    bi.biSize = sizeof(bi);
    bi.biWidth = bmp_w;
    bi.biHeight = -bmp_h;  // top-down rows
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;
    // GetDIBits requires the bitmap not to be selected into a DC.
    SelectObject(memory_dc_, old_bitmap_);
    const int lines = GetDIBits(memory_dc_, bitmap_, 0, static_cast<UINT>(bmp_h), bgra.data,
                                reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS);
    SelectObject(memory_dc_, bitmap_);
    if (lines != bmp_h) {
        Release();
        error = "GetDIBits failed";
        return false;
    }

    cv::Mat bgr;
    cv::cvtColor(bgra, bgr, cv::COLOR_BGRA2BGR);
    if (!print) {
        out = bgr;
        return true;
    }
    const Rect crop = ClientCropRect(window, {client_origin.x, client_origin.y}, client);
    if (crop.width != client.width || crop.height != client.height) {
        error = "client area lies outside the window bitmap";
        return false;
    }
    out = bgr(cv::Rect(crop.x, crop.y, crop.width, crop.height)).clone();
    return true;
}

}  // namespace maba::win

#endif  // _WIN32
