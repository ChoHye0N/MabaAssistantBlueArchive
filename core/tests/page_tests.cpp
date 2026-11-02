// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#include <string>

#include "maba/page.h"
#include "test_framework.h"

namespace {

bool HasError(const maba::PageLoadResult& r, const char* text) {
    return !r.ok && r.error.find(text) != std::string::npos;
}

MABA_TEST(PageParsesFullExample) {
    const auto r = maba::ParsePage(R"({
      "id": "main_menu",
      "anchors": [
        { "template": "main_menu_mail_icon.png", "roi": { "x": 1000, "y": 0, "w": 280, "h": 120 }, "threshold": 0.85 },
        { "template": "main_menu_cafe_icon.png" }
      ],
      "rois": { "ap_value": { "x": 0, "y": 0, "w": 0, "h": 0 } },
      "strings": ["main_menu.title"],
      "transitions": [
        { "to": "mail", "click": { "template": "main_menu_mail_icon.png" } },
        { "to": "cafe", "click": { "x": 100, "y": 650 } }
      ]
    })");
    CHECK(r.ok);
    CHECK(r.page.id == "main_menu");
    CHECK(r.page.anchors.size() == 2);
    CHECK(r.page.anchors[0].roi.has_value() && r.page.anchors[0].roi->x == 1000);
    CHECK(r.page.anchors[0].threshold == 0.85);
    CHECK(!r.page.anchors[1].roi.has_value());
    CHECK(r.page.anchors[1].threshold == maba::kDefaultThreshold);

    const maba::Transition* mail = r.page.FindTransition("mail");
    CHECK(mail && mail->click_template && mail->click_template->file == "main_menu_mail_icon.png");
    const maba::Transition* cafe = r.page.FindTransition("cafe");
    CHECK(cafe && cafe->click_point && cafe->click_point->y == 650);
    CHECK(r.page.FindTransition("shop") == nullptr);
}

MABA_TEST(PageZeroRoiMeansUnset) {
    const auto r = maba::ParsePage(
        R"({"id":"p","anchors":[{"template":"a.png","roi":{"x":0,"y":0,"w":0,"h":0}}]})");
    CHECK(r.ok && !r.page.anchors[0].roi.has_value());
}

MABA_TEST(PageParsesEnter) {
    const auto r = maba::ParsePage(
        R"({"id":"p","anchors":[{"template":"a.png"}],"enter":{"tap":{"x":640,"y":100},"max_taps":5,"interval_ms":500}})");
    CHECK(r.ok && r.page.enter.has_value());
    if (r.ok && r.page.enter) {
        CHECK(r.page.enter->tap.x == 640 && r.page.enter->tap.y == 100);
        CHECK(r.page.enter->max_taps == 5);
        CHECK(r.page.enter->interval.count() == 500);
    }
    const auto d = maba::ParsePage(R"({"id":"p","anchors":[{"template":"a.png"}],"enter":{"tap":{"x":1,"y":1}}})");
    CHECK(d.ok && d.page.enter && d.page.enter->max_taps == 30 && d.page.enter->interval.count() == 2000);
    CHECK(maba::ParsePage(R"({"id":"p","anchors":[{"template":"a.png"}]})").page.enter == std::nullopt);

    CHECK(HasError(maba::ParsePage(R"({"id":"p","anchors":[{"template":"a.png"}],"enter":[]})"), "'enter'"));
    CHECK(HasError(maba::ParsePage(R"({"id":"p","anchors":[{"template":"a.png"}],"enter":{}})"), "'tap'"));
    CHECK(HasError(maba::ParsePage(
                       R"({"id":"p","anchors":[{"template":"a.png"}],"enter":{"tap":{"x":1280,"y":0}}})"),
                   "inside 1280x720"));
    CHECK(HasError(maba::ParsePage(
                       R"({"id":"p","anchors":[{"template":"a.png"}],"enter":{"tap":{"x":1,"y":1},"max_taps":0}})"),
                   "max_taps"));
}

MABA_TEST(PageRejectsInvalid) {
    CHECK(HasError(maba::ParsePage("{"), "invalid JSON"));
    CHECK(HasError(maba::ParsePage(R"({"anchors":[]})"), "'id'"));
    CHECK(HasError(maba::ParsePage(R"({"id":"p","anchors":[]})"), "non-empty"));
    CHECK(HasError(maba::ParsePage(R"({"id":"p","anchors":[{"roi":null}]})"), "'template'"));
    CHECK(HasError(maba::ParsePage(R"({"id":"p","anchors":[{"template":"a.png","threshold":1.5}]})"),
                   "threshold"));
    CHECK(HasError(maba::ParsePage(R"({"id":"p","anchors":[{"template":"a.png","threshold":0}]})"),
                   "threshold"));
    CHECK(HasError(maba::ParsePage(
                       R"({"id":"p","anchors":[{"template":"a.png","roi":{"x":1200,"y":0,"w":100,"h":10}}]})"),
                   "inside 1280x720"));
    CHECK(HasError(maba::ParsePage(
                       R"({"id":"p","anchors":[{"template":"a.png"}],"transitions":[{"to":"m","click":{}}]})"),
                   "needs 'template'"));
    CHECK(HasError(maba::ParsePage(
                       R"({"id":"p","anchors":[{"template":"a.png"}],"transitions":[{"to":"m","click":{"x":1280,"y":0}}]})"),
                   "inside 1280x720"));
    CHECK(HasError(maba::ParsePage(
                       R"({"id":"p","anchors":[{"template":"a.png","roi":{"x":"1","y":0,"w":1,"h":1}}]})"),
                   "integer"));
}

}  // namespace
