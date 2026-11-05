// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Colour blobs, task settings and the step language (conditions, loops, each, clicks).
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "fakes.h"
#include "maba/color.h"
#include "maba/task.h"
#include "maba/screen_change.h"
#include "maba/task_runner.h"
#include "nav_rig.h"
#include "test_framework.h"

namespace {

using maba_test::NavRig;

maba::TaskSpec Task(const char* json) {
    const auto t = maba::ParseTask(json);
    if (!t.ok) std::printf("task parse error: %s\n", t.error.c_str());
    CHECK(t.ok);
    return t.task;
}

void Paint(maba::Frame& f, maba::Rect r, std::uint8_t b, std::uint8_t g, std::uint8_t red) {
    for (int y = r.y; y < r.y + r.height; ++y) {
        for (int x = r.x; x < r.x + r.width; ++x) {
            const std::size_t i = (static_cast<std::size_t>(y) * f.width + x) * 3;
            f.bgr[i] = b;
            f.bgr[i + 1] = g;
            f.bgr[i + 2] = red;
        }
    }
}

// Yellow marker colour of the cafe students, as measured in the game.
maba::ColorSpec Yellow() {
    maba::ColorSpec c;
    c.h_min = 20;
    c.s_min = 200;
    c.v_min = 230;
    c.h_max = 30;
    c.min_width = 30;
    c.max_width = 40;
    c.min_height = 9;
    c.max_height = 15;
    return c;
}

MABA_TEST(HsvMatchesOpenCvScale) {
    int h = 0, s = 0, v = 0;
    maba::BgrToHsv(2, 222, 255, h, s, v);  // a marker pixel; OpenCV gives (26, 253, 255)
    CHECK(h == 26 && s == 253 && v == 255);
    maba::BgrToHsv(255, 200, 49, h, s, v);  // active banner dot; OpenCV gives (98, 206, 255)
    CHECK(h == 98 && s == 206 && v == 255);
    maba::BgrToHsv(10, 10, 10, h, s, v);
    CHECK(h == 0 && s == 0 && v == 10);
}

MABA_TEST(ColorBlobsFilterBySize) {
    maba::Frame f = maba_test::BlankFrame(maba::kNormWidth, maba::kNormHeight, 20);
    Paint(f, {100, 200, 35, 12}, 2, 222, 255);  // marker stroke
    Paint(f, {400, 50, 35, 12}, 2, 222, 255);   // another one, higher up
    Paint(f, {700, 300, 13, 13}, 2, 222, 255);  // notification dot: too small
    Paint(f, {900, 300, 35, 12}, 255, 0, 0);    // right size, wrong colour
    const auto blobs = maba::FindColorBlobs(f, Yellow());
    CHECK(blobs.size() == 2);
    if (blobs.size() == 2) {
        CHECK(blobs[0].x == 400 && blobs[0].y == 50 && blobs[0].width == 35 && blobs[0].height == 12);
        CHECK(blobs[1].x == 100 && blobs[1].y == 200);
    }
    maba::ColorSpec in_roi = Yellow();
    in_roi.roi = maba::Rect{0, 150, 640, 200};
    CHECK(maba::FindColorBlobs(f, in_roi).size() == 1);
}

MABA_TEST(SettingsParseAndResolve) {
    const auto t = Task(R"({"id": "t", "settings": [
        {"key": "on", "type": "bool", "default": true},
        {"key": "n", "type": "int", "default": 5, "min": 0, "max": 5},
        {"key": "mode", "type": "choice", "default": "all", "choices": ["all", "some"]},
        {"key": "targets", "type": "list", "default": ["a", "b"], "choices": ["a", "b", "c"]}]})");
    const auto d = maba::DefaultSettings(t);
    CHECK(d.at("on").scalar == "true" && d.at("n").scalar == "5" && d.at("mode").scalar == "all");
    CHECK((d.at("targets").list == std::vector<std::string>{"a", "b"}));

    std::map<std::string, maba::TaskSettings> out;
    std::string error;
    CHECK(maba::ResolveSettings({t}, R"({"t": {"n": 2, "targets": ["c"]}})", out, error));
    CHECK(out["t"].at("n").scalar == "2" && out["t"].at("on").scalar == "true");
    CHECK((out["t"].at("targets").list == std::vector<std::string>{"c"}));
    CHECK(!maba::ResolveSettings({t}, R"({"t": {"n": 9}})", out, error) && error.find("[0, 5]") != std::string::npos);
    CHECK(!maba::ResolveSettings({t}, R"({"t": {"mode": "x"}})", out, error));
    CHECK(!maba::ResolveSettings({t}, R"({"t": {"zzz": 1}})", out, error) && error.find("unknown setting") != std::string::npos);
    CHECK(!maba::ResolveSettings({t}, R"({"other": {}})", out, error) && error.find("unknown task") != std::string::npos);
    CHECK(maba::ResolveSettings({t}, "", out, error) && out["t"].at("n").scalar == "5");

    CHECK(!maba::ParseTask(R"({"id": "t", "settings": [{"key": "c", "type": "choice", "default": "x"}]})").ok);
    CHECK(!maba::ParseTask(R"({"id": "t", "settings": [{"key": "n", "type": "int", "default": 9, "min": 0, "max": 5}]})").ok);
}

MABA_TEST(StepLanguageParsesAndRejects) {
    const auto t = Task(R"({"id": "t", "steps": [
        {"action": "if", "when": {"present": {"template": "a.png"}}, "then": [{"action": "back"}], "else": [{"action": "end", "status": "skipped"}]},
        {"action": "loop", "max": 3, "until": {"value": "tickets", "le": 0}, "steps": [{"action": "break"}]},
        {"action": "click", "template": "s.png", "pick": "bottom", "offset": {"x": 397, "y": -16}, "expect": {"template": "e.png"}, "retries": 3},
        {"action": "click", "points": [{"x": 1, "y": 2}, {"x": 3, "y": 4}]},
        {"action": "click_color", "color": {"hsv_min": [20, 200, 230], "hsv_max": [30, 255, 255], "width": [30, 40]}, "offset": {"x": 45, "y": 15}, "max": 8},
        {"action": "swipe", "from": {"x": 60, "y": 170}, "to": {"x": 150, "y": 170}},
        {"action": "fail", "message": "no"}]})");
    CHECK(t.steps.size() == 7);
    CHECK(t.steps[0].steps.size() == 1 && t.steps[0].else_steps.size() == 1 && t.steps[0].else_steps[0].end_skipped);
    CHECK(t.steps[1].until && t.steps[1].until->op == maba::CompareOp::kLessEqual);
    CHECK(t.steps[2].pick == maba::Pick::kBottom && t.steps[2].offset.y == -16 && t.steps[2].expect && t.steps[2].retries == 3);
    CHECK(t.steps[3].points.size() == 2);
    CHECK(t.steps[4].color && t.steps[4].color->max_width == 40 && t.steps[4].max == 8);

    CHECK(!maba::ParseTask(R"({"id": "t", "steps": [{"action": "if", "then": []}]})").ok);
    CHECK(!maba::ParseTask(R"({"id": "t", "steps": [{"action": "loop", "steps": []}]})").ok);
    CHECK(!maba::ParseTask(R"({"id": "t", "steps": [{"action": "loop", "max": 2, "max_value": "x", "steps": []}]})").ok);
    CHECK(!maba::ParseTask(R"({"id": "t", "steps": [{"action": "click", "template": "a.png", "pick": "middle"}]})").ok);
    CHECK(!maba::ParseTask(R"({"id": "t", "steps": [{"action": "click_color", "color": {"hsv_min": [200, 0, 0], "hsv_max": [30, 255, 255]}}]})").ok);
    CHECK(!maba::ParseTask(R"({"id": "t", "steps": [{"action": "each", "setting": "x"}]})").ok);
    CHECK(!maba::ParseTask(R"({"id": "t", "steps": [{"action": "back", "when": {"setting": "x"}}]})").ok);
}

MABA_TEST(ValidateFindsUndeclaredSettingsInNestedSteps) {
    NavRig rig;
    const auto t = Task(R"({"id": "t", "settings": [{"key": "on", "type": "bool", "default": true}], "steps": [
        {"action": "if", "when": {"setting": "on", "equals": true}, "then": [
            {"action": "goto", "page": "nowhere"},
            {"action": "back", "when": {"setting": "missing", "equals": 1}},
            {"action": "each", "setting": "on", "steps": [{"action": "back"}]}]}]})");
    const auto problems = maba::ValidateTasks({t}, rig.pages);
    CHECK(problems.size() == 3);
}

MABA_TEST(SettingsSelectBranchesAndLoopCounts) {
    NavRig rig;
    maba::RunnerOptions options;
    const auto task = Task(R"({"id": "t", "settings": [
            {"key": "tap", "type": "bool", "default": true},
            {"key": "n", "type": "int", "default": 2, "min": 0, "max": 5},
            {"key": "targets", "type": "list", "default": ["b", "a"], "choices": ["a", "b"]}],
        "steps": [
            {"action": "click", "x": 1, "y": 1, "when": {"setting": "tap", "equals": false}},
            {"action": "loop", "max_setting": "n", "steps": [{"action": "click", "points": [{"x": 10, "y": 10}, {"x": 20, "y": 20}]}]},
            {"action": "each", "setting": "targets", "cases": {
                "a": [{"action": "click", "x": 100, "y": 100}],
                "b": [{"action": "click", "x": 200, "y": 200}, {"action": "break"}, {"action": "click", "x": 9, "y": 9}]}}]})");
    std::string error;
    CHECK(maba::ResolveSettings({task}, R"({"t": {"n": 3}})", options.settings, error));
    maba::TaskRunner runner(rig.Nav(), nullptr, options);
    const auto r = runner.RunTask(task);
    CHECK(r.status == maba::TaskStatus::kSucceeded);
    const auto& c = rig.controller.clicks;
    CHECK(c.size() == 5);
    if (c.size() == 5) {
        CHECK(c[0].x == 10 && c[1].x == 20 && c[2].x == 10);  // loop index cycles the points
        CHECK(c[3].x == 200 && c[4].x == 100);                // list order, break leaves "b" only
    }
}

MABA_TEST(LoopCountAdjustedBySetting) {
    const auto clicks = [](const char* settings) {
        NavRig rig;
        maba::RunnerOptions options;
        const auto task = Task(R"({"id": "t", "settings": [{"key": "n", "type": "int", "default": 1, "min": 1, "max": 5}],
            "steps": [{"action": "loop", "max_setting": "n", "max_adjust": -1, "steps": [{"action": "click", "x": 10, "y": 10}]}]})");
        std::string error;
        CHECK(maba::ResolveSettings({task}, settings, options.settings, error));
        maba::TaskRunner runner(rig.Nav(), nullptr, options);
        CHECK(runner.RunTask(task).status == maba::TaskStatus::kSucceeded);
        return rig.controller.clicks.size();
    };
    CHECK(clicks("{}") == 0);  // default 1: the stepper already shows 1
    CHECK(clicks(R"({"t": {"n": 3}})") == 2);
    CHECK(!maba::ParseTask(R"({"id": "t", "steps": [{"action": "loop", "max": 1, "max_adjust": 99, "steps": []}]})").ok);
}

// Scenes far enough apart for ChangedShare, with the reaction check on.
struct ReactionRig : NavRig {
    ReactionRig() {
        controller.scenes[kMainScene] = maba_test::BlankFrame(maba::kNormWidth, maba::kNormHeight, 60);
        controller.scenes[kMailScene] = maba_test::BlankFrame(maba::kNormWidth, maba::kNormHeight, 160);
        matcher.Add("main.png", {60});
        matcher.Add("mail_button.png", {60}, {1100, 40, 60, 50});
        matcher.Add("mail_title.png", {160});
        options.reaction.enabled = true;
        options.reaction.interval = std::chrono::milliseconds{100};
    }
    maba::RunnerOptions options;
};

MABA_TEST(ClickWithoutReactionFailsAfterRetries) {
    ReactionRig rig;
    maba::TaskRunner runner(rig.Nav(), nullptr, rig.options);
    const auto r = runner.RunTask(Task(R"({"id": "t", "steps": [
        {"action": "click", "x": 5, "y": 5, "timeout_ms": 500},
        {"action": "fail", "message": "must not get here"}]})"));
    CHECK(r.status == maba::TaskStatus::kFailed);
    CHECK(r.message.find("no reaction") != std::string::npos);
    CHECK(rig.controller.clicks.size() == 3);  // first click and two retries
}

MABA_TEST(IgnoredClickIsRepeatedUntilTheScreenReacts) {
    ReactionRig rig;
    int seen = 0;
    rig.controller.on_click = [&](maba::Point, maba_test::FakeController& c) {
        if (++seen == 2) c.scene = NavRig::kMailScene;  // the game ignored the first click
    };
    maba::TaskRunner runner(rig.Nav(), nullptr, rig.options);
    const auto r = runner.RunTask(Task(R"({"id": "t", "steps": [{"action": "click", "x": 5, "y": 5, "timeout_ms": 500}]})"));
    CHECK(r.status == maba::TaskStatus::kSucceeded);
    CHECK(rig.controller.clicks.size() == 2);
}

MABA_TEST(ExpectVisibleBeforeTheClickNeedsAChange) {
    ReactionRig rig;
    maba::TaskRunner runner(rig.Nav(), nullptr, rig.options);
    const auto r = runner.RunTask(Task(R"({"id": "t", "steps": [
        {"action": "click", "x": 5, "y": 5, "expect": {"template": "main.png"}, "timeout_ms": 500}]})"));
    CHECK(r.status == maba::TaskStatus::kFailed);
    CHECK(rig.controller.clicks.size() == 3);
}

MABA_TEST(ToggleClickIsNotChecked) {
    ReactionRig rig;
    maba::TaskRunner runner(rig.Nav(), nullptr, rig.options);
    const auto r = runner.RunTask(Task(R"({"id": "t", "steps": [{"action": "click", "x": 5, "y": 5, "no_verify": true}]})"));
    CHECK(r.status == maba::TaskStatus::kSucceeded);
    CHECK(rig.controller.clicks.size() == 1);
}

MABA_TEST(ChangeRoiLooksOnlyThere) {
    ReactionRig rig;
    rig.controller.on_click = [](maba::Point, maba_test::FakeController& c) {
        for (int y = 0; y < 20; ++y)
            for (int x = 0; x < 20 * 3; ++x) c.scenes[c.scene].bgr[static_cast<std::size_t>(y) * maba::kNormWidth * 3 + x] = 255;
    };
    maba::TaskRunner runner(rig.Nav(), nullptr, rig.options);
    const auto r = runner.RunTask(Task(R"({"id": "t", "steps": [
        {"action": "click", "x": 5, "y": 5, "timeout_ms": 500, "change_roi": {"x": 0, "y": 0, "w": 40, "h": 40}}]})"));
    CHECK(r.status == maba::TaskStatus::kSucceeded);
    CHECK(rig.controller.clicks.size() == 1);
}

MABA_TEST(BackWithoutReactionFails) {
    ReactionRig rig;
    maba::TaskRunner runner(rig.Nav(), nullptr, rig.options);
    const auto r = runner.RunTask(Task(R"({"id": "t", "steps": [{"action": "back", "timeout_ms": 500}]})"));
    CHECK(r.status == maba::TaskStatus::kFailed);
    CHECK(r.message.find("back key") != std::string::npos);
    CHECK(rig.controller.backs == 3);
}

MABA_TEST(FixedClickOnTheWrongScreenClicksNothing) {
    ReactionRig rig;
    maba::TaskRunner runner(rig.Nav(), nullptr, rig.options);
    const auto wrong_page = runner.RunTask(Task(R"({"id": "t", "steps": [
        {"action": "click", "x": 5, "y": 5, "on_page": "mail", "timeout_ms": 300}]})"));
    CHECK(wrong_page.status == maba::TaskStatus::kFailed);
    CHECK(wrong_page.message.find("not on page") != std::string::npos);
    const auto missing = runner.RunTask(Task(R"({"id": "t", "steps": [
        {"action": "click", "x": 5, "y": 5, "require": {"template": "mail_title.png"}, "timeout_ms": 300}]})"));
    CHECK(missing.status == maba::TaskStatus::kFailed);
    CHECK(missing.message.find("not visible") != std::string::npos);
    CHECK(rig.controller.clicks.empty());
}

MABA_TEST(FixedClickNeedsAGuardInTaskData) {
    NavRig rig;
    const auto check = [&](const char* json) { return maba::ValidateTasks({Task(json)}, rig.pages).size(); };
    CHECK(check(R"({"id": "t", "steps": [{"action": "click", "x": 5, "y": 5}]})") == 1);
    CHECK(check(R"({"id": "t", "steps": [{"action": "click", "x": 5, "y": 5, "on_page": "mail"}]})") == 0);
    CHECK(check(R"({"id": "t", "steps": [{"action": "click", "x": 5, "y": 5, "on_page": "nowhere"}]})") == 1);
    CHECK(check(R"({"id": "t", "steps": [{"action": "click", "x": 5, "y": 5, "expect": {"template": "a.png"}}]})") == 0);
}

MABA_TEST(WaitStableEndsOnARestingScreen) {
    NavRig rig;
    maba::TaskRunner runner(rig.Nav(), nullptr);
    const auto r = runner.RunTask(Task(R"({"id": "t", "steps": [{"action": "wait_stable", "timeout_ms": 2000}]})"));
    CHECK(r.status == maba::TaskStatus::kSucceeded);
}

MABA_TEST(LoadingScreenIsNotARestingScreen) {
    CHECK(maba::IsLoadingScreen(maba_test::BlankFrame(1280, 720, 0)));
    CHECK(!maba::IsLoadingScreen(maba_test::BlankFrame(1280, 720, 200)));
    NavRig rig;
    rig.controller.scenes[rig.controller.scene] = maba_test::BlankFrame(maba::kNormWidth, maba::kNormHeight, 0);
    CHECK(!rig.Nav().WaitSettled(std::chrono::milliseconds{2000}));  // bounded, and not "settled"
}

MABA_TEST(NumbersAreReadWithAMarginAroundTheCrop) {
    NavRig rig;
    maba_test::FakeOcr ocr;
    maba::TaskRunner runner(rig.Nav(), &ocr);
    const auto r = runner.RunTask(Task(R"({"id": "t", "steps": [
        {"action": "read_number", "roi": {"x": 0, "y": 0, "w": 100, "h": 30}, "store": "n"}]})"));
    CHECK(r.status == maba::TaskStatus::kSucceeded);
    CHECK(ocr.last_width == 100 * 3 / 2 + 32);  // the fake crop is 1.5 times the region

    auto small = maba_test::BlankFrame(2, 2, 7);
    small.bgr[0] = 9;
    const auto wide = maba::AddMargin(small, 3);
    CHECK(wide.width == 8 && wide.height == 8);
    CHECK(wide.bgr[0] == 9);                   // corner repeats the nearest pixel
    CHECK(wide.bgr.back() == 7);
    CHECK(maba::AddMargin(small, 0).width == 2);
}

MABA_TEST(WheelStepTurnsTheWheelAndNeedsAPlatformThatHasOne) {
    NavRig rig;
    maba::TaskRunner runner(rig.Nav(), nullptr);
    const auto ok = runner.RunTask(Task(R"({"id": "t", "steps": [{"action": "wheel", "x": 640, "y": 360, "notches": -8}]})"));
    CHECK(ok.status == maba::TaskStatus::kSucceeded);
    CHECK(rig.controller.scrolled == -8);

    rig.controller.can_scroll = false;  // emulators: the zoom is a convenience, the task goes on
    const auto skipped = runner.RunTask(Task(R"({"id": "t", "steps": [
        {"action": "wheel", "x": 640, "y": 360, "notches": -8, "optional": true}]})"));
    CHECK(skipped.status == maba::TaskStatus::kSucceeded);
    CHECK(!maba::ParseTask(R"({"id": "t", "steps": [{"action": "wheel", "x": 1, "y": 1}]})").ok);
}

MABA_TEST(OptionalFailuresShowInTheResult) {
    NavRig rig;
    maba::TaskRunner runner(rig.Nav(), nullptr);
    const auto reported = runner.RunTask(Task(R"({"id": "t", "steps": [
        {"action": "wait_template", "template": "nothing.png", "timeout_ms": 100, "optional": true},
        {"action": "wait_template", "template": "nothing.png", "timeout_ms": 100, "optional": true}]})"));
    CHECK(reported.status == maba::TaskStatus::kSucceeded);
    CHECK(reported.message.find("2 step(s) did not work") != std::string::npos);
    CHECK(reported.message.find("nothing.png") != std::string::npos);

    const auto ended = runner.RunTask(Task(R"({"id": "t", "steps": [
        {"action": "wait_template", "template": "nothing.png", "timeout_ms": 100, "optional": true},
        {"action": "end", "message": "done early"}]})"));
    CHECK(ended.message.find("done early; 1 step(s)") != std::string::npos);

    const auto quiet = runner.RunTask(Task(R"({"id": "t", "steps": [
        {"action": "wait_template", "template": "nothing.png", "timeout_ms": 100, "optional": true, "quiet": true}]})"));
    CHECK(quiet.status == maba::TaskStatus::kSucceeded && quiet.message.empty());
}

MABA_TEST(ClickTextFuzzyAcceptsOneMisreadLetterOnly) {
    const auto run = [](const char* json) {
        NavRig rig;
        maba_test::FakeOcr ocr;
        ocr.results = {{"\xEC\x82\xB4\xEB\xA0\x88\xEC\x97\x85\xEB\xAC\xB4\xEA\xB4\x80", {0, 0, 100, 20}, 0.9f}};  // the misread name
        maba::TaskRunner runner(rig.Nav(), &ocr);
        const auto r = runner.RunTask(Task(json));
        return std::make_pair(r.status == maba::TaskStatus::kSucceeded, rig.controller.clicks.size());
    };
    // Wanted: the real name with its first letter, one letter from what OCR returned.
    const auto strict = run(R"({"id": "t", "steps": [{"action": "click_text", "texts": ["\uC0F4\uB808\uC5C5\uBB34\uAD00"],
        "roi": {"x": 0, "y": 0, "w": 100, "h": 30}, "max_scrolls": 0, "optional": true, "quiet": true}]})");
    CHECK(strict.second == 0);
    const auto fuzzy = run(R"({"id": "t", "steps": [{"action": "click_text", "texts": ["\uC0F4\uB808\uC5C5\uBB34\uAD00"], "fuzzy": true,
        "roi": {"x": 0, "y": 0, "w": 100, "h": 30}, "max_scrolls": 0}]})");
    CHECK(fuzzy.first && fuzzy.second == 1);
    const auto two_letters = run(R"({"id": "t", "steps": [{"action": "click_text", "texts": ["\uC0F4\uB808\uC5C5\uBB34\uC2E4"], "fuzzy": true,
        "roi": {"x": 0, "y": 0, "w": 100, "h": 30}, "max_scrolls": 0, "optional": true, "quiet": true}]})");
    CHECK(two_letters.second == 0);
}

MABA_TEST(ReadTextIsStoredInTheRecord) {
    NavRig rig;
    maba_test::FakeOcr ocr;
    ocr.results = {{"Name", {0, 0, 10, 10}, 0.9f}};
    maba::RunnerOptions options;
    options.data_dir = std::filesystem::temp_directory_path() / "maba_read_text_test";
    std::filesystem::remove_all(options.data_dir);
    maba::TaskRunner runner(rig.Nav(), &ocr, options);
    const auto r = runner.RunTask(Task(R"({"id": "t", "steps": [
        {"action": "read_text", "roi": {"x": 0, "y": 0, "w": 100, "h": 30}, "store": "nickname"},
        {"action": "save", "file": "a.json", "values": ["nickname"]}]})"));
    CHECK(r.status == maba::TaskStatus::kSucceeded);
    CHECK(r.texts.at("nickname") == "Name");
    std::ifstream in(options.data_dir / "a.json");
    const std::string saved((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    CHECK(saved.find("\"nickname\": \"Name\"") != std::string::npos);
    std::filesystem::remove_all(options.data_dir);

    ocr.results.clear();
    const auto empty = runner.RunTask(Task(R"({"id": "t", "steps": [
        {"action": "read_text", "roi": {"x": 0, "y": 0, "w": 100, "h": 30}, "store": "nickname"}]})"));
    CHECK(empty.status == maba::TaskStatus::kFailed);
}

MABA_TEST(ChangedShareCountsStrongChangesOnly) {
    const auto base = maba_test::BlankFrame(100, 100, 100);
    auto small = maba_test::BlankFrame(100, 100, 110);  // lighting: many pixels, a little
    CHECK(maba::ChangedShare(base, small) == 0.0);
    CHECK(maba::ChangedShare(base, maba_test::BlankFrame(100, 100, 200)) == 100.0);
    CHECK(maba::ChangedShare(base, maba_test::BlankFrame(50, 50, 100)) == 100.0);
    auto corner = base;
    for (int y = 0; y < 10; ++y)
        for (int x = 0; x < 30; ++x) corner.bgr[static_cast<std::size_t>(y) * 300 + x] = 255;
    CHECK(maba::ChangedShare(base, corner) < 2.0);
    CHECK(maba::ChangedShare(base, corner, maba::Rect{0, 0, 10, 10}) == 100.0);
}

MABA_TEST(IfPresentEndAndFail) {
    NavRig rig;
    maba::TaskRunner runner(rig.Nav(), nullptr);
    const auto skipped = runner.RunTask(Task(R"({"id": "t", "steps": [
        {"action": "if", "when": {"absent": {"template": "mail_title.png"}},
         "then": [{"action": "end", "status": "skipped", "message": "no mail page"}]},
        {"action": "fail", "message": "not reached"}]})"));
    CHECK(skipped.status == maba::TaskStatus::kSkipped && skipped.message == "no mail page");

    const auto failed = runner.RunTask(Task(R"({"id": "t", "steps": [
        {"action": "loop", "max": 3, "steps": [{"action": "fail", "message": "no team"}]}]})"));
    CHECK(failed.status == maba::TaskStatus::kFailed && failed.message.find("no team") != std::string::npos);

    const auto absorbed = runner.RunTask(Task(R"({"id": "t", "steps": [
        {"action": "loop", "max": 3, "optional": true, "steps": [{"action": "fail", "message": "x"}]},
        {"action": "click", "x": 5, "y": 5}]})"));
    CHECK(absorbed.status == maba::TaskStatus::kSucceeded);
}

MABA_TEST(LoopStopsOnUntilAndValue) {
    NavRig rig;
    maba::TaskRunner runner(rig.Nav(), nullptr);
    // Clicking the mail button leaves the main menu: the loop stops once main.png is gone.
    const auto r = runner.RunTask(Task(R"({"id": "t", "steps": [
        {"action": "loop", "max": 5, "until": {"absent": {"template": "main.png"}},
         "steps": [{"action": "click", "x": 1130, "y": 65}]}]})"));
    CHECK(r.status == maba::TaskStatus::kSucceeded);
    CHECK(rig.controller.clicks.size() == 1);
}

MABA_TEST(ClickPicksOffsetsAndRetriesUntilExpected) {
    NavRig rig;
    rig.matcher.entries["mail_claim.png"].more = {{1000, 300, 100, 40}, {1000, 650, 100, 40}};
    rig.controller.scene = NavRig::kMailScene;
    maba::TaskRunner runner(rig.Nav(), nullptr);
    const auto r = runner.RunTask(Task(R"({"id": "t", "steps": [
        {"action": "click", "template": "mail_claim.png", "pick": "bottom", "offset": {"x": -50, "y": 5}},
        {"action": "click", "template": "mail_claim.png", "pick": "top"}]})"));
    CHECK(r.status == maba::TaskStatus::kSucceeded);
    CHECK(rig.controller.clicks.size() == 2);
    if (rig.controller.clicks.size() == 2) {
        CHECK(rig.controller.clicks[0].x == 1000 && rig.controller.clicks[0].y == 675);
        CHECK(rig.controller.clicks[1].y == 320);
    }

    // The first click is ignored by the game; the step clicks again until the page shows.
    NavRig slow;
    int ignored = 1;
    slow.controller.on_click = [&](maba::Point p, maba_test::FakeController& c) {
        if (ignored-- > 0) return;
        if (p.x == 1130 && p.y == 65) c.scene = NavRig::kMailScene;
    };
    maba::TaskRunner slow_runner(slow.Nav(), nullptr);
    const auto s = slow_runner.RunTask(Task(R"({"id": "t", "steps": [
        {"action": "click", "template": "mail_button.png", "expect": {"template": "mail_title.png"}, "timeout_ms": 500}]})"));
    CHECK(s.status == maba::TaskStatus::kSucceeded);
    CHECK(slow.controller.clicks.size() == 2);
}

MABA_TEST(LenientReadNumberDropsTheLabel) {
    NavRig rig;
    maba_test::FakeOcr ocr;
    ocr.results = {{"tickets", {0, 0, 40, 20}, 0.9f}, {"5/5", {50, 0, 30, 20}, 0.9f}};
    maba::TaskRunner runner(rig.Nav(), &ocr);
    const auto strict = runner.RunTask(Task(R"({"id": "t", "steps": [
        {"action": "read_number", "roi": {"x": 0, "y": 0, "w": 100, "h": 30}, "store": "tickets"}]})"));
    CHECK(strict.status == maba::TaskStatus::kFailed);
    const auto lenient = runner.RunTask(Task(R"({"id": "t", "steps": [
        {"action": "read_number", "roi": {"x": 0, "y": 0, "w": 100, "h": 30}, "store": "tickets", "lenient": true},
        {"action": "loop", "max_value": "tickets", "until": {"value": "tickets", "lt": 1}, "steps": [{"action": "click", "x": 3, "y": 3}]}]})"));
    CHECK(lenient.status == maba::TaskStatus::kSucceeded);
    CHECK(lenient.values.count("tickets") && lenient.values.at("tickets") == 5 && lenient.values.at("tickets_max") == 5);
    CHECK(rig.controller.clicks.size() == 5);
}

MABA_TEST(ClickColorRetriesOnceThenSkips) {
    NavRig rig;
    // A marker that never goes away (the click misses the student): first offset, then the
    // retry offset, then it is left alone.
    Paint(rig.controller.scenes[NavRig::kMainScene], {100, 200, 35, 12}, 2, 222, 255);
    maba::TaskRunner runner(rig.Nav(), nullptr);
    const auto r = runner.RunTask(Task(R"({"id": "t", "steps": [
        {"action": "click_color", "color": {"hsv_min": [20, 200, 230], "hsv_max": [30, 255, 255], "width": [30, 40]},
         "offset": {"x": 55, "y": 0}, "retry_offset": {"x": 45, "y": 25}, "max": 10}]})"));
    CHECK(r.status == maba::TaskStatus::kSucceeded);
    CHECK(rig.controller.clicks.size() == 2);
    if (rig.controller.clicks.size() == 2) {
        CHECK(rig.controller.clicks[0].x == 117 + 55 && rig.controller.clicks[0].y == 206);
        CHECK(rig.controller.clicks[1].x == 117 + 45 && rig.controller.clicks[1].y == 206 + 25);
    }
}

MABA_TEST(ClickTextScrollsUntilFound) {
    NavRig rig;
    maba_test::FakeOcr ocr;
    // The first wanted name is not listed, the second is. CaptureRegion of the fake returns a
    // crop 1.5x the region, so OCR boxes are scaled back.
    ocr.results = {{"Aru", {30, 15, 60, 30}, 0.9f}};
    maba::TaskRunner runner(rig.Nav(), &ocr);
    auto task = Task(R"({"id": "t", "settings": [{"key": "names", "type": "list", "default": ["Hina", "Aru"]}], "steps": [
        {"action": "click_text", "text_setting": "names", "roi": {"x": 400, "y": 180, "w": 400, "h": 400}, "click_x": 770, "offset": {"x": 0, "y": 10},
         "max_scrolls": 3, "from": {"x": 640, "y": 520}, "to": {"x": 640, "y": 230}}]})");
    const auto r = runner.RunTask(task);
    CHECK(r.status == maba::TaskStatus::kSucceeded);
    CHECK(rig.controller.clicks.size() == 1);
    if (rig.controller.clicks.size() == 1) {
        CHECK(rig.controller.clicks[0].x == 770);
        CHECK(rig.controller.clicks[0].y == 180 + 20 + 10);  // (15 + 15) / 1.5 = 20 in the region
    }

    ocr.results = {{"Mika", {0, 0, 10, 10}, 0.9f}};
    rig.controller.clicks.clear();
    const auto missing = runner.RunTask(task);
    CHECK(missing.status == maba::TaskStatus::kFailed && missing.message.find("none of the texts") != std::string::npos);
    CHECK(rig.controller.clicks.empty());
    CHECK(ocr.calls >= 1 + 4);  // every page was read
}

MABA_TEST(ClickTextByStringKey) {
    NavRig rig;
    maba_test::FakeOcr ocr;
    ocr.results = {{"Lv.90", {0, 0, 30, 10}, 0.9f}, {"Aru", {0, 15, 30, 10}, 0.9f}, {"57", {40, 15, 10, 10}, 0.9f},
                   {"Hina (Swimsuit)", {0, 30, 60, 10}, 0.9f}};
    maba::RunnerOptions options;
    options.strings = {{"loc.office", "Hina(Swimsuit)"}};
    maba::TaskRunner runner(rig.Nav(), &ocr, options);
    const auto r = runner.RunTask(Task(R"({"id": "t", "settings": [{"key": "locs", "type": "list", "default": ["office"]}], "steps": [
        {"action": "each", "setting": "locs", "steps": [
            {"action": "click_text", "text_key": "loc.{value}", "roi": {"x": 0, "y": 0, "w": 300, "h": 300}}]}]})"));
    CHECK(r.status == maba::TaskStatus::kSucceeded);
    CHECK(rig.controller.clicks.size() == 1);  // spaces are ignored when comparing

    const auto missing = runner.RunTask(Task(R"({"id": "t", "steps": [
        {"action": "click_text", "text_key": "loc.none", "roi": {"x": 0, "y": 0, "w": 300, "h": 300}}]})"));
    CHECK(missing.status == maba::TaskStatus::kFailed && missing.message.find("strings/") != std::string::npos);
}

MABA_TEST(SaveWritesRecordsWithHistory) {
    NavRig rig;
    maba_test::FakeOcr ocr;
    ocr.results = {{"UID: 123456", {}, 0.9f}};
    const auto dir = std::filesystem::temp_directory_path() / "maba_save_test";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    maba::RunnerOptions options;
    options.data_dir = dir;
    maba::TaskRunner runner(rig.Nav(), &ocr, options);
    const auto task = Task(R"({"id": "t", "steps": [
        {"action": "read_number", "roi": {"x": 0, "y": 0, "w": 100, "h": 30}, "store": "uid", "lenient": true},
        {"action": "save", "file": "accounts/{uid}.json", "values": ["uid"], "history": true}]})");
    for (int run = 0; run < 2; ++run) {
        const auto r = runner.RunTask(task);
        if (r.status != maba::TaskStatus::kSucceeded) std::printf("save failed: %s\n", r.message.c_str());
        CHECK(r.status == maba::TaskStatus::kSucceeded);
    }
    const auto path = dir / "accounts" / "123456.json";
    CHECK(std::filesystem::exists(path));
    std::string text;
    {
        std::ifstream in(path);  // closed before the folder is removed (Windows locks open files)
        text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    CHECK(text.find("\"uid\": 123456") != std::string::npos);
    std::size_t entries = 0;
    for (auto pos = text.find("\"time\""); pos != std::string::npos; pos = text.find("\"time\"", pos + 1)) ++entries;
    CHECK(entries == 2);  // one history entry per run
    std::filesystem::remove_all(dir, ec);

    CHECK(!maba::ParseTask(R"({"id": "t", "steps": [{"action": "save", "file": "../x.json"}]})").ok);
    maba::TaskRunner no_dir(rig.Nav(), &ocr);
    CHECK(no_dir.RunTask(task).status == maba::TaskStatus::kFailed);
}

MABA_TEST(PopupColorConditions) {
    NavRig rig;
    // "AUTO is off": the button is there (anchor) but not lit (no yellow).
    const auto p = maba::ParsePopup(R"({"id": "auto_off", "anchors": [{"template": "main.png"}],
        "no_colors": [{"hsv_min": [20, 200, 230], "hsv_max": [30, 255, 255], "roi": {"x": 1160, "y": 650, "w": 110, "h": 50}}],
        "dismiss": {"x": 1215, "y": 677}})");
    CHECK(p.ok);
    rig.popups = {p.popup};
    rig.controller.clicks_to.clear();
    rig.controller.on_click = [](maba::Point pt, maba_test::FakeController& c) {
        if (pt.x == 1215) Paint(c.scenes[NavRig::kMainScene], {1170, 660, 90, 30}, 2, 222, 255);  // lit
    };
    const maba::Observation obs = rig.Nav().Observe();
    CHECK(obs.ok);
    CHECK(rig.controller.clicks.size() == 1);  // dismissed once, then the colour is present
    CHECK(!maba::ParsePopup(R"({"id": "x", "anchors": [{"template": "a.png"}], "colors": {}, "dismiss": "back"})").ok);
}

MABA_TEST(ClickColorTapsEveryMarker) {
    NavRig rig;
    // Two markers on the main menu; each tap clears the one it hit (the student was touched).
    maba::Frame& f = rig.controller.scenes[NavRig::kMainScene];
    Paint(f, {100, 200, 35, 12}, 2, 222, 255);
    Paint(f, {400, 400, 35, 12}, 2, 222, 255);
    rig.controller.on_click = [&](maba::Point p, maba_test::FakeController& c) {
        Paint(c.scenes[NavRig::kMainScene], {p.x - 45 - 17, p.y - 15 - 6, 35, 12}, 11, 11, 11);
    };
    maba::TaskRunner runner(rig.Nav(), nullptr);
    const auto r = runner.RunTask(Task(R"({"id": "t", "steps": [
        {"action": "click_color", "color": {"hsv_min": [20, 200, 230], "hsv_max": [30, 255, 255], "width": [30, 40], "height": [9, 15]},
         "offset": {"x": 45, "y": 15}, "max": 10}]})"));
    CHECK(r.status == maba::TaskStatus::kSucceeded);
    CHECK(rig.controller.clicks.size() == 2);
    if (rig.controller.clicks.size() == 2) {
        CHECK(rig.controller.clicks[0].x == 117 + 45 && rig.controller.clicks[0].y == 206 + 15);
    }
}

}  // namespace
