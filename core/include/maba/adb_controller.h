// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// IController for Android emulators over ADB (MuMu, LDPlayer, BlueStacks, ...).
// Capture: `screencap` raw or PNG ("encode"), the faster one chosen on connect.
// Input: `input tap/swipe/keyevent`, or the MaaTouch helper (minitouch protocol) when its file
// is provided. Needs OpenCV (PNG decoding, scaling). Notes: docs/notes/controllers.md.
#pragma once

#if defined(MABA_WITH_OPENCV)

#include <memory>
#include <string>

#include <opencv2/core.hpp>

#include "maba/adb.h"
#include "maba/controller.h"
#include "maba/log.h"
#include "maba/process.h"

namespace maba {

enum class AdbScreencap { kAuto, kRaw, kEncode };
enum class AdbTouch { kAdbInput, kMaaTouch };

const char* ToString(AdbScreencap m);
const char* ToString(AdbTouch m);

struct AdbControllerOptions {
    std::string adb_path = "adb";
    std::string serial = "127.0.0.1:16384";  // MuMu 12 default; see config/emulators.json
    AdbScreencap screencap = AdbScreencap::kAuto;
    AdbTouch touch = AdbTouch::kAdbInput;
    std::string maatouch_path;  // local MaaTouch file (not bundled), pushed on connect
    // Key used by Back(). 111 = KEYCODE_ESCAPE, 4 = KEYCODE_BACK. Verify which one the game
    // treats as "back" on every page.
    int back_keycode = 111;
    int command_timeout_ms = 10000;
    int screencap_timeout_ms = 15000;
    int click_hold_ms = 50;
    int swipe_step_ms = 16;
};

class AdbController : public IController {
public:
    // `launcher` is needed only for AdbTouch::kMaaTouch (may be null otherwise).
    AdbController(IProcessRunner& runner, IProcessLauncher* launcher, AdbControllerOptions options,
                  LogFn log = {});
    ~AdbController() override;

    // adb connect (network serials), device check, screen size, screencap probe, touch helper.
    // Called automatically by the first operation.
    bool Connect();

    Size NativeSize() const override { return native_; }
    Frame Screenshot() override;
    Frame CaptureRegion(Rect normalized) override;
    bool Click(Point normalized) override;
    bool Swipe(Point from, Point to, int duration_ms) override;
    bool Back() override;

    const std::string& LastError() const { return last_error_; }
    std::string Error() const override { return last_error_; }
    AdbScreencap ScreencapMethod() const { return method_; }
    const AdbControllerOptions& Options() const { return options_; }

private:
    bool EnsureConnected();
    bool CaptureNative(cv::Mat& out);
    bool CaptureWith(AdbScreencap method, cv::Mat& out);
    bool StartTouchHelper();
    bool Shell(const std::string& remote, std::string* out = nullptr);
    bool Touch(const std::string& commands);
    bool CheckScreen(const cv::Mat& native);
    Point ToNativePoint(Point normalized) const;
    bool Fail(std::string message);

    IProcessRunner& runner_;
    IProcessLauncher* launcher_;
    AdbControllerOptions options_;
    LogFn log_;
    bool connected_ = false;
    AdbScreencap method_ = AdbScreencap::kAuto;
    bool crlf_ = false;  // shell output needs CRLF repair (old adb on Windows)
    Size native_{};      // landscape screen size in pixels
    std::unique_ptr<IInteractiveProcess> touch_;
    adb::TouchLimits touch_limits_;
    std::string last_error_;
};

}  // namespace maba

#endif  // MABA_WITH_OPENCV
