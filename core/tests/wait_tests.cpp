// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#include "fakes.h"
#include "maba/wait.h"
#include "test_framework.h"

namespace {

using namespace std::chrono_literals;

MABA_TEST(WaitSucceedsImmediately) {
    maba_test::ManualClock clock;
    const auto r = maba::WaitUntil([] { return true; }, {1000ms, 100ms, 50}, clock);
    CHECK(r.ok && r.attempts == 1 && clock.sleeps == 0);
}

MABA_TEST(WaitSucceedsAfterSomeAttempts) {
    maba_test::ManualClock clock;
    int calls = 0;
    const auto r = maba::WaitUntil([&] { return ++calls == 4; }, {1000ms, 100ms, 50}, clock);
    CHECK(r.ok && r.attempts == 4 && clock.sleeps == 3);
    CHECK(r.elapsed == 300ms);
}

MABA_TEST(WaitTimesOut) {
    maba_test::ManualClock clock;
    const auto r = maba::WaitUntil([] { return false; }, {1000ms, 100ms, 1000}, clock);
    CHECK(!r.ok);
    CHECK(r.elapsed <= 1000ms);
    CHECK(r.attempts == 11);  // t = 0, 100, ..., 1000
}

MABA_TEST(WaitAttemptCap) {
    maba_test::ManualClock clock;
    const auto r = maba::WaitUntil([] { return false; }, {1000000ms, 1ms, 7}, clock);
    CHECK(!r.ok && r.attempts == 7);
}

MABA_TEST(WaitSlowConditionCountsTowardTimeout) {
    maba_test::ManualClock clock;
    clock.advance_per_now = 400ms;  // each check of the clock costs time
    const auto r = maba::WaitUntil([] { return false; }, {1000ms, 100ms, 1000}, clock);
    CHECK(!r.ok && r.attempts < 5);
}

MABA_TEST(WaitZeroIntervalStillBounded) {
    maba_test::ManualClock clock;
    const auto r = maba::WaitUntil([] { return false; }, {50ms, 0ms, 1000}, clock);
    CHECK(!r.ok && r.attempts <= 51);
}

}  // namespace
