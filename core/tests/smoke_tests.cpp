// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// main_menu -> mail smoke flow against a fake controller and a fake matcher.
#include <map>
#include <set>
#include <string>

#include "fakes.h"
#include "maba/smoke.h"
#include "test_framework.h"

namespace {

using namespace std::chrono_literals;

constexpr std::uint8_t kMain = 1;
constexpr std::uint8_t kMail = 2;
constexpr std::uint8_t kPopup = 3;

// Recognizes scenes by the fill value of the frame: each template is "visible" in a set of scenes.
class FakeMatcher : public maba::IMatcher {
public:
    struct Entry {
        std::set<std::uint8_t> scenes;
        maba::Rect box;
    };

    maba::MatchResult Find(const maba::Frame& frame, const maba::TemplateRef& ref) override {
        ++finds;
        maba::MatchResult r;
        auto it = entries.find(ref.file);
        if (it == entries.end()) {
            error = "template not found: " + ref.file;
            return r;
        }
        const bool visible = !frame.empty() && it->second.scenes.count(frame.bgr[0]) > 0;
        r.score = visible ? 0.97 : 0.42;
        r.found = r.score >= ref.threshold;
        r.box = it->second.box;
        r.center = {r.box.x + r.box.width / 2, r.box.y + r.box.height / 2};
        return r;
    }
    std::vector<maba::MatchResult> FindAll(const maba::Frame& f, const maba::TemplateRef& ref,
                                           int) override {
        return {Find(f, ref)};
    }
    const std::string& LastError() const override { return error; }

    std::map<std::string, Entry> entries;
    std::string error;
    int finds = 0;
};

std::map<std::string, maba::PageDef> Pages() {
    const auto main = maba::ParsePage(R"({
      "id": "main_menu",
      "anchors": [{ "template": "main_menu_mail_icon.png" }],
      "transitions": [{ "to": "mail", "click": { "template": "main_menu_mail_icon.png" } }]
    })");
    const auto mail = maba::ParsePage(R"({"id": "mail", "anchors": [{ "template": "mail_title.png" }]})");
    CHECK(main.ok && mail.ok);
    return {{"main_menu", main.page}, {"mail", mail.page}};
}

struct Rig {
    Rig() {
        controller.scenes = {maba_test::BlankFrame(1280, 720, kMain),
                             maba_test::BlankFrame(1280, 720, kMail),
                             maba_test::BlankFrame(1280, 720, kPopup)};
        matcher.entries["main_menu_mail_icon.png"] = {{kMain}, {1100, 40, 60, 50}};
        matcher.entries["mail_title.png"] = {{kMail}, {80, 20, 200, 40}};
        options.on_failure = [this](const maba::Frame& f, const std::string& reason) {
            ++failures;
            failure_frame_value = f.empty() ? 0 : f.bgr[0];
            failure_reason = reason;
        };
    }
    maba::SmokeResult Run() {
        return maba::RunTransitionSmoke(controller, matcher, pages, options, clock);
    }

    maba_test::FakeController controller;
    FakeMatcher matcher;
    maba_test::ManualClock clock;
    std::map<std::string, maba::PageDef> pages = Pages();
    maba::SmokeOptions options;
    int failures = 0;
    int failure_frame_value = -1;
    std::string failure_reason;
};

MABA_TEST(SmokeHappyPathClicksTemplateCenter) {
    Rig rig;
    rig.controller.on_click = [](maba::Point, maba_test::FakeController& c) { c.scene = 1; };
    const auto r = rig.Run();
    CHECK(r.ok && r.error.empty());
    CHECK(r.clicked);
    CHECK(rig.controller.clicks.size() == 1);
    CHECK(rig.controller.clicks[0].x == 1130 && rig.controller.clicks[0].y == 65);
    CHECK(r.from_score > 0.9 && r.to_score > 0.9);
    CHECK(rig.failures == 0);
}

MABA_TEST(SmokeArrivesAfterLoadingFrames) {
    Rig rig;
    int clicked_at = -1;
    // After the click, three screenshots show a loading screen before the mail page.
    rig.controller.on_click = [&](maba::Point, maba_test::FakeController& c) {
        c.scene = 2;
        clicked_at = c.screenshots;
    };
    rig.controller.before_screenshot = [&](maba_test::FakeController& c) {
        if (c.scene == 2 && clicked_at >= 0 && c.screenshots - clicked_at > 3) c.scene = 1;
    };
    const auto r = rig.Run();
    CHECK(r.ok);
    CHECK(rig.controller.screenshots - clicked_at == 4);
    CHECK(rig.clock.sleeps >= 3);
}

MABA_TEST(SmokeMainNotDetectedTimesOut) {
    Rig rig;
    rig.controller.scene = 2;  // popup covers the main screen
    rig.options.detect = {1000ms, 250ms, 40};
    const auto r = rig.Run();
    CHECK(!r.ok);
    CHECK(r.error.find("'main_menu' not detected") != std::string::npos);
    CHECK(r.error.find("main_menu_mail_icon.png") != std::string::npos);
    CHECK(rig.controller.clicks.empty());
    CHECK(rig.failures == 1 && rig.failure_frame_value == kPopup);
    CHECK(rig.clock.now <= 1000ms);
}

MABA_TEST(SmokeMailNeverAppears) {
    Rig rig;  // the click does nothing
    rig.options.arrive = {900ms, 300ms, 40};
    const auto r = rig.Run();
    CHECK(!r.ok && r.clicked);
    CHECK(r.error.find("did not appear") != std::string::npos);
    CHECK(rig.failures == 1 && rig.failure_frame_value == kMain);
}

MABA_TEST(SmokeDryRunDoesNotClick) {
    Rig rig;
    rig.options.dry_run = true;
    const auto r = rig.Run();
    CHECK(r.ok && !r.clicked);
    CHECK(r.click_point.x == 1130);
    CHECK(rig.controller.clicks.empty());
}

MABA_TEST(SmokeConfigErrors) {
    Rig rig;
    rig.options.to_page = "shop";
    CHECK(rig.Run().error.find("unknown page: shop") != std::string::npos);

    Rig rig2;
    rig2.options.from_page = "mail";
    rig2.options.to_page = "main_menu";
    CHECK(rig2.Run().error.find("no transition") != std::string::npos);

    Rig rig3;
    rig3.matcher.entries.erase("main_menu_mail_icon.png");
    rig3.options.detect = {500ms, 250ms, 40};
    const auto r3 = rig3.Run();
    CHECK(!r3.ok && r3.error.find("template not found") != std::string::npos);
}

MABA_TEST(SmokeEmptyFramesAreBounded) {
    Rig rig;
    rig.controller.scenes[0] = maba::Frame{};
    rig.options.detect = {10000ms, 100ms, 5};
    const auto r = rig.Run();
    CHECK(!r.ok && r.error.find("no frame captured (5 attempts)") != std::string::npos);
    CHECK(rig.controller.screenshots == 5);
}

MABA_TEST(DetectPageRequiresEveryAnchor) {
    FakeMatcher m;
    m.entries["a.png"] = {{kMain}, {}};
    m.entries["b.png"] = {{kMail}, {}};
    maba::PageDef page;
    page.id = "p";
    page.anchors = {{"a.png", std::nullopt, 0.9}, {"b.png", std::nullopt, 0.9}};
    const auto d = maba::DetectPage(m, maba_test::BlankFrame(1280, 720, kMain), page);
    CHECK(!d.detected && d.failed_anchor == "b.png" && d.min_score < 0.5);
    page.anchors.pop_back();
    CHECK(maba::DetectPage(m, maba_test::BlankFrame(1280, 720, kMain), page).detected);
}

}  // namespace
