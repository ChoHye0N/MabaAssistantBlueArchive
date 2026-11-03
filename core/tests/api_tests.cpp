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
    CHECK(MabaApiVersion() == 4);
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
    // Launching is for the emulator only (for now), and needs "connect".
    CHECK(MabaStart(h, R"({"tasks": ["cafe"], "connect": {"type": "steam"}, "launch": {}})") == 0);
    CHECK(std::string(MabaLastError(h)).find("adb") != std::string::npos);
    CHECK(MabaStart(h, R"({"tasks": ["cafe"], "launch": {}})") == 0);
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
