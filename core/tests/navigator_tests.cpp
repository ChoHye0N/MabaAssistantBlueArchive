// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Popup handler, page graph and Navigator against scene fakes.
#include <atomic>
#include <chrono>
#include <map>
#include <string>

#include "fakes.h"
#include "maba/detect.h"
#include "maba/navigator.h"
#include "maba/popup.h"
#include "nav_rig.h"
#include "test_framework.h"

namespace {

using maba_test::NavRig;

MABA_TEST(FindPathUsesShortestRoute) {
    NavRig rig;
    const auto path = maba::FindPath(rig.pages, "mail", "shop");
    CHECK(path.size() == 3);
    CHECK(path.size() == 3 && path[0] == "mail" && path[1] == "main_menu" && path[2] == "shop");
    CHECK(maba::FindPath(rig.pages, "mail", "mail").size() == 1);
    CHECK(maba::FindPath(rig.pages, "mail", "nowhere").empty());
}

MABA_TEST(ValidatePageGraphReportsUnknownTargets) {
    NavRig rig;
    CHECK(maba::ValidatePageGraph(rig.pages, "main_menu").empty());
    rig.pages["shop"].transitions.push_back({"missing", std::nullopt, maba::Point{1, 1}});
    const auto problems = maba::ValidatePageGraph(rig.pages, "home");
    CHECK(problems.size() == 2);
}

MABA_TEST(GoToDismissesPopupThenNavigates) {
    NavRig rig;
    rig.controller.scene = NavRig::kPopupScene;
    const maba::NavResult r = rig.Nav().GoTo("mail");
    CHECK(r.ok);
    CHECK(r.page == "mail");
    CHECK(rig.controller.scene == NavRig::kMailScene);
    // popup dismiss click, then the mail button
    CHECK(rig.controller.clicks.size() == 2);
    CHECK(rig.controller.clicks.size() == 2 && rig.controller.clicks[1].x == 1130);
}

MABA_TEST(GoToTwoHopsThroughHome) {
    NavRig rig;
    rig.controller.scene = NavRig::kMailScene;
    const maba::NavResult r = rig.Nav().GoTo("shop");
    CHECK(r.ok);
    CHECK(rig.controller.scene == NavRig::kShopScene);
    CHECK(r.steps == 2);
}

MABA_TEST(GoToPressesBackOnUnknownScreen) {
    NavRig rig;
    rig.controller.scene = NavRig::kUnknownScene;
    rig.controller.back_to[NavRig::kUnknownScene] = NavRig::kMainScene;
    const maba::NavResult r = rig.Nav().ReturnHome();
    CHECK(r.ok);
    CHECK(rig.controller.backs == 1);
    CHECK(r.page == "main_menu");
}

MABA_TEST(GoToWaitsOutABattleWithoutPressingBack) {
    NavRig rig;
    rig.matcher.Add("hud.png", {10});
    rig.AddPage(R"({"id": "battle", "wait_ms": 8000, "anchors": [{"template": "hud.png"}]})");
    rig.controller.scene = NavRig::kUnknownScene;
    const auto r = rig.Nav().GoTo("main_menu");
    CHECK(!r.ok);
    CHECK(r.error.find("still waiting") != std::string::npos);
    CHECK(rig.controller.backs == 0);
}

MABA_TEST(GoToFailsOnEndlessUnknownScreenAndReportsFrame) {
    NavRig rig;
    rig.controller.scene = NavRig::kUnknownScene;  // back does nothing
    int failures = 0;
    rig.nav_options.on_failure = [&](const maba::Frame& f, const std::string&) {
        ++failures;
        CHECK(!f.empty());
    };
    const maba::NavResult r = rig.Nav().GoTo("mail");
    CHECK(!r.ok);
    CHECK(rig.controller.backs == rig.nav_options.max_back_presses);
    CHECK(failures == 1);
    CHECK(r.error.find("not recognized") != std::string::npos);
}

MABA_TEST(GoToRespectsStepBudgetWhenClicksDoNothing) {
    NavRig rig;
    rig.controller.clicks_to.clear();  // the game ignores clicks
    rig.controller.scene = NavRig::kMainScene;
    const maba::NavResult r = rig.Nav().GoTo("mail");
    CHECK(!r.ok);
    CHECK(static_cast<int>(rig.controller.clicks.size()) == rig.nav_options.max_steps);
}

MABA_TEST(GoToStopsWhenCancelled) {
    NavRig rig;
    std::atomic<bool> cancel{true};
    rig.nav_options.cancel = &cancel;
    rig.controller.scene = NavRig::kMainScene;
    const maba::NavResult r = rig.Nav().GoTo("mail");
    CHECK(!r.ok);
    CHECK(r.cancelled);
    CHECK(rig.controller.clicks.empty());
}

MABA_TEST(EnterGameTapsThroughStartUpScreens) {
    NavRig rig;
    rig.pages["main_menu"].enter = maba::EnterSpec{{640, 100}, {}, 5, std::chrono::milliseconds{100}};
    rig.controller.scene = NavRig::kUnknownScene;  // e.g. an event splash: back does nothing
    rig.controller.clicks_to.push_back({NavRig::kUnknownScene, {640, 100}, NavRig::kMainScene});
    const maba::NavResult r = rig.Nav().EnterGame();
    CHECK(r.ok);
    CHECK(r.page == "main_menu");
    CHECK(r.steps == 1);
    CHECK(rig.controller.clicks.size() == 1);
    CHECK(rig.controller.backs == 0);
}

MABA_TEST(EnterGameTapsAScreenAtItsOwnPoint) {
    NavRig rig;
    rig.matcher.Add("title.png", {10});  // the unknown scene is a title screen
    maba::EnterSpec enter{{640, 100}, {}, 5, std::chrono::milliseconds{100}};
    enter.screens.push_back({maba::TemplateRef{"title.png"}, {305, 531}});
    enter.screens.push_back({maba::TemplateRef{"never.png"}, {1, 1}});
    rig.pages["main_menu"].enter = enter;
    rig.controller.scene = NavRig::kUnknownScene;
    rig.controller.clicks_to.push_back({NavRig::kUnknownScene, {305, 531}, NavRig::kMainScene});
    const maba::NavResult r = rig.Nav().EnterGame();
    CHECK(r.ok && r.page == "main_menu");
    CHECK(rig.controller.clicks.size() == 1 && rig.controller.clicks[0].x == 305);
    CHECK(maba::ParsePage(R"({"id": "p", "anchors": [{"template": "a.png"}],
        "enter": {"tap": {"x": 1, "y": 1}, "screens": [{"when": {"template": "t.png"}, "tap": {"x": 2, "y": 3}}]}})").ok);
    CHECK(!maba::ParsePage(R"({"id": "p", "anchors": [{"template": "a.png"}],
        "enter": {"tap": {"x": 1, "y": 1}, "screens": [{"tap": {"x": 2, "y": 3}}]}})").ok);
}

MABA_TEST(EnterGameWaitsOnLoadingScreensWithoutTapping) {
    NavRig rig;
    rig.matcher.Add("loading.png", {10});  // the unknown scene shows the loading text
    maba::EnterSpec enter{{640, 100}, {}, 2, std::chrono::milliseconds{100}};
    enter.loading.push_back(maba::TemplateRef{"loading.png"});
    enter.max_loading = std::chrono::milliseconds{500};
    rig.pages["main_menu"].enter = enter;
    rig.controller.scene = NavRig::kUnknownScene;
    const maba::NavResult r = rig.Nav().EnterGame();
    CHECK(!r.ok);
    CHECK(r.error.find("still loading") != std::string::npos);
    CHECK(rig.controller.clicks.empty());  // loading rounds neither tap nor use up max_taps
    CHECK(maba::ParsePage(R"({"id": "p", "anchors": [{"template": "a.png"}],
        "enter": {"tap": {"x": 1, "y": 1}, "loading": [{"template": "l.png"}], "max_loading_ms": 60000}})").ok);
    CHECK(!maba::ParsePage(R"({"id": "p", "anchors": [{"template": "a.png"}],
        "enter": {"tap": {"x": 1, "y": 1}, "loading": {"template": "l.png"}}})").ok);
}

MABA_TEST(EnterGameTriesAgainWhenATapIsRefused) {
    NavRig rig;
    rig.pages["main_menu"].enter = maba::EnterSpec{{640, 100}, {}, 5, std::chrono::milliseconds{100}};
    rig.controller.scene = NavRig::kUnknownScene;
    rig.controller.refuse_clicks = 2;  // the game is loading and does not take input yet
    rig.controller.clicks_to.push_back({NavRig::kUnknownScene, {640, 100}, NavRig::kMainScene});
    const maba::NavResult r = rig.Nav().EnterGame();
    CHECK(r.ok && r.page == "main_menu");
    CHECK(r.steps == 3);
    CHECK(rig.controller.clicks.size() == 1);
}

MABA_TEST(EnterGameGivesUpAfterMaxTaps) {
    NavRig rig;
    rig.pages["main_menu"].enter = maba::EnterSpec{{640, 100}, {}, 5, std::chrono::milliseconds{100}};
    rig.controller.scene = NavRig::kUnknownScene;  // taps do nothing
    int failures = 0;
    rig.nav_options.on_failure = [&](const maba::Frame&, const std::string&) { ++failures; };
    const maba::NavResult r = rig.Nav().EnterGame();
    CHECK(!r.ok);
    CHECK(rig.controller.clicks.size() == 5);
    CHECK(failures == 1);
    CHECK(r.error.find("no known page") != std::string::npos);
}

MABA_TEST(EnterGameDoesNotTapOnKnownPagesOrWithoutEnter) {
    NavRig known;
    known.pages["main_menu"].enter = maba::EnterSpec{{640, 100}, {}, 5, std::chrono::milliseconds{100}};
    known.controller.scene = NavRig::kMailScene;
    const maba::NavResult a = known.Nav().EnterGame();
    CHECK(a.ok && a.page == "mail");
    CHECK(known.controller.clicks.empty());

    NavRig popup;  // a popup over the home page is dismissed, not tapped through
    popup.pages["main_menu"].enter = maba::EnterSpec{{640, 100}, {}, 5, std::chrono::milliseconds{100}};
    popup.controller.scene = NavRig::kPopupScene;
    const maba::NavResult b = popup.Nav().EnterGame();
    CHECK(b.ok && b.page == "main_menu" && b.steps == 0);

    NavRig plain;  // no enter entry: nothing to do
    plain.controller.scene = NavRig::kUnknownScene;
    CHECK(plain.Nav().EnterGame().ok);
    CHECK(plain.controller.clicks.empty());
}

MABA_TEST(PopupSettleGivesUpOnStuckPopup) {
    NavRig rig;
    rig.controller.clicks_to.clear();  // dismiss click has no effect
    rig.controller.scene = NavRig::kPopupScene;
    maba::PopupHandler handler(rig.matcher, rig.popups, {3, std::chrono::milliseconds{10}});
    const maba::SettleResult r = handler.Settle(rig.controller, rig.clock);
    CHECK(!r.ok);
    CHECK(r.dismissed == 3);
    CHECK(r.error.find("notice") != std::string::npos);
}

MABA_TEST(PopupBackDismissal) {
    NavRig rig;
    const auto p = maba::ParsePopup(R"({"id": "quit", "anchors": [{"template": "notice.png"}],
                                       "dismiss": "back"})");
    CHECK(p.ok);
    rig.controller.scene = NavRig::kPopupScene;
    rig.controller.back_to[NavRig::kPopupScene] = NavRig::kMainScene;
    maba::PopupHandler handler(rig.matcher, {p.popup});
    const maba::SettleResult r = handler.Settle(rig.controller, rig.clock);
    CHECK(r.ok);
    CHECK(r.dismissed == 1);
    CHECK(rig.controller.backs == 1);
}

MABA_TEST(PopupParseRejectsBadDismiss) {
    CHECK(!maba::ParsePopup(R"({"id": "a", "anchors": [{"template": "x.png"}]})").ok);
    CHECK(!maba::ParsePopup(R"({"id": "a", "anchors": [{"template": "x.png"}], "dismiss": "close"})").ok);
    CHECK(!maba::ParsePopup(R"({"id": "a", "anchors": [], "dismiss": "back"})").ok);
    CHECK(!maba::ParsePopup(R"({"id": "a", "anchors": [{"template": "x.png"}], "dismiss": {"x": 2000, "y": 1}})").ok);
    const auto ok = maba::ParsePopup(
        R"({"id": "a", "priority": 5, "anchors": [{"template": "x.png"}], "dismiss": {"template": "close.png"}})");
    CHECK(ok.ok && ok.popup.priority == 5 && ok.popup.click_template.has_value());
}

MABA_TEST(GameLanguageGuessNeedsAPageOfThatLanguage) {
    std::map<std::string, maba::PageDef> pages;
    for (const char* json : {R"({"id": "main_menu", "anchors": [{"template": "home_icon.png"}]})",
                             R"({"id": "mail", "anchors": [{"template": "mail_title.png"}]})"}) {
        const auto p = maba::ParsePage(json);
        CHECK(p.ok);
        pages[p.page.id] = p.page;
    }
    const maba::Frame home = maba_test::BlankFrame(maba::kNormWidth, maba::kNormHeight, 1);
    const maba::Frame mail = maba_test::BlankFrame(maba::kNormWidth, maba::kNormHeight, 2);
    maba_test::SceneMatcher korean, english;
    korean.Add("home_icon.png", {1});   // shared by every language
    english.Add("home_icon.png", {1});
    korean.Add("mail_title.png", {});   // the Korean title is not on this English screen
    english.Add("mail_title.png", {2});
    // mail_title.png exists for English; home_icon.png only in the shared folder.
    const auto own = [](const std::string& lang, const std::string& file) { return lang == "en" && file == "mail_title.png"; };
    const std::vector<std::pair<std::string, maba::IMatcher*>> others = {{"en", &english}};

    const auto guess = maba::GuessGameLanguage(korean, others, mail, pages, own);
    CHECK(guess && guess->language == "en" && guess->page == "mail");
    // A page the set language sees, or one found only through shared templates, proves nothing.
    CHECK(!maba::GuessGameLanguage(korean, others, home, pages, own));
    CHECK(!maba::GuessGameLanguage(korean, others, maba_test::BlankFrame(maba::kNormWidth, maba::kNormHeight, 9), pages, own));
}

}  // namespace

// A window that is not a known popup over the home page: the page still matches under it, and a
// bright spot of the page tells it apart. Navigation taps a free spot, then presses back.
namespace {
struct CoveredRig {
    enum : std::size_t { kCovered = 0, kClear = 1, kMail = 2 };
    CoveredRig() : controller({40, 250, 12}) {
        matcher.Add("main.png", {40, 250});
        matcher.Add("mail_button.png", {40, 250}, {1100, 40, 60, 50});  // center (1130, 65)
        matcher.Add("mail_title.png", {12});
        auto main = maba::ParsePage(R"({"id": "main_menu", "anchors": [{"template": "main.png"}],
            "clear": {"hsv_min": [0, 0, 200], "hsv_max": [180, 60, 255], "roi": {"x": 296, "y": 0, "w": 9, "h": 5}},
            "clear_tap": {"x": 300, "y": 2},
            "transitions": [{"to": "mail", "click": {"template": "mail_button.png"}}]})");
        auto mail = maba::ParsePage(R"({"id": "mail", "anchors": [{"template": "mail_title.png"}],
            "transitions": [{"to": "main_menu", "click": {"x": 40, "y": 40}}]})");
        CHECK(main.ok && mail.ok);
        pages[main.page.id] = main.page;
        pages[mail.page.id] = mail.page;
        controller.clicks_to = {{kClear, {1130, 65}, kMail}, {kCovered, {1130, 65}, kCovered}};
        controller.scene = kCovered;
        nav_options.after_back = std::chrono::milliseconds{100};
    }
    maba::Navigator& Nav() {
        handler = std::make_unique<maba::PopupHandler>(matcher, popups, popup_options);
        nav = std::make_unique<maba::Navigator>(controller, matcher, pages, *handler, clock, nav_options);
        return *nav;
    }
    maba_test::SceneController controller;
    maba_test::SceneMatcher matcher;
    maba_test::ManualClock clock;
    std::map<std::string, maba::PageDef> pages;
    std::vector<maba::PopupDef> popups;
    maba::PopupOptions popup_options;
    maba::NavigatorOptions nav_options;
    std::unique_ptr<maba::PopupHandler> handler;
    std::unique_ptr<maba::Navigator> nav;
};
}  // namespace

MABA_TEST(CoveredPageIsClearedByATapBeforeNavigating) {
    CoveredRig rig;
    rig.controller.clicks_to.push_back({CoveredRig::kCovered, {300, 2}, CoveredRig::kClear});
    const auto r = rig.Nav().GoTo("mail");
    CHECK(r.ok);
    CHECK(rig.controller.clicks.size() == 2);
    CHECK(rig.controller.clicks.size() == 2 && rig.controller.clicks[0].x == 300 && rig.controller.clicks[0].y == 2);
    CHECK(rig.controller.backs == 0);
}

MABA_TEST(CoveredPageFallsBackToBackAndIsBounded) {
    CoveredRig rig;
    rig.controller.back_to[CoveredRig::kCovered] = CoveredRig::kClear;  // a dialog that only back closes
    const auto r = rig.Nav().GoTo("mail");
    CHECK(r.ok);
    CHECK(rig.controller.backs == 1);
    CHECK(rig.controller.clicks.size() == 3);  // two taps, then the mail button
    // A window nothing closes: four tries, then navigation goes on as before (and fails here).
    CoveredRig stuck;
    const auto s = stuck.Nav().GoTo("mail");
    CHECK(!s.ok);
    CHECK(stuck.controller.backs >= 2);
    std::size_t taps = 0;
    for (const auto& c : stuck.controller.clicks) taps += c.x == 300 && c.y == 2;
    CHECK(taps == 2);
}
