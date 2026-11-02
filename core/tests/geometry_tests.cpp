// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#include "maba/geometry.h"
#include "test_framework.h"

namespace {

using maba::Point;
using maba::Rect;

bool Eq(const Rect& a, const Rect& b) {
    return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}

MABA_TEST(IntersectBasics) {
    CHECK(Eq(maba::Intersect({0, 0, 10, 10}, {5, 5, 10, 10}), {5, 5, 5, 5}));
    CHECK(Eq(maba::Intersect({0, 0, 10, 10}, {10, 0, 5, 5}), {}));  // touching edges only
    CHECK(Eq(maba::Intersect({0, 0, 0, 10}, {0, 0, 5, 5}), {}));
}

MABA_TEST(ClientCropWithBorders) {
    // Window bitmap at (100,50) 1296x759; client origin at (108,81) 1280x720
    // (8 px side border, 31 px title bar): typical for a classic frame.
    const Rect crop = maba::ClientCropRect({100, 50, 1296, 759}, {108, 81}, {1280, 720});
    CHECK(Eq(crop, {8, 31, 1280, 720}));
}

MABA_TEST(ClientCropAsymmetricBorders) {
    // Invisible resize borders differ per side on newer Windows; the origin comes from
    // ClientToScreen, so no symmetric-border assumption is needed.
    const Rect crop = maba::ClientCropRect({-7, 0, 1934, 1127}, {0, 40}, {1920, 1080});
    CHECK(Eq(crop, {7, 40, 1920, 1080}));
}

MABA_TEST(ClientCropClampedAndInvalid) {
    const Rect crop = maba::ClientCropRect({0, 0, 100, 100}, {10, 10}, {200, 200});
    CHECK(Eq(crop, {10, 10, 90, 90}));
    CHECK(Eq(maba::ClientCropRect({0, 0, 0, 0}, {0, 0}, {10, 10}), {}));
    CHECK(Eq(maba::ClientCropRect({0, 0, 10, 10}, {0, 0}, {0, 10}), {}));
}

MABA_TEST(NormalizedRectToNativeScales) {
    CHECK(Eq(maba::NormalizedRectToNative({100, 50, 200, 100}, {1920, 1080}),
             {150, 75, 300, 150}));
    CHECK(Eq(maba::NormalizedRectToNative({100, 50, 200, 100}, {1280, 720}),
             {100, 50, 200, 100}));
    // 1600x900 (scale 1.25): edges round independently
    CHECK(Eq(maba::NormalizedRectToNative({1, 1, 3, 3}, {1600, 900}), {1, 1, 4, 4}));
}

MABA_TEST(NormalizedRectToNativeAdjacentNoGap) {
    const auto a = maba::NormalizedRectToNative({0, 0, 333, 10}, {1600, 900});
    const auto b = maba::NormalizedRectToNative({333, 0, 333, 10}, {1600, 900});
    CHECK(a.x + a.width == b.x);
}

MABA_TEST(NormalizedRectToNativeClampAndInvalid) {
    CHECK(Eq(maba::NormalizedRectToNative({1200, 700, 200, 100}, {1920, 1080}),
             {1800, 1050, 120, 30}));
    CHECK(Eq(maba::NormalizedRectToNative({1300, 0, 10, 10}, {1920, 1080}), {}));
    CHECK(Eq(maba::NormalizedRectToNative({0, 0, 0, 10}, {1920, 1080}), {}));
    CHECK(Eq(maba::NormalizedRectToNative({0, 0, 10, 10}, {0, 0}), {}));
}

MABA_TEST(NativeRectToNormalizedRoundTrip) {
    const Rect n{120, 80, 240, 64};
    const auto native = maba::NormalizedRectToNative(n, {2560, 1440});
    CHECK(Eq(maba::NativeRectToNormalized(native, {2560, 1440}), n));
}

MABA_TEST(AbsoluteInputSingleMonitor) {
    const Rect desk{0, 0, 1920, 1080};
    Point p = maba::ToAbsoluteInput({0, 0}, desk);
    CHECK(p.x == 0 && p.y == 0);
    p = maba::ToAbsoluteInput({1919, 1079}, desk);
    CHECK(p.x == 65535 && p.y == 65535);
    p = maba::ToAbsoluteInput({5000, -10}, desk);  // clamped
    CHECK(p.x == 65535 && p.y == 0);
}

MABA_TEST(AbsoluteInputNegativeOriginMonitor) {
    // Secondary monitor left of the primary: virtual desktop starts at x = -1920.
    const Rect desk{-1920, 0, 3840, 1080};
    const Point left = maba::ToAbsoluteInput({-1920, 0}, desk);
    CHECK(left.x == 0);
    const Point primary_origin = maba::ToAbsoluteInput({0, 0}, desk);
    CHECK(primary_origin.x == 32776);  // round(1920 * 65535 / 3839)
}

MABA_TEST(SwipePathEndpointsAndSteps) {
    const auto path = maba::SwipePath({100, 100}, {300, 100}, 200, 20);
    CHECK(path.size() == 11);
    CHECK(path.front().x == 100 && path.back().x == 300);
    bool monotonic = true;
    for (std::size_t i = 1; i < path.size(); ++i) monotonic = monotonic && path[i].x >= path[i - 1].x;
    CHECK(monotonic);
}

MABA_TEST(SwipePathCapsAndDegenerate) {
    CHECK(maba::SwipePath({0, 0}, {10, 10}, 100000, 1, 50).size() == 50);
    const auto zero = maba::SwipePath({5, 5}, {9, 9}, 0, 16);
    CHECK(zero.size() == 2 && zero.back().x == 9);
    CHECK(maba::SwipePath({0, 0}, {1, 1}, 100, 0).size() == 2);
}

}  // namespace
