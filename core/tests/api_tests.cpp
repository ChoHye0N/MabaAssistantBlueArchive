// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// C ABI smoke tests through the shared library, as the GUI uses it.
#include <cstring>
#include <string>

#include <nlohmann/json.hpp>

#include "maba/maba_api.h"
#include "test_framework.h"

namespace {

using nlohmann::json;

MABA_TEST(ApiVersion) {
    CHECK(MabaApiVersion() == 2);
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

}  // namespace
