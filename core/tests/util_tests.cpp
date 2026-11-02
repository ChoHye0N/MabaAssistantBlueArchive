// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// OCR text parsing, schedule arithmetic, languages, process quoting and the real process runner.
#include <chrono>
#include <string>

#include "fakes.h"
#include "maba/languages.h"
#include "maba/ocr_text.h"
#include "maba/process.h"
#include "maba/schedule.h"
#include "test_framework.h"

namespace maba_test {
extern std::string g_self_path;
}

namespace {

using namespace std::chrono_literals;

MABA_TEST(ParseIntegerAcceptsGroupedDigits) {
    CHECK(maba::ParseInteger("123") == 123);
    CHECK(maba::ParseInteger(" 1,234 ") == 1234);
    CHECK(maba::ParseInteger("1.234.567") == 1234567);
    CHECK(maba::ParseInteger("1 234") == 1234);
    CHECK(maba::ParseInteger("0") == 0);
}

MABA_TEST(ParseIntegerRejectsAmbiguousText) {
    CHECK(!maba::ParseInteger(""));
    CHECK(!maba::ParseInteger("12,34"));
    CHECK(!maba::ParseInteger("1,234.567"));
    CHECK(!maba::ParseInteger("12a"));
    CHECK(!maba::ParseInteger("-5"));
    CHECK(!maba::ParseInteger(",123"));
    CHECK(!maba::ParseInteger("123,"));
    CHECK(!maba::ParseInteger("1234567890123"));
}

MABA_TEST(ParseFractionReadsValueAndMax) {
    const auto f = maba::ParseFraction("120 / 240");
    CHECK(f && f->value == 120 && f->max == 240);
    CHECK(!maba::ParseFraction("120"));
    CHECK(!maba::ParseFraction("1/2/3"));
    CHECK(!maba::ParseFraction("a/2"));
}

MABA_TEST(JoinLeftToRightSortsByX) {
    CHECK(maba::JoinLeftToRight({{"40", {50, 0, 10, 10}, 1}, {"1,2", {0, 0, 10, 10}, 1}}) == "1,240");
}

// 2026-11-02 19:00:00 UTC = 1793646000
constexpr std::int64_t kReset = 1793646000;

MABA_TEST(ScheduleSlotsFollowServerReset) {
    maba::DailySchedule s;  // 19:00 UTC + 5 min
    CHECK(maba::IsValid(s));
    CHECK(maba::LastReset(kReset, s) == kReset);
    CHECK(maba::LastReset(kReset - 1, s) == kReset - 86400);
    CHECK(maba::CurrentSlot(kReset + 600, s) == kReset + 300);
    // Before the delay has passed: not due, next run at the slot.
    CHECK(!maba::IsDue(kReset + 60, 0, s));
    CHECK(maba::NextRun(kReset + 60, kReset - 3600, s) == kReset + 300);
    // After the slot, never run: due now.
    CHECK(maba::IsDue(kReset + 301, 0, s));
    CHECK(maba::NextRun(kReset + 301, 0, s) == kReset + 301);
    // Already ran today: tomorrow's slot.
    CHECK(!maba::IsDue(kReset + 4000, kReset + 400, s));
    CHECK(maba::NextRun(kReset + 4000, kReset + 400, s) == kReset + 86400 + 300);
    maba::DailySchedule bad;
    bad.reset_hour_utc = 24;
    CHECK(!maba::IsValid(bad));
}

MABA_TEST(LanguagesLoadFromRepo) {
    const auto r = maba::LoadLanguages(std::string(MABA_SOURCE_DIR) + "/config/languages.json");
    CHECK(r.ok);
    CHECK(r.config.primary == "ko");
    const maba::Language* ko = r.config.Find("ko");
    CHECK(ko && ko->ocr_lang == "korean");
    CHECK(!maba::ParseLanguages(R"({"primary": "xx", "languages": [{"code": "ko", "ocr_lang": "korean"}]})").ok);
}

MABA_TEST(QuoteWindowsArgFollowsArgvRules) {
    CHECK(maba::QuoteWindowsArg("plain") == "plain");
    CHECK(maba::QuoteWindowsArg("") == "\"\"");
    CHECK(maba::QuoteWindowsArg("a b") == "\"a b\"");
    CHECK(maba::QuoteWindowsArg("say \"hi\"") == "\"say \\\"hi\\\"\"");
    CHECK(maba::QuoteWindowsArg("C:\\dir with space\\") == "\"C:\\dir with space\\\\\"");
    CHECK(maba::QuoteWindowsArg("a\\\\b") == "a\\\\b");
}

std::vector<std::string> EchoCommand(const std::string& text) {
#ifdef _WIN32
    return {"cmd.exe", "/c", "echo " + text};
#else
    return {"/bin/sh", "-c", "echo " + text};
#endif
}

MABA_TEST(SystemRunnerCapturesOutputAndExitCode) {
    maba::SystemProcessRunner runner;
    const auto r = runner.Run(EchoCommand("hello"), 10s);
    CHECK(r.ok());
    CHECK(r.out.find("hello") != std::string::npos);
#ifdef _WIN32
    const auto fail = runner.Run({"cmd.exe", "/c", "exit 3"}, 10s);
#else
    const auto fail = runner.Run({"/bin/sh", "-c", "exit 3"}, 10s);
#endif
    CHECK(fail.started && fail.exit_code == 3);
    const auto missing = runner.Run({"maba-no-such-program-xyz"}, 5s);
    CHECK(!missing.started);
    CHECK(!missing.error.empty());
}

MABA_TEST(SystemRunnerKillsOnTimeout) {
    maba::SystemProcessRunner runner;
#ifdef _WIN32
    const auto r = runner.Run({"cmd.exe", "/c", "ping -n 30 127.0.0.1 >NUL"}, 300ms);
#else
    const auto r = runner.Run({"/bin/sh", "-c", "sleep 30"}, 300ms);
#endif
    CHECK(r.started);
    CHECK(r.timed_out);
}

MABA_TEST(SystemLauncherTalksLines) {
    maba::SystemProcessLauncher launcher;
    std::string error;
    auto p = launcher.Start({maba_test::g_self_path, "--echo-peer"}, error);
    CHECK(p != nullptr);
    if (!p) return;
    CHECK(p->WriteLine("ping 1"));
    const auto line = p->ReadLine(5s);
    CHECK(line && *line == "ping 1");
    CHECK(p->WriteLine("한글 2"));
    const auto line2 = p->ReadLine(5s);
    CHECK(line2 && *line2 == "한글 2");
    p->Terminate();
    CHECK(!p->Running());
}

}  // namespace
