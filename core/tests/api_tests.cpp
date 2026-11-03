// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// C ABI smoke tests through the shared library, as the GUI uses it.
#include <cstring>
#include <chrono>
#include <string>
#include <thread>

#include <nlohmann/json.hpp>

#include "maba/maba_api.h"
#include "test_framework.h"

namespace {

using nlohmann::json;

MABA_TEST(ApiVersion) {
    CHECK(MabaApiVersion() == 5);
    CHECK(std::strlen(MabaGetVersion()) > 0);
}

MABA_TEST(ApiScheduleNext) {
    const json r = json::parse(MabaScheduleNext(
        R"({"now": 1793646301, "last_run": 0, "reset_hour_utc": 19, "reset_minute_utc": 0, "delay_minutes": 5})"));
    CHECK(r["ok"] == true && r["due"] == true && r["next"] == 1793646301);
    const json bad = json::parse(MabaScheduleNext(R"({"now": 1, "reset_hour_utc": 30})"));
    CHECK(bad["ok"] == false);
    const json missing = json::parse(MabaScheduleNext("{}"));
    CHECK(missing["ok"] == false);
    // 07:30 KST = 22:30 UTC; now = 21:30 UTC, an hour before the slot.
    const json clock = json::parse(MabaScheduleNext(
        R"({"now": 1793655000, "last_run": 1793572300, "mode": "clock", "hour": 7, "minute": 30, "utc_offset_minutes": 540})"));
    CHECK(clock["ok"] == true && clock["due"] == false && clock["next"] == 1793658600);
    CHECK(clock["minutes_before_reset"] == 1230);
    const json badClock = json::parse(MabaScheduleNext(R"({"now": 1, "mode": "clock", "hour": 25, "minute": 0})"));
    CHECK(badClock["ok"] == false);
    const json badMode = json::parse(MabaScheduleNext(R"({"now": 1, "mode": "weekly"})"));
    CHECK(badMode["ok"] == false);
}

int g_events = 0;
void MABA_CALL OnEvent(const char*, void* user) { ++*static_cast<int*>(user); }

MABA_TEST(ApiCreateLoadsRepoData) {
    const std::string cfg = json{{"root", MABA_SOURCE_DIR}, {"language", "ko"}}.dump();
    MabaHandle h = MabaCreate(cfg.c_str(), &OnEvent, &g_events);
    CHECK(h != nullptr);
    CHECK(std::string(MabaLastError(h)).empty());
    const json tasks = json::parse(MabaGetTasks(h));
    CHECK(tasks.is_array() && tasks.size() >= 6);
    CHECK(g_events > 0);  // load log
    CHECK(MabaStart(h, R"({"tasks": ["cafe"]})") == 0);  // not connected
    CHECK(std::string(MabaLastError(h)) == "not connected");
    CHECK(MabaConnect(h, R"({"type": "teleport"})") == 0);
    CHECK(MabaIsRunning(h) == 0);
    CHECK(MabaStop(h) == 0);
    MabaDestroy(h);
}

MABA_TEST(ApiCreateReportsBadRoot) {
    MabaHandle h = MabaCreate(R"({"root": "/maba/no/such/dir"})", nullptr, nullptr);
    CHECK(h != nullptr);
    CHECK(!std::string(MabaLastError(h)).empty());
    CHECK(json::parse(MabaGetTasks(h)).empty());
    MabaDestroy(h);
    MabaHandle bad = MabaCreate("not json", nullptr, nullptr);
    CHECK(std::string(MabaLastError(bad)).rfind("config", 0) == 0);
    MabaDestroy(bad);
}

MABA_TEST(ApiStartValidatesLaunch) {
    const std::string cfg = json{{"root", MABA_SOURCE_DIR}, {"language", "ko"}, {"ocr", {{"enabled", false}}}}.dump();
    MabaHandle h = MabaCreate(cfg.c_str(), nullptr, nullptr);
    CHECK(std::string(MabaLastError(h)).empty());
    // Launching needs "connect" with a known type; the Steam client has no emulator to close.
    CHECK(MabaStart(h, R"({"tasks": ["cafe"], "connect": {"type": "other"}, "launch": {}})") == 0);
    CHECK(std::string(MabaLastError(h)).find("adb or steam") != std::string::npos);
    CHECK(MabaStart(h, R"({"tasks": ["cafe"], "launch": {}})") == 0);
    CHECK(MabaStart(h, R"({"tasks": ["cafe"], "connect": {"type": "steam"}, "finish": {"close_emulator": true}})") == 0);
    CHECK(std::string(MabaLastError(h)).find("close_emulator") != std::string::npos);
    CHECK(MabaStart(h, R"({"tasks": ["cafe"], "connect": {"type": "steam"}, "launch": {"game_timeout_s": 5}})") == 0);
    CHECK(std::string(MabaLastError(h)).find("timeouts") != std::string::npos);
    CHECK(MabaStart(h, R"({"tasks": ["cafe"], "connect": {"type": "steam"}, "launch": {"steam_url": "https://x"}})") == 0);
    CHECK(std::string(MabaLastError(h)).find("steam://") != std::string::npos);
    CHECK(MabaStart(h, R"({"tasks": ["cafe"], "connect": {"type": "adb"}, "launch": {"boot_timeout_s": 1}})") == 0);
    CHECK(std::string(MabaLastError(h)).find("timeouts") != std::string::npos);
    CHECK(MabaStart(h, R"({"tasks": ["cafe"], "connect": {"type": "adb"}, "launch": {"emulator_args": 5}})") == 0);
    CHECK(MabaStart(h, R"({"tasks": ["cafe"], "connect": [1]})") == 0);

    // A missing device without an emulator path ends the run with an error, on the worker.
    const std::string start = json{{"tasks", {"cafe"}},
                                   {"connect", {{"type", "adb"}, {"adb_path", "maba-no-such-adb"}, {"serial", "emulator-5554"}}},
                                   {"launch", {{"start_game", true}}},
                                   {"finish", {{"close_game", true}, {"close_emulator", true}}}}
                                  .dump();
    CHECK(MabaStart(h, start.c_str()) == 1);
    for (int i = 0; i < 500 && MabaIsRunning(h); ++i) std::this_thread::sleep_for(std::chrono::milliseconds{20});
    CHECK(MabaIsRunning(h) == 0);
    MabaDestroy(h);
}

}  // namespace
