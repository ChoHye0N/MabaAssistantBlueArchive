// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Coordinate normalization, task ordering and interface smoke checks.
#include <string>
#include <vector>

#include "fakes.h"
#include "maba/coords.h"
#include "maba/task_order.h"
#include "test_framework.h"

namespace {

using maba::Point;
using maba::Size;
using maba::TaskDef;

MABA_TEST(FakesCompileAndWork) {
    maba_test::FakeController c;
    CHECK(!c.Screenshot().empty());
    CHECK(c.Screenshot().width == maba::kNormWidth);
    CHECK(c.CaptureRegion({0, 0, 100, 40}).width == 150);  // native-resolution crop
    CHECK(c.Click({1, 1}) && c.clicks.size() == 1);
    maba_test::FakeOcr ocr;
    const auto r = ocr.Recognize(c.Screenshot(), {0, 0, 10, 10}, "ko");
    CHECK(r.size() == 1 && r[0].text == "123");
}

MABA_TEST(CoordsIdentity) {
    const Point p = maba::ToNative({640, 360}, {1280, 720});
    CHECK(p.x == 640 && p.y == 360);
}

MABA_TEST(CoordsScaleUp) {
    const Size native{1920, 1080};
    Point p = maba::ToNative({640, 360}, native);
    CHECK(p.x == 960 && p.y == 540);
    p = maba::ToNative({0, 0}, native);
    CHECK(p.x == 0 && p.y == 0);
    p = maba::ToNative({1280, 720}, native);  // out of range -> clamped to last pixel
    CHECK(p.x == 1919 && p.y == 1079);
}

MABA_TEST(CoordsRoundTrip) {
    const Size native{1920, 1080};
    const Point n = maba::ToNormalized({960, 540}, native);
    CHECK(n.x == 640 && n.y == 360);
}

MABA_TEST(CoordsInvalidSize) {
    const Point p = maba::ToNative({10, 10}, {0, 0});
    CHECK(p.x == 0 && p.y == 0);
}

MABA_TEST(Aspect) {
    CHECK(maba::IsSupportedAspect({1920, 1080}));
    CHECK(maba::IsSupportedAspect({1280, 720}));
    CHECK(maba::IsSupportedAspect({2560, 1440}));
    CHECK(!maba::IsSupportedAspect({1280, 800}));
    CHECK(!maba::IsSupportedAspect({0, 720}));
}

MABA_TEST(OrderKeepsInputOrder) {
    const std::vector<TaskDef> tasks = {
        {"cafe", {}}, {"schedule", {}}, {"shop", {}},
        {"spend_ap", {}}, {"mail", {"spend_ap"}}, {"mission", {"mail"}}};
    const auto r = maba::ResolveOrder(tasks);
    CHECK(r.ok);
    const std::vector<std::string> expected = {"cafe", "schedule", "shop",
                                               "spend_ap", "mail", "mission"};
    CHECK(r.order == expected);
}

MABA_TEST(OrderReordersForDependency) {
    const std::vector<TaskDef> tasks = {{"mail", {"spend_ap"}}, {"spend_ap", {}}};
    const auto r = maba::ResolveOrder(tasks);
    CHECK(r.ok);
    const std::vector<std::string> expected = {"spend_ap", "mail"};
    CHECK(r.order == expected);
}

MABA_TEST(OrderErrors) {
    auto r = maba::ResolveOrder({{"a", {"missing"}}});
    CHECK(!r.ok && r.error.find("unknown") != std::string::npos);

    r = maba::ResolveOrder({{"a", {}}, {"a", {}}});
    CHECK(!r.ok && r.error.find("duplicate") != std::string::npos);

    r = maba::ResolveOrder({{"a", {"b"}}, {"b", {"a"}}});
    CHECK(!r.ok && r.error.find("cycle") != std::string::npos);

    r = maba::ResolveOrder({{"a", {"a"}}});
    CHECK(!r.ok && r.error.find("cycle") != std::string::npos);
}

}  // namespace
