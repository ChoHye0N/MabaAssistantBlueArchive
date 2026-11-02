// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Minimal dependency-free test runner. Replace with GoogleTest/Catch2 if the suite grows.
#include <cstdio>
#include <string>
#include <vector>

#include "maba/controller.h"
#include "maba/coords.h"
#include "maba/ocr.h"
#include "maba/task_order.h"

namespace {

int g_failures = 0;

#define CHECK(cond)                                                                \
    do {                                                                           \
        if (!(cond)) {                                                             \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);            \
            ++g_failures;                                                          \
        }                                                                          \
    } while (0)

using maba::Point;
using maba::Size;
using maba::TaskDef;

// Compile-time check that the interfaces are implementable, and a template for test fakes.
class FakeController : public maba::IController {
public:
    maba::Size NativeSize() const override { return {1920, 1080}; }
    maba::Frame Screenshot() override { return Blank(maba::kNormWidth, maba::kNormHeight); }
    maba::Frame CaptureRegion(maba::Rect r) override { return Blank(r.width * 3 / 2, r.height * 3 / 2); }
    bool Click(Point) override { ++clicks; return true; }
    bool Swipe(Point, Point, int) override { return true; }
    bool Back() override { return true; }
    int clicks = 0;

private:
    static maba::Frame Blank(int w, int h) {
        maba::Frame f;
        f.width = w;
        f.height = h;
        f.bgr.assign(static_cast<std::size_t>(w) * h * 3, 0);
        return f;
    }
};

class FakeOcr : public maba::IOcr {
public:
    std::vector<maba::OcrResult> Recognize(const maba::Frame&, const maba::Rect&,
                                          const std::string&) override {
        return {{"123", {}, 0.9f}};
    }
};

void TestFakesCompileAndWork() {
    FakeController c;
    CHECK(!c.Screenshot().empty());
    CHECK(c.Screenshot().width == maba::kNormWidth);
    CHECK(c.CaptureRegion({0, 0, 100, 40}).width == 150);  // native-resolution crop
    CHECK(c.Click({1, 1}) && c.clicks == 1);
    FakeOcr ocr;
    const auto r = ocr.Recognize(c.Screenshot(), {0, 0, 10, 10}, "ko");
    CHECK(r.size() == 1 && r[0].text == "123");
}

void TestCoordsIdentity() {
    const Point p = maba::ToNative({640, 360}, {1280, 720});
    CHECK(p.x == 640 && p.y == 360);
}

void TestCoordsScaleUp() {
    const Size native{1920, 1080};
    Point p = maba::ToNative({640, 360}, native);
    CHECK(p.x == 960 && p.y == 540);
    p = maba::ToNative({0, 0}, native);
    CHECK(p.x == 0 && p.y == 0);
    p = maba::ToNative({1280, 720}, native);  // out of range -> clamped to last pixel
    CHECK(p.x == 1919 && p.y == 1079);
}

void TestCoordsRoundTrip() {
    const Size native{1920, 1080};
    const Point n = maba::ToNormalized({960, 540}, native);
    CHECK(n.x == 640 && n.y == 360);
}

void TestCoordsInvalidSize() {
    const Point p = maba::ToNative({10, 10}, {0, 0});
    CHECK(p.x == 0 && p.y == 0);
}

void TestAspect() {
    CHECK(maba::IsSupportedAspect({1920, 1080}));
    CHECK(maba::IsSupportedAspect({1280, 720}));
    CHECK(maba::IsSupportedAspect({2560, 1440}));
    CHECK(!maba::IsSupportedAspect({1280, 800}));
    CHECK(!maba::IsSupportedAspect({0, 720}));
}

void TestOrderKeepsInputOrder() {
    const std::vector<TaskDef> tasks = {
        {"cafe", {}}, {"schedule", {}}, {"shop", {}},
        {"spend_ap", {}}, {"mail", {"spend_ap"}}, {"mission", {"mail"}}};
    const auto r = maba::ResolveOrder(tasks);
    CHECK(r.ok);
    const std::vector<std::string> expected = {"cafe", "schedule", "shop",
                                               "spend_ap", "mail", "mission"};
    CHECK(r.order == expected);
}

void TestOrderReordersForDependency() {
    const std::vector<TaskDef> tasks = {{"mail", {"spend_ap"}}, {"spend_ap", {}}};
    const auto r = maba::ResolveOrder(tasks);
    CHECK(r.ok);
    const std::vector<std::string> expected = {"spend_ap", "mail"};
    CHECK(r.order == expected);
}

void TestOrderErrors() {
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

int main() {
    TestCoordsIdentity();
    TestCoordsScaleUp();
    TestCoordsRoundTrip();
    TestCoordsInvalidSize();
    TestAspect();
    TestOrderKeepsInputOrder();
    TestOrderReordersForDependency();
    TestOrderErrors();
    TestFakesCompileAndWork();

    if (g_failures == 0) {
        std::printf("all tests passed\n");
        return 0;
    }
    std::printf("%d check(s) failed\n", g_failures);
    return 1;
}
