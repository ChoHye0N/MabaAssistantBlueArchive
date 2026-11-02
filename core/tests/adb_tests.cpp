// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Portable ADB helpers, and the ADB controller against a fake adb (OpenCV builds).
#include <string>

#include "fakes.h"
#include "maba/adb.h"
#include "test_framework.h"

#if defined(MABA_WITH_OPENCV)
#include <opencv2/imgcodecs.hpp>

#include "maba/adb_controller.h"
#include "maba/vision.h"
#endif

namespace {

void PutLe32(std::string& s, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) s += static_cast<char>((v >> (8 * i)) & 0xff);
}

// Raw screencap: header (w, h, format[, colorspace]) + RGBA pixels of one color.
std::string RawScreencap(int w, int h, bool header16, unsigned char r, unsigned char g,
                         unsigned char b) {
    std::string s;
    PutLe32(s, w);
    PutLe32(s, h);
    PutLe32(s, 1);
    if (header16) PutLe32(s, 0);
    for (int i = 0; i < w * h; ++i) {
        s += static_cast<char>(r);
        s += static_cast<char>(g);
        s += static_cast<char>(b);
        s += static_cast<char>(255);
    }
    return s;
}

MABA_TEST(AdbParsesDevices) {
    const auto d = maba::adb::ParseDevices(
        "* daemon started successfully\nList of devices attached\n127.0.0.1:16384\tdevice\r\n"
        "emulator-5554\toffline\n\n");
    CHECK(d.size() == 2);
    CHECK(d.size() == 2 && d[0].serial == "127.0.0.1:16384" && d[0].state == "device");
    CHECK(d.size() == 2 && d[1].state == "offline");
}

MABA_TEST(AdbParsesWmSize) {
    const auto a = maba::adb::ParseWmSize("Physical size: 720x1280\n");
    CHECK(a && a->width == 720 && a->height == 1280);
    const auto b = maba::adb::ParseWmSize("Physical size: 1080x1920\nOverride size: 900x1600\n");
    CHECK(b && b->width == 900 && b->height == 1600);
    CHECK(!maba::adb::ParseWmSize("error: device offline"));
    const maba::Size l = maba::adb::Landscape({720, 1280});
    CHECK(l.width == 1280 && l.height == 720);
}

MABA_TEST(AdbConvertsCrlf) {
    std::string s = "a\r\nb\r\r\nc\r";
    CHECK(maba::adb::ConvertCrlf(s));
    CHECK(s == "a\nb\r\nc\r");  // single pass: "\r\r\n" -> "\r\n"
    std::string t = "plain\n";
    CHECK(!maba::adb::ConvertCrlf(t));
}

MABA_TEST(AdbDecodesRawScreencap) {
    for (bool h16 : {false, true}) {
        const auto f = maba::adb::DecodeRawScreencap(RawScreencap(4, 2, h16, 10, 20, 30));
        CHECK(f.has_value());
        if (!f) continue;
        CHECK(f->width == 4 && f->height == 2);
        CHECK(f->bgr[0] == 30 && f->bgr[1] == 20 && f->bgr[2] == 10);
    }
    std::string truncated = RawScreencap(4, 2, false, 1, 2, 3);
    truncated.pop_back();
    CHECK(!maba::adb::DecodeRawScreencap(truncated));
    std::string transparent = RawScreencap(4, 2, false, 1, 2, 3);
    transparent.back() = 0;
    CHECK(!maba::adb::DecodeRawScreencap(transparent));
}

MABA_TEST(AdbCommandLines) {
    const auto shell = maba::adb::ShellCommand("adb", "emulator-5554", "input tap 1 2");
    CHECK(shell.size() == 5 && shell[3] == "shell" && shell[4] == "input tap 1 2");
    CHECK(maba::adb::TapRemote({3, 4}) == "input tap 3 4");
    CHECK(maba::adb::SwipeRemote({1, 2}, {3, 4}, 0) == "input swipe 1 2 3 4 1");
    CHECK(maba::adb::KeyEventRemote(111) == "input keyevent 111");
}

MABA_TEST(TouchProtocol) {
    maba::adb::TouchLimits l;
    CHECK(!maba::adb::ParseTouchHeaderLine("v 1", l));
    CHECK(maba::adb::ParseTouchHeaderLine("^ 10 1080 1920 255", l));
    CHECK(l.max_x == 1920 && l.max_y == 1080 && l.max_contacts == 10);  // swapped to landscape
    const maba::Point p = maba::adb::ToTouch({640, 360}, {1280, 720}, l);
    CHECK(p.x == 960 && p.y == 540);
    CHECK(maba::adb::TouchTap({1, 2}, 50, l) == "d 0 1 2 255\nc\nw 50\nu 0\nc\n");
    CHECK(maba::adb::TouchSwipe({{0, 0}, {5, 5}}, 16, l) ==
          "d 0 0 0 255\nc\nw 16\nm 0 5 5 255\nc\nw 16\nu 0\nc\n");
    CHECK(maba::adb::TouchKey(111) == "k 111 d\nc\nk 111 u\nc\n");
}

#if defined(MABA_WITH_OPENCV)

struct AdbRig {
    AdbRig() {
        runner.responses["adb devices"] =
            maba_test::FakeRunner::Ok("List of devices attached\n127.0.0.1:16384\tdevice\n");
        runner.responses["adb -s 127.0.0.1:16384 shell wm size"] =
            maba_test::FakeRunner::Ok("Physical size: 720x1280\n");
        runner.responses["adb -s 127.0.0.1:16384 exec-out screencap"] =
            maba_test::FakeRunner::Ok(RawScreencap(1280, 720, true, 0, 0, 200));
        cv::Mat img(720, 1280, CV_8UC3, cv::Scalar(200, 0, 0));
        std::vector<uchar> png;
        cv::imencode(".png", img, png);
        runner.responses["adb -s 127.0.0.1:16384 exec-out screencap -p"] =
            maba_test::FakeRunner::Ok(std::string(png.begin(), png.end()));
    }
    maba_test::FakeRunner runner;
};

MABA_TEST(AdbControllerConnectsAndCaptures) {
    AdbRig rig;
    maba::AdbController c(rig.runner, nullptr, {});
    CHECK(c.Connect());
    CHECK(c.NativeSize().width == 1280 && c.NativeSize().height == 720);
    CHECK(c.ScreencapMethod() != maba::AdbScreencap::kAuto);
    const maba::Frame f = c.Screenshot();
    CHECK(f.width == 1280 && f.height == 720);
    CHECK(!f.empty() && f.bgr[0] == 200 && f.bgr[2] == 0);  // BGR: blue channel
    CHECK(c.Click({640, 360}));
    CHECK(rig.runner.commands.back() == "adb -s 127.0.0.1:16384 shell input tap 640 360");
    CHECK(c.Back());
    CHECK(rig.runner.commands.back() == "adb -s 127.0.0.1:16384 shell input keyevent 111");
    CHECK(c.Swipe({0, 0}, {100, 0}, 300));
    CHECK(rig.runner.commands.back() == "adb -s 127.0.0.1:16384 shell input swipe 0 0 100 0 300");
}

MABA_TEST(AdbControllerScalesToNativeAndRepairsCrlf) {
    AdbRig rig;
    rig.runner.responses["adb -s 127.0.0.1:16384 shell wm size"] =
        maba_test::FakeRunner::Ok("Physical size: 1080x1920\n");
    // Encode output with every "\n" turned into "\r\n", as old adb does on Windows.
    cv::Mat img(1080, 1920, CV_8UC3, cv::Scalar(0, 99, 0));
    std::vector<uchar> png;
    cv::imencode(".png", img, png);
    std::string damaged;
    for (uchar ch : png) {
        if (ch == '\n') damaged += '\r';
        damaged += static_cast<char>(ch);
    }
    rig.runner.responses["adb -s 127.0.0.1:16384 exec-out screencap -p"] = maba_test::FakeRunner::Ok(damaged);
    maba::AdbControllerOptions o;
    o.screencap = maba::AdbScreencap::kEncode;
    maba::AdbController c(rig.runner, nullptr, o);
    const maba::Frame f = c.Screenshot();
    CHECK(f.width == 1280 && f.height == 720);
    CHECK(!f.empty() && f.bgr[1] == 99);
    CHECK(c.Click({640, 360}));
    CHECK(rig.runner.commands.back() == "adb -s 127.0.0.1:16384 shell input tap 960 540");
    const maba::Frame crop = c.CaptureRegion({0, 0, 128, 72});
    CHECK(crop.width == 192 && crop.height == 108);
}

MABA_TEST(AdbControllerReportsMissingDevice) {
    AdbRig rig;
    rig.runner.responses["adb devices"] = maba_test::FakeRunner::Ok("List of devices attached\n");
    maba::AdbController c(rig.runner, nullptr, {});
    CHECK(!c.Connect());
    CHECK(c.LastError().find("not found") != std::string::npos);
    CHECK(c.Screenshot().empty());
}

MABA_TEST(AdbControllerRejectsNon169) {
    AdbRig rig;
    rig.runner.responses["adb -s 127.0.0.1:16384 shell wm size"] =
        maba_test::FakeRunner::Ok("Physical size: 1200x1920\n");
    maba::AdbController c(rig.runner, nullptr, {});
    CHECK(!c.Connect());
    CHECK(c.LastError().find("16:9") != std::string::npos);
}

MABA_TEST(AdbControllerUsesMaaTouch) {
    AdbRig rig;
    maba_test::FakeLauncher launcher;
    maba_test::FakeInteractive* helper = nullptr;
    launcher.make = [&] {
        auto p = std::make_unique<maba_test::FakeInteractive>();
        p->pending = {"v 1", "^ 10 720 1280 255", "$ 1234"};
        helper = p.get();
        return p;
    };
    maba::AdbControllerOptions o;
    o.touch = maba::AdbTouch::kMaaTouch;
    o.maatouch_path = "maatouch";
    maba::AdbController c(rig.runner, &launcher, o);
    CHECK(c.Connect());
    CHECK(launcher.starts == 1);
    CHECK(c.Click({640, 360}));
    CHECK(helper && !helper->written.empty());
    if (helper && !helper->written.empty())
        CHECK(helper->written.back().rfind("d 0 640 360 255\nc\n", 0) == 0);
    CHECK(c.Back());
    CHECK(helper && helper->written.back().rfind("k 111 d", 0) == 0);
}

#endif  // MABA_WITH_OPENCV

}  // namespace
