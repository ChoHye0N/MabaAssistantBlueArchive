// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Window capture with GDI: BitBlt (screen copy) or PrintWindow (window renders itself).
// Derived from the project owner's earlier project; see docs/SOURCES.md.
#pragma once

#ifdef _WIN32

#include <string>

#include <windows.h>

#include <opencv2/core.hpp>

namespace maba::win {

enum class CaptureMethod {
    BitBlt,       // copies the client area from the screen; the window must be visible and unobscured
    PrintWindow,  // asks the window to render itself; works when obscured, not when minimized
};

const char* ToString(CaptureMethod method);

// Captures the client area of a window at native resolution. Keeps the GDI memory DC and bitmap
// between calls while the size and method stay the same. Not thread-safe; one instance per thread.
// Call inside a DpiAwareScope so all coordinates are physical pixels.
class GdiCapture {
public:
    GdiCapture() = default;
    ~GdiCapture();
    GdiCapture(const GdiCapture&) = delete;
    GdiCapture& operator=(const GdiCapture&) = delete;

    // Writes a CV_8UC3 image of the client area to `out`. On failure returns false and sets `error`.
    bool Capture(HWND hwnd, CaptureMethod method, cv::Mat& out, std::string& error);

private:
    bool EnsureResources(HDC reference, CaptureMethod method, int width, int height);
    void Release();

    CaptureMethod method_ = CaptureMethod::BitBlt;
    int width_ = 0;
    int height_ = 0;
    HDC memory_dc_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    HGDIOBJ old_bitmap_ = nullptr;
};

}  // namespace maba::win

#endif  // _WIN32
