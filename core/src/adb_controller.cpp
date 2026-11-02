// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Written from docs/notes/emulator-adb.md (fastest-screencap probe, MaaTouch launch line); the
// noted behaviors follow the emulator controller of an AGPL-3.0 project, see docs/SOURCES.md.
#include "maba/adb_controller.h"

#if defined(MABA_WITH_OPENCV)

#include <chrono>

#include <opencv2/imgcodecs.hpp>

#include "maba/coords.h"
#include "maba/geometry.h"
#include "maba/vision.h"

namespace maba {

namespace {

using Clock = std::chrono::steady_clock;
using std::chrono::milliseconds;

constexpr const char* kMaaTouchRemote = "/data/local/tmp/maatouch";

}  // namespace

const char* ToString(AdbScreencap m) {
    switch (m) {
        case AdbScreencap::kAuto: return "auto";
        case AdbScreencap::kRaw: return "raw";
        case AdbScreencap::kEncode: return "encode";
    }
    return "?";
}

const char* ToString(AdbTouch m) {
    switch (m) {
        case AdbTouch::kAdbInput: return "adb";
        case AdbTouch::kMaaTouch: return "maatouch";
    }
    return "?";
}

AdbController::AdbController(IProcessRunner& runner, IProcessLauncher* launcher,
                             AdbControllerOptions options, LogFn log)
    : runner_(runner), launcher_(launcher), options_(std::move(options)), log_(std::move(log)) {}

AdbController::~AdbController() {
    if (touch_) touch_->Terminate();
}

bool AdbController::Fail(std::string message) {
    last_error_ = std::move(message);
    return false;
}

bool AdbController::Shell(const std::string& remote, std::string* out) {
    const ProcessResult r = runner_.Run(adb::ShellCommand(options_.adb_path, options_.serial, remote),
                                        milliseconds{options_.command_timeout_ms});
    if (!r.started) return Fail("cannot run adb: " + r.error);
    if (r.timed_out) return Fail("adb shell '" + remote + "' timed out");
    if (r.exit_code != 0) return Fail("adb shell '" + remote + "' failed: " + r.err);
    if (out) *out = r.out;
    return true;
}

bool AdbController::Connect() {
    connected_ = false;
    last_error_.clear();
    if (options_.serial.empty()) return Fail("no device serial configured");
    const milliseconds timeout{options_.command_timeout_ms};

    // Network serials (host:port) need `adb connect`; local ones (emulator-5554) do not.
    if (options_.serial.find(':') != std::string::npos) {
        const ProcessResult r = runner_.Run(adb::ConnectCommand(options_.adb_path, options_.serial), timeout);
        if (!r.started) return Fail("cannot run adb (" + options_.adb_path + "): " + r.error);
        if (r.timed_out) return Fail("adb connect timed out");
    }
    const ProcessResult devices = runner_.Run(adb::DevicesCommand(options_.adb_path), timeout);
    if (!devices.ok()) return Fail("adb devices failed: " + (devices.error.empty() ? devices.err : devices.error));
    bool listed = false;
    for (const auto& d : adb::ParseDevices(devices.out)) {
        if (d.serial != options_.serial) continue;
        listed = true;
        if (d.state != "device") return Fail("device " + d.serial + " is " + d.state);
    }
    if (!listed) return Fail("device " + options_.serial + " not found in `adb devices`");

    std::string wm;
    if (!Shell("wm size", &wm)) return false;
    const auto size = adb::ParseWmSize(wm);
    if (!size) return Fail("cannot read the screen size from: " + wm);
    native_ = adb::Landscape(*size);
    if (!IsSupportedAspect(native_))
        return Fail("screen " + std::to_string(native_.width) + "x" + std::to_string(native_.height) +
                    " is not 16:9; set the emulator resolution to 1280x720, 1600x900 or 1920x1080");

    connected_ = true;  // CaptureWith needs it
    method_ = options_.screencap;
    if (method_ == AdbScreencap::kAuto) {
        // Try both and keep the faster one.
        long long best = -1;
        for (AdbScreencap m : {AdbScreencap::kRaw, AdbScreencap::kEncode}) {
            cv::Mat img;
            const auto start = Clock::now();
            if (!CaptureWith(m, img)) {
                Log(log_, LogLevel::kInfo, std::string("adb: screencap ") + ToString(m) +
                                               " not usable: " + last_error_);
                continue;
            }
            const long long cost =
                std::chrono::duration_cast<milliseconds>(Clock::now() - start).count();
            Log(log_, LogLevel::kInfo,
                std::string("adb: screencap ") + ToString(m) + " " + std::to_string(cost) + " ms");
            if (best < 0 || cost < best) {
                best = cost;
                method_ = m;
            }
        }
        if (best < 0) {
            connected_ = false;
            return Fail("no screencap method works: " + last_error_);
        }
    }
    if (options_.touch == AdbTouch::kMaaTouch && !StartTouchHelper()) {
        connected_ = false;
        return false;
    }
    Log(log_, LogLevel::kInfo, "adb: connected to " + options_.serial + " (" +
                                   std::to_string(native_.width) + "x" + std::to_string(native_.height) +
                                   ", screencap " + ToString(method_) + ", touch " +
                                   ToString(options_.touch) + ")");
    last_error_.clear();
    return true;
}

bool AdbController::EnsureConnected() { return connected_ || Connect(); }

bool AdbController::StartTouchHelper() {
    if (!launcher_) return Fail("MaaTouch needs a process launcher");
    if (options_.maatouch_path.empty()) return Fail("MaaTouch selected but no file configured");
    const ProcessResult push = runner_.Run(
        adb::PushCommand(options_.adb_path, options_.serial, options_.maatouch_path, kMaaTouchRemote),
        milliseconds{options_.command_timeout_ms});
    if (!push.ok()) return Fail("cannot push MaaTouch: " + (push.error.empty() ? push.err : push.error));

    const std::string launch = std::string("export CLASSPATH=") + kMaaTouchRemote +
                               "; app_process /data/local/tmp com.shxyke.MaaTouch.App";
    std::string error;
    touch_ = launcher_->Start(adb::ShellCommand(options_.adb_path, options_.serial, launch), error);
    if (!touch_) return Fail("cannot start MaaTouch: " + error);
    touch_limits_ = {};
    for (int i = 0; i < 10; ++i) {  // header: v, ^, $ lines
        const auto line = touch_->ReadLine(milliseconds{3000});
        if (!line) break;
        if (adb::ParseTouchHeaderLine(*line, touch_limits_)) break;
    }
    if (!touch_limits_.valid()) {
        touch_->Terminate();
        touch_.reset();
        return Fail("MaaTouch did not report its touch limits");
    }
    return true;
}

bool AdbController::CaptureWith(AdbScreencap method, cv::Mat& out) {
    const std::string remote = method == AdbScreencap::kEncode ? "screencap -p" : "screencap";
    const ProcessResult r = runner_.Run(adb::ExecOutCommand(options_.adb_path, options_.serial, remote),
                                        milliseconds{options_.screencap_timeout_ms});
    if (!r.started) return Fail("cannot run adb: " + r.error);
    if (r.timed_out) return Fail("screencap timed out");
    if (r.exit_code != 0 || r.out.empty()) return Fail("screencap failed: " + r.err);

    std::string data = r.out;
    if (crlf_) adb::ConvertCrlf(data);
    if (method == AdbScreencap::kRaw) {
        auto frame = adb::DecodeRawScreencap(data);
        if (!frame && !crlf_ && adb::ConvertCrlf(data)) {
            frame = adb::DecodeRawScreencap(data);
            if (frame) crlf_ = true;
        }
        if (!frame) return Fail("cannot decode raw screencap (" + std::to_string(r.out.size()) + " bytes)");
        out = FrameView(*frame).clone();
    } else {
        cv::Mat img = cv::imdecode(cv::Mat(1, static_cast<int>(data.size()), CV_8UC1, data.data()),
                                   cv::IMREAD_COLOR);
        if (img.empty() && !crlf_ && adb::ConvertCrlf(data)) {
            img = cv::imdecode(cv::Mat(1, static_cast<int>(data.size()), CV_8UC1, data.data()),
                               cv::IMREAD_COLOR);
            if (!img.empty()) crlf_ = true;
        }
        if (img.empty()) return Fail("cannot decode PNG screencap");
        out = img;
    }
    return CheckScreen(out);
}

bool AdbController::CheckScreen(const cv::Mat& native) {
    if (native.rows > native.cols)
        return Fail("screen is in portrait orientation; set the emulator to landscape");
    const Size s{native.cols, native.rows};
    if (!IsSupportedAspect(s)) return Fail("captured screen is not 16:9");
    native_ = s;  // the capture is the source of truth for input scaling
    return true;
}

bool AdbController::CaptureNative(cv::Mat& out) {
    if (!EnsureConnected()) return false;
    if (CaptureWith(method_, out)) return true;
    // One reconnect attempt: the emulator may have restarted.
    const std::string first = last_error_;
    if (!Connect()) return Fail(first + "; reconnect failed: " + last_error_);
    return CaptureWith(method_, out);
}

Frame AdbController::Screenshot() {
    cv::Mat native;
    if (!CaptureNative(native)) return {};
    last_error_.clear();
    return NormalizeFrame(native);
}

Frame AdbController::CaptureRegion(Rect normalized) {
    cv::Mat native;
    if (!CaptureNative(native)) return {};
    const Rect r = NormalizedRectToNative(normalized, native_);
    if (!IsValid(r)) {
        Fail("empty region");
        return {};
    }
    last_error_.clear();
    return FrameFromMat(native(cv::Rect(r.x, r.y, r.width, r.height)));
}

Point AdbController::ToNativePoint(Point normalized) const { return ToNative(normalized, native_); }

bool AdbController::Touch(const std::string& commands) {
    if (!touch_ || !touch_->Running()) {
        if (!StartTouchHelper()) return false;
    }
    if (!touch_->WriteLine(commands.substr(0, commands.size() - 1))) {  // WriteLine adds '\n'
        touch_.reset();
        return Fail("MaaTouch write failed");
    }
    return true;
}

bool AdbController::Click(Point normalized) {
    if (!EnsureConnected()) return false;
    const Point p = ToNativePoint(normalized);
    if (options_.touch == AdbTouch::kMaaTouch)
        return Touch(adb::TouchTap(adb::ToTouch(p, native_, touch_limits_), options_.click_hold_ms,
                                   touch_limits_));
    return Shell(adb::TapRemote(p));
}

bool AdbController::Swipe(Point from, Point to, int duration_ms) {
    if (!EnsureConnected()) return false;
    const Point a = ToNativePoint(from), b = ToNativePoint(to);
    if (options_.touch == AdbTouch::kMaaTouch) {
        std::vector<Point> path;
        for (const Point& p : SwipePath(a, b, duration_ms, options_.swipe_step_ms))
            path.push_back(adb::ToTouch(p, native_, touch_limits_));
        return Touch(adb::TouchSwipe(path, options_.swipe_step_ms, touch_limits_));
    }
    return Shell(adb::SwipeRemote(a, b, duration_ms));
}

bool AdbController::Back() {
    if (!EnsureConnected()) return false;
    if (options_.touch == AdbTouch::kMaaTouch) return Touch(adb::TouchKey(options_.back_keycode));
    return Shell(adb::KeyEventRemote(options_.back_keycode));
}

}  // namespace maba

#endif  // MABA_WITH_OPENCV
