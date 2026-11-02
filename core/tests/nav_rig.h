// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// A small fake game: main menu, mail, shop, a notice popup over the main menu and an unknown
// screen. Shared by the Navigator and task-runner tests.
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "fakes.h"
#include "maba/navigator.h"
#include "maba/page.h"
#include "maba/popup.h"
#include "test_framework.h"

namespace maba_test {

struct NavRig {
    enum : std::size_t { kUnknownScene = 0, kMainScene, kMailScene, kShopScene, kPopupScene };

    NavRig() : controller({10, 11, 12, 14, 13}) {
        matcher.Add("main.png", {11});
        matcher.Add("mail_button.png", {11}, {1100, 40, 60, 50});  // center (1130, 65)
        matcher.Add("mail_title.png", {12});
        matcher.Add("mail_claim.png", {12}, {1000, 600, 100, 40});  // center (1050, 620)
        matcher.Add("shop_title.png", {14});
        matcher.Add("notice.png", {13});

        AddPage(R"({"id": "main_menu", "anchors": [{"template": "main.png"}],
                    "transitions": [{"to": "mail", "click": {"template": "mail_button.png"}},
                                    {"to": "shop", "click": {"x": 600, "y": 650}}]})");
        AddPage(R"({"id": "mail", "anchors": [{"template": "mail_title.png"}],
                    "transitions": [{"to": "main_menu", "click": {"x": 40, "y": 40}}]})");
        AddPage(R"({"id": "shop", "anchors": [{"template": "shop_title.png"}],
                    "transitions": [{"to": "main_menu", "click": {"x": 40, "y": 40}}]})");
        const auto popup = maba::ParsePopup(
            R"({"id": "notice", "anchors": [{"template": "notice.png"}], "dismiss": {"x": 640, "y": 600}})");
        CHECK(popup.ok);
        popups.push_back(popup.popup);

        controller.clicks_to = {
            {kMainScene, {1130, 65}, kMailScene},
            {kMainScene, {600, 650}, kShopScene},
            {kMailScene, {40, 40}, kMainScene},
            {kShopScene, {40, 40}, kMainScene},
            {kPopupScene, {640, 600}, kMainScene},
        };
        controller.scene = kMainScene;
        nav_options.after_back = std::chrono::milliseconds{100};
    }

    void AddPage(const char* json) {
        const auto p = maba::ParsePage(json);
        CHECK(p.ok);
        pages[p.page.id] = p.page;
    }

    maba::Navigator& Nav() {
        handler = std::make_unique<maba::PopupHandler>(matcher, popups, popup_options);
        nav = std::make_unique<maba::Navigator>(controller, matcher, pages, *handler, clock, nav_options);
        return *nav;
    }

    SceneController controller;
    SceneMatcher matcher;
    ManualClock clock;
    std::map<std::string, maba::PageDef> pages;
    std::vector<maba::PopupDef> popups;
    maba::PopupOptions popup_options;
    maba::NavigatorOptions nav_options;
    std::unique_ptr<maba::PopupHandler> handler;
    std::unique_ptr<maba::Navigator> nav;
};

}  // namespace maba_test
