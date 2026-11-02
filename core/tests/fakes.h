// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Test doubles: a controller that replays frames, a fake OCR and a manual clock.
#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "maba/controller.h"
#include "maba/ocr.h"
#include "maba/wait.h"

namespace maba_test {

inline maba::Frame BlankFrame(int w, int h, std::uint8_t value = 0) {
    maba::Frame f;
    f.width = w;
    f.height = h;
    f.bgr.assign(static_cast<std::size_t>(w) * h * 3, value);
    return f;
}

// Replays "scenes": Screenshot() returns the current scene's frame. A click can move to
// another scene through `on_click`, which mimics the game reacting to input.
class FakeController : public maba::IController {
public:
    FakeController() { scenes.push_back(BlankFrame(maba::kNormWidth, maba::kNormHeight)); }

    maba::Size NativeSize() const override { return {1920, 1080}; }
    maba::Frame Screenshot() override {
        ++screenshots;
        return scenes.at(scene);
    }
    maba::Frame CaptureRegion(maba::Rect r) override {
        return BlankFrame(r.width * 3 / 2, r.height * 3 / 2);
    }
    bool Click(maba::Point p) override {
        clicks.push_back(p);
        if (on_click) on_click(p, *this);
        return true;
    }
    bool Swipe(maba::Point, maba::Point, int) override { return true; }
    bool Back() override { ++backs; return true; }

    std::vector<maba::Frame> scenes;
    std::size_t scene = 0;
    std::function<void(maba::Point, FakeController&)> on_click;
    std::vector<maba::Point> clicks;
    int screenshots = 0;
    int backs = 0;
};

class FakeOcr : public maba::IOcr {
public:
    std::vector<maba::OcrResult> Recognize(const maba::Frame&, const maba::Rect&,
                                          const std::string&) override {
        return {{"123", {}, 0.9f}};
    }
};

// Time only moves when Sleep is called (or `advance_per_now` is set).
class ManualClock : public maba::IClock {
public:
    std::chrono::milliseconds Now() override {
        now += advance_per_now;
        return now;
    }
    void Sleep(std::chrono::milliseconds d) override {
        now += d;
        ++sleeps;
    }
    std::chrono::milliseconds now{0};
    std::chrono::milliseconds advance_per_now{0};
    int sleeps = 0;
};

}  // namespace maba_test
