// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Popup handler, page graph and Navigator against scene fakes.
#include <atomic>
#include <map>
#include <string>

#include "fakes.h"
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

}  // namespace
