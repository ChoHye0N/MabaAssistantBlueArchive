// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// IController for the Blue Archive Steam PC client (global): GDI window capture + real mouse input.
#pragma once

#ifdef _WIN32

#include <string>

#include <windows.h>

#include <opencv2/core.hpp>

#include "gdi_capture.h"
#include "maba/controller.h"
#include "maba/log.h"

namespace maba::win {

struct SteamControllerOptions {
    CaptureMethod capture = CaptureMethod::BitBlt;
    // Window lookup. Values used by the earlier project; verify against the current client.
    std::wstring window_class = L"UnityWndClass";
    std::wstring window_title = L"Blue Archive";
    bool bring_to_front = true;  // SetForegroundWindow before input
    bool fit_window = false;     // Connect changes a window that is not 16:9 or too small to 1280x720
    LogFn log;
    bool restore_cursor = true;  // put the cursor back after each click/swipe
    int move_settle_ms = 100;    // pause after moving, before pressing (the game reads the
                                 // cursor once per frame; a quicker press lands at the old spot)
    int click_hold_ms = 60;      // button held down
    int swipe_step_ms = 16;      // time between swipe moves
};

class SteamController : public IController {
public:
    explicit SteamController(SteamControllerOptions options = {});

    // Finds the game window. Called automatically by every operation when needed.
    bool Connect();
    HWND Window() const { return hwnd_; }
    const SteamControllerOptions& Options() const { return options_; }
    void SetCaptureMethod(CaptureMethod method) { options_.capture = method; }

    // Client size of the last successful operation (physical pixels).
    Size NativeSize() const override { return native_; }
    Frame Screenshot() override;
    Frame CaptureRegion(Rect normalized) override;
    bool Click(Point normalized) override;
    bool Swipe(Point from, Point to, int duration_ms) override;
    bool Scroll(Point normalized, int notches) override;
    bool Back() override;  // Esc key. Verify in the game that Esc acts as "back" on every page.

    // Why the last operation failed (empty after a success).
    const std::string& LastError() const { return last_error_; }
    std::string Error() const override { return last_error_; }

private:
    bool EnsureWindow();
    void FitWindow();
    bool CaptureNative(cv::Mat& out);
    bool PrepareInput();
    bool ToScreen(Point normalized, POINT& screen);
    bool Fail(std::string message);

    SteamControllerOptions options_;
    HWND hwnd_ = nullptr;
    Size native_{};
    GdiCapture capture_;
    std::string last_error_;
};

}  // namespace maba::win

#endif  // _WIN32
