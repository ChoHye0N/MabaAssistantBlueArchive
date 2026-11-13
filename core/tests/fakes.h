// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Test doubles: a controller that replays frames, a fake OCR and a manual clock.
#pragma once

#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "maba/controller.h"
#include "maba/matcher.h"
#include "maba/ocr.h"
#include "maba/process.h"
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
        if (before_screenshot) before_screenshot(*this);
        return scenes.at(scene);
    }
    maba::Frame CaptureRegion(maba::Rect r) override {
        return BlankFrame(r.width * 3 / 2, r.height * 3 / 2);
    }
    bool Click(maba::Point p) override {
        if (refuse_clicks > 0) {  // e.g. the game window does not answer yet
            --refuse_clicks;
            return false;
        }
        clicks.push_back(p);
        if (on_click) on_click(p, *this);
        return true;
    }
    bool Swipe(maba::Point, maba::Point, int) override { return true; }
    bool Scroll(maba::Point, int notches) override {
        scrolled += notches;
        return can_scroll;
    }
    bool Back() override { ++backs; return true; }

    int scrolled = 0;
    int refuse_clicks = 0;
    bool can_scroll = true;
    std::vector<maba::Frame> scenes;
    std::size_t scene = 0;
    std::function<void(maba::Point, FakeController&)> on_click;
    std::function<void(FakeController&)> before_screenshot;
    std::vector<maba::Point> clicks;
    int screenshots = 0;
    int backs = 0;
};

class FakeOcr : public maba::IOcr {
public:
    std::vector<maba::OcrResult> Recognize(const maba::Frame& frame, const maba::Rect&,
                                          const std::string& language) override {
        ++calls;
        last_width = frame.width;
        last_language = language;
        return results;
    }
    std::vector<maba::OcrResult> results{{"123", {}, 0.9f}};
    std::string last_language;
    int calls = 0;
    int last_width = 0;
};

// Scenes are told apart by the fill value of the frame (first byte). Each template is visible
// in a set of scenes, at a fixed box.
class SceneMatcher : public maba::IMatcher {
public:
    struct Entry {
        std::set<std::uint8_t> scenes;
        maba::Rect box;
        std::vector<maba::Rect> more;  // further matches returned by FindAll
    };

    void Add(const std::string& file, std::set<std::uint8_t> scenes, maba::Rect box = {10, 10, 20, 20}) {
        entries[file] = {std::move(scenes), box};
    }

    maba::MatchResult Find(const maba::Frame& frame, const maba::TemplateRef& ref) override {
        maba::MatchResult r;
        auto it = entries.find(ref.file);
        if (it == entries.end()) return r;
        const bool visible = !frame.empty() && it->second.scenes.count(frame.bgr[0]) > 0;
        r.score = visible ? 0.97 : 0.30;
        r.found = r.score >= ref.threshold;
        r.box = it->second.box;
        r.center = {r.box.x + r.box.width / 2, r.box.y + r.box.height / 2};
        return r;
    }
    std::vector<maba::MatchResult> FindAll(const maba::Frame& f, const maba::TemplateRef& ref,
                                           int) override {
        std::vector<maba::MatchResult> out{Find(f, ref)};
        auto it = entries.find(ref.file);
        if (it == entries.end() || !out.front().found) return out;
        for (const auto& box : it->second.more) {
            maba::MatchResult r = out.front();
            r.box = box;
            r.center = {box.x + box.width / 2, box.y + box.height / 2};
            out.push_back(r);
        }
        return out;
    }
    const std::string& LastError() const override { return error; }

    std::map<std::string, Entry> entries;
    std::string error;
};

// Controller whose scenes are frames filled with a value; transitions are a map from
// (scene, click point) to the next scene, plus what Back() does per scene.
class SceneController : public FakeController {
public:
    explicit SceneController(std::vector<std::uint8_t> values) {
        scenes.clear();
        for (auto v : values) scenes.push_back(BlankFrame(maba::kNormWidth, maba::kNormHeight, v));
        on_click = [this](maba::Point p, FakeController&) {
            for (const auto& t : clicks_to) {
                if (t.from == scene && t.at.x == p.x && t.at.y == p.y) {
                    scene = t.to;
                    return;
                }
            }
        };
    }
    bool Back() override {
        ++backs;
        auto it = back_to.find(scene);
        if (it != back_to.end()) scene = it->second;
        return true;
    }
    struct ClickTransition {
        std::size_t from;
        maba::Point at;
        std::size_t to;
    };
    std::vector<ClickTransition> clicks_to;
    std::map<std::size_t, std::size_t> back_to;
};

// Process runner that answers by the joined command line.
class FakeRunner : public maba::IProcessRunner {
public:
    maba::ProcessResult Run(const std::vector<std::string>& argv,
                            std::chrono::milliseconds) override {
        std::string cmd;
        for (const auto& a : argv) cmd += (cmd.empty() ? "" : " ") + a;
        commands.push_back(cmd);
        auto it = responses.find(cmd);
        if (it != responses.end()) return it->second;
        maba::ProcessResult r;
        r.started = true;
        r.exit_code = 0;
        return r;
    }
    static maba::ProcessResult Ok(std::string out) {
        maba::ProcessResult r;
        r.started = true;
        r.exit_code = 0;
        r.out = std::move(out);
        return r;
    }
    std::map<std::string, maba::ProcessResult> responses;
    std::vector<std::string> commands;
};

// Line peer driven by a handler: every written line may produce reply lines.
class FakeInteractive : public maba::IInteractiveProcess {
public:
    bool WriteLine(const std::string& line) override {
        if (!running) return false;
        written.push_back(line);
        if (on_line) on_line(line, *this);
        return true;
    }
    std::optional<std::string> ReadLine(std::chrono::milliseconds) override {
        if (pending.empty()) return std::nullopt;
        std::string l = pending.front();
        pending.pop_front();
        return l;
    }
    bool Running() override { return running; }
    void Terminate() override { running = false; }

    std::deque<std::string> pending;
    std::vector<std::string> written;
    std::function<void(const std::string&, FakeInteractive&)> on_line;
    bool running = true;
};

class FakeLauncher : public maba::IProcessLauncher {
public:
    std::unique_ptr<maba::IInteractiveProcess> Start(const std::vector<std::string>& argv,
                                                     std::string& error) override {
        ++starts;
        last_argv = argv;
        if (!make) {
            error = "no process";
            return nullptr;
        }
        return make();
    }
    std::function<std::unique_ptr<maba::IInteractiveProcess>()> make;
    std::vector<std::string> last_argv;
    int starts = 0;
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
