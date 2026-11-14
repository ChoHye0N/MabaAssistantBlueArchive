// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#ifdef _WIN32

#include "steam_controller.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "display_info.h"
#include "dpi_scope.h"
#include "maba/coords.h"
#include "maba/geometry.h"
#include "maba/vision.h"
#include "send_input.h"

namespace maba::win {

namespace {

constexpr WORD kScanEsc = 0x01;
constexpr int kMaxPauseMs = 2000;
constexpr int kMaxSwipeMs = 5000;
constexpr int kWheelStepMs = 150;
constexpr int kForegroundSettleMs = 80;

void Pause(int ms) { ::Sleep(static_cast<DWORD>(std::clamp(ms, 0, kMaxPauseMs))); }

HWND RootOf(HWND hwnd) { return hwnd ? GetAncestor(hwnd, GA_ROOT) : nullptr; }

// PrintWindow returns an all-black image for some DirectX windows; treat that as a failure.
bool IsBlack(const cv::Mat& bgr) {
    double max_value = 0;
    cv::minMaxLoc(bgr.reshape(1), nullptr, &max_value);
    return max_value < 8;
}

class CursorRestore {
public:
    explicit CursorRestore(bool enabled) : enabled_(enabled && GetCursorPos(&saved_)) {}
    ~CursorRestore() {
        if (enabled_) MoveCursorTo(saved_);
    }

private:
    POINT saved_{};
    bool enabled_;
};

}  // namespace

SteamController::SteamController(SteamControllerOptions options) : options_(std::move(options)) {}

bool SteamController::Fail(std::string message) {
    last_error_ = std::move(message);
    return false;
}

bool SteamController::Connect() {
    const wchar_t* cls = options_.window_class.empty() ? nullptr : options_.window_class.c_str();
    const wchar_t* title = options_.window_title.empty() ? nullptr : options_.window_title.c_str();
    hwnd_ = FindWindowW(cls, title);
    if (!hwnd_) return Fail("game window not found (is the Steam client running?)");
    FitWindow();
    if (WindowOnHdrDisplay(hwnd_))
        Log(options_.log, LogLevel::kWarn,
            "HDR is on for the display of the game window: captured colours can differ and colour checks "
            "may fail. Turn HDR off in the Windows display settings");
    last_error_.clear();
    return true;
}

// On connecting, and again before an action when the window no longer fits (a game that has just
// started sizes its window once more).
void SteamController::FitWindow() {
    DpiAwareScope dpi;
    if (IsIconic(hwnd_)) return;
    const auto client_size = [this] {
        RECT rc{};
        GetClientRect(hwnd_, &rc);
        return Size{rc.right - rc.left, rc.bottom - rc.top};
    };
    Size size = client_size();
    if (!NeedsWindowFit(size)) return;
    const std::string old = std::to_string(size.width) + "x" + std::to_string(size.height);
    if (!options_.fit_window) {
        Log(options_.log, LogLevel::kWarn, "the game window is " + old + ": use 16:9, at least 1280x720");
        return;
    }
    if (IsZoomed(hwnd_)) {
        ShowWindow(hwnd_, SW_RESTORE);
        Sleep(300);
        size = client_size();
    }
    // The frame around the client area is measured, so this holds at any display scale.
    RECT outer{};
    if (!GetWindowRect(hwnd_, &outer)) return;
    const int width = outer.right - outer.left - size.width + kNormWidth;
    const int height = outer.bottom - outer.top - size.height + kNormHeight;
    SetWindowPos(hwnd_, nullptr, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    for (int i = 0; i < 20; ++i) {
        Sleep(100);
        size = client_size();
        if (size.width == kNormWidth && size.height == kNormHeight) break;
    }
    if (NeedsWindowFit(size)) {
        Log(options_.log, LogLevel::kWarn, "could not change the game window from " + old);
        return;
    }
    Sleep(500);  // the game lays its screen out again
    Log(options_.log, LogLevel::kInfo, "game window changed from " + old + " to 1280x720");
}

bool SteamController::EnsureWindow() {
    if (!hwnd_ || !IsWindow(hwnd_)) {
        if (!Connect()) return false;
    }
    if (IsIconic(hwnd_)) return Fail("game window is minimized");
    RECT rc{};
    if (!GetClientRect(hwnd_, &rc)) return Fail("cannot read the client size");
    Size size{rc.right - rc.left, rc.bottom - rc.top};
    // A game started by this run sizes its window again after the window first appears (seen
    // 2026-11-13: 1468x856 after the title screen), so a window that stopped fitting is fitted again.
    if (!IsSupportedAspect(size) && options_.fit_window) {
        FitWindow();
        if (!GetClientRect(hwnd_, &rc)) return Fail("cannot read the client size");
        size = Size{rc.right - rc.left, rc.bottom - rc.top};
    }
    if (!IsSupportedAspect(size)) {
        return Fail("client area " + std::to_string(size.width) + "x" + std::to_string(size.height) +
                    " is not 16:9; set a 16:9 window, at least 1280x720");
    }
    native_ = size;
    return true;
}

bool SteamController::CaptureNative(cv::Mat& out) {
    DpiAwareScope dpi;
    if (!EnsureWindow()) return false;
    std::string error;
    if (!capture_.Capture(hwnd_, options_.capture, out, error)) {
        return Fail(std::string(ToString(options_.capture)) + ": " + error);
    }
    if (out.cols != native_.width || out.rows != native_.height) {
        return Fail("window size changed during capture");
    }
    if (IsBlack(out)) {
        return Fail(std::string(ToString(options_.capture)) +
                    ": captured an all-black frame (try the other capture method)");
    }
    last_error_.clear();
    return true;
}

Frame SteamController::Screenshot() {
    cv::Mat native;
    if (!CaptureNative(native)) return {};
    return NormalizeFrame(native);
}

Frame SteamController::CaptureRegion(Rect normalized) {
    cv::Mat native;
    if (!CaptureNative(native)) return {};
    const Rect r = NormalizedRectToNative(normalized, native_);
    if (!IsValid(r)) {
        Fail("region lies outside the screen");
        return {};
    }
    return FrameFromMat(native(cv::Rect(r.x, r.y, r.width, r.height)));
}

bool SteamController::PrepareInput() {
    if (!EnsureWindow()) return false;
    if (options_.bring_to_front && GetForegroundWindow() != hwnd_) {
        if (IsIconic(hwnd_)) ShowWindow(hwnd_, SW_RESTORE);
        SetForegroundWindow(hwnd_);
        Pause(kForegroundSettleMs);
        if (RootOf(GetForegroundWindow()) != RootOf(hwnd_)) {
            // Windows refuses focus changes from a background process unless it just received
            // input; a lone Alt press counts as such input and has no effect in the game.
            INPUT alt[2] = {};
            alt[0].type = alt[1].type = INPUT_KEYBOARD;
            alt[0].ki.wVk = alt[1].ki.wVk = VK_MENU;
            alt[1].ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(2, alt, sizeof(INPUT));
            SetForegroundWindow(hwnd_);
            Pause(kForegroundSettleMs);
        }
    }
    // Real mouse input goes to whatever window is in front: refuse instead of clicking elsewhere.
    if (RootOf(GetForegroundWindow()) != RootOf(hwnd_)) {
        return Fail("game window is not in the foreground; click the game once and retry");
    }
    return true;
}

bool SteamController::ToScreen(Point normalized, POINT& screen) {
    const Point native = ToNative(normalized, native_);
    screen = {native.x, native.y};
    if (!ClientToScreen(hwnd_, &screen)) return Fail("ClientToScreen failed");
    // Another top-level window (overlay, notification) over the target point would get the click.
    if (RootOf(WindowFromPoint(screen)) != RootOf(hwnd_)) {
        return Fail("another window covers the target point");
    }
    return true;
}

bool SteamController::Click(Point normalized) {
    DpiAwareScope dpi;
    if (!PrepareInput()) return false;
    POINT pt{};
    if (!ToScreen(normalized, pt)) return false;

    CursorRestore restore(options_.restore_cursor);
    if (!MoveCursorTo(pt)) return Fail("SendInput move failed (is the game running as administrator?)");
    Pause(options_.move_settle_ms);
    if (!LeftButton(true)) return Fail("SendInput button down failed");
    Pause(options_.click_hold_ms);
    const bool up = LeftButton(false);
    Pause(options_.move_settle_ms);
    if (!up) return Fail("SendInput button up failed");
    last_error_.clear();
    return true;
}

bool SteamController::Scroll(Point normalized, int notches) {
    DpiAwareScope dpi;
    if (!PrepareInput()) return false;
    POINT pt{};
    if (!ToScreen(normalized, pt)) return false;

    CursorRestore restore(options_.restore_cursor);
    if (!MoveCursorTo(pt)) return Fail("SendInput move failed (is the game running as administrator?)");
    Pause(options_.move_settle_ms);
    const int step = notches < 0 ? -1 : 1;
    for (int i = 0; i < std::abs(notches); ++i) {
        if (!WheelNotch(step)) return Fail("SendInput wheel failed");
        Pause(kWheelStepMs);  // one notch per frame or two, or the game drops some
    }
    last_error_.clear();
    return true;
}

bool SteamController::Swipe(Point from, Point to, int duration_ms) {
    DpiAwareScope dpi;
    if (!PrepareInput()) return false;
    duration_ms = std::clamp(duration_ms, 0, kMaxSwipeMs);
    const auto path = SwipePath(from, to, duration_ms, std::max(options_.swipe_step_ms, 1));

    std::vector<POINT> screen(path.size());
    for (std::size_t i = 0; i < path.size(); ++i) {
        const Point native = ToNative(path[i], native_);
        screen[i] = {native.x, native.y};
        if (!ClientToScreen(hwnd_, &screen[i])) return Fail("ClientToScreen failed");
    }
    if (RootOf(WindowFromPoint(screen.front())) != RootOf(hwnd_)) {
        return Fail("another window covers the swipe start point");
    }

    CursorRestore restore(options_.restore_cursor);
    if (!MoveCursorTo(screen.front())) return Fail("SendInput move failed");
    Pause(options_.move_settle_ms);
    if (!LeftButton(true)) return Fail("SendInput button down failed");
    bool ok = true;
    for (std::size_t i = 1; i < screen.size() && ok; ++i) {
        Pause(options_.swipe_step_ms);
        ok = MoveCursorTo(screen[i]);
    }
    Pause(options_.move_settle_ms);
    const bool up = LeftButton(false);  // always release, even after a failed move
    if (!ok || !up) return Fail("SendInput failed during swipe");
    last_error_.clear();
    return true;
}

bool SteamController::Back() {
    DpiAwareScope dpi;
    if (!PrepareInput()) return false;
    if (!PressScanCode(kScanEsc)) return Fail("SendInput key press failed");
    last_error_.clear();
    return true;
}

}  // namespace maba::win

#endif  // _WIN32
