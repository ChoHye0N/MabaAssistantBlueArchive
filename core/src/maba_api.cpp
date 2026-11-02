// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#include "maba/maba_api.h"

#include <atomic>
#include <ctime>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include "maba/adb.h"
#include "maba/assistant.h"
#include "maba/ocr_sidecar.h"
#include "maba/process.h"
#include "maba/schedule.h"

#if defined(MABA_WITH_OPENCV)
#include "maba/adb_controller.h"
#include "maba/vision.h"
#endif
#if defined(_WIN32) && defined(MABA_WITH_OPENCV) && defined(MABA_HAS_WINDOWS_PLATFORM)
#include "steam_controller.h"
#endif

#ifndef MABA_VERSION_STRING
#define MABA_VERSION_STRING "0.0.0"
#endif

using nlohmann::json;
namespace fs = std::filesystem;

struct MabaInstance {
    MabaEventCallback callback = nullptr;
    void* user_data = nullptr;
    std::mutex callback_mutex;

    std::unique_ptr<maba::Assistant> assistant;
    bool loaded = false;
    bool ocr_enabled = true;
    std::vector<std::string> ocr_command;
    maba::SystemProcessRunner runner;
    maba::SystemProcessLauncher launcher;

    std::thread worker;
    std::atomic<bool> running{false};
    std::atomic<bool> cancel{false};

    std::string last_error;
    std::string result;  // storage for returned strings

    void Emit(const json& event) {
        std::lock_guard<std::mutex> lock(callback_mutex);
        if (!callback) return;
        const std::string text = event.dump(-1, ' ', false, json::error_handler_t::replace);
        callback(text.c_str(), user_data);
    }
    void LogEvent(maba::LogLevel level, const std::string& message) {
        Emit({{"type", "log"}, {"level", maba::ToString(level)}, {"message", message}});
    }
    void JoinWorker() {
        if (worker.joinable()) worker.join();
    }
};

namespace {

thread_local std::string g_thread_result;

fs::path Utf8Path(const std::string& s) { return fs::u8path(s); }

#if defined(MABA_WITH_OPENCV)
std::string Timestamp() {
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tm);
    return buf;
}
#endif

int Fail(MabaInstance* h, std::string message) {
    h->last_error = std::move(message);
    return 0;
}

json Parse(const char* text, std::string& error) {
    if (!text) return json::object();
    try {
        json j = json::parse(text);
        if (!j.is_object()) error = "argument must be a JSON object";
        return j;
    } catch (const json::exception& e) {
        error = std::string("invalid JSON: ") + e.what();
        return json();
    }
}

}  // namespace

extern "C" {

MABA_API const char* MABA_CALL MabaGetVersion(void) { return MABA_VERSION_STRING; }

MABA_API int MABA_CALL MabaApiVersion(void) { return 2; }

MABA_API MabaHandle MABA_CALL MabaCreate(const char* config_json, MabaEventCallback callback,
                                         void* user_data) {
    auto* h = new (std::nothrow) MabaInstance();
    if (!h) return nullptr;
    h->callback = callback;
    h->user_data = user_data;

    std::string error;
    const json cfg = Parse(config_json, error);
    if (!error.empty()) {
        h->last_error = "config: " + error;
        return h;
    }
    maba::AssistantConfig config;
    try {
        config.root = Utf8Path(cfg.value("root", std::string(".")));
        config.language = cfg.value("language", std::string("ko"));
        if (auto it = cfg.find("ocr"); it != cfg.end() && it->is_object()) {
            h->ocr_enabled = it->value("enabled", true);
            if (auto c = it->find("command"); c != it->end() && c->is_array())
                h->ocr_command = c->get<std::vector<std::string>>();
        }
    } catch (const json::exception& e) {
        h->last_error = std::string("config: ") + e.what();
        return h;
    }
    if (h->ocr_command.empty()) h->ocr_command = maba::DefaultOcrCommand(config.root);

    h->assistant = std::make_unique<maba::Assistant>(
        config, [h](maba::LogLevel level, const std::string& msg) { h->LogEvent(level, msg); });
    std::string load_error;
    h->loaded = h->assistant->Load(load_error);
    if (!h->loaded) h->last_error = load_error;
    return h;
}

MABA_API void MABA_CALL MabaDestroy(MabaHandle h) {
    if (!h) return;
    h->cancel = true;
    h->JoinWorker();
    {
        std::lock_guard<std::mutex> lock(h->callback_mutex);
        h->callback = nullptr;
    }
    delete h;
}

MABA_API const char* MABA_CALL MabaLastError(MabaHandle h) {
    if (!h) return "invalid handle";
    h->result = h->last_error;
    return h->result.c_str();
}

MABA_API const char* MABA_CALL MabaGetTasks(MabaHandle h) {
    if (!h) return "[]";
    json list = json::array();
    if (h->assistant && h->loaded) {
        for (const auto& t : h->assistant->Tasks()) {
            list.push_back({{"id", t.id},
                            {"name", t.name},
                            {"after", t.after},
                            {"status", t.status},
                            {"implemented", !t.steps.empty()},
                            {"needs_ocr", t.needs_ocr}});
        }
    }
    h->result = list.dump();
    return h->result.c_str();
}

MABA_API int MABA_CALL MabaConnect(MabaHandle h, const char* json_text) {
    if (!h) return 0;
    if (h->running) return Fail(h, "cannot connect while running");
    if (!h->loaded) return Fail(h, h->last_error.empty() ? "data not loaded" : h->last_error);
    std::string error;
    const json args = Parse(json_text, error);
    if (!error.empty()) return Fail(h, error);
    const std::string type = args.value("type", std::string());
    auto log = [h](maba::LogLevel level, const std::string& msg) { h->LogEvent(level, msg); };

    try {
        if (type == "adb") {
#if defined(MABA_WITH_OPENCV)
            maba::AdbControllerOptions o;
            o.adb_path = args.value("adb_path", o.adb_path);
            o.serial = args.value("serial", o.serial);
            const std::string sc = args.value("screencap", std::string("auto"));
            o.screencap = sc == "raw" ? maba::AdbScreencap::kRaw
                          : sc == "encode" ? maba::AdbScreencap::kEncode
                                           : maba::AdbScreencap::kAuto;
            o.touch = args.value("touch", std::string("adb")) == "maatouch" ? maba::AdbTouch::kMaaTouch
                                                                            : maba::AdbTouch::kAdbInput;
            o.maatouch_path = args.value("maatouch_path", std::string());
            o.back_keycode = args.value("back_keycode", o.back_keycode);
            auto c = std::make_unique<maba::AdbController>(h->runner, &h->launcher, o, log);
            if (!c->Connect()) return Fail(h, c->LastError());
            h->assistant->SetController(std::move(c));
#else
            return Fail(h, "this build has no emulator controller (built without OpenCV)");
#endif
        } else if (type == "steam") {
#if defined(_WIN32) && defined(MABA_WITH_OPENCV) && defined(MABA_HAS_WINDOWS_PLATFORM)
            maba::win::SteamControllerOptions o;
            o.capture = args.value("capture", std::string("bitblt")) == "printwindow"
                            ? maba::win::CaptureMethod::PrintWindow
                            : maba::win::CaptureMethod::BitBlt;
            auto c = std::make_unique<maba::win::SteamController>(o);
            if (!c->Connect()) return Fail(h, c->LastError());
            h->assistant->SetController(std::move(c));
#else
            return Fail(h, "the Steam controller is only available in the Windows build");
#endif
        } else {
            return Fail(h, "unknown controller type '" + type + "' (steam or adb)");
        }
    } catch (const json::exception& e) {
        return Fail(h, std::string("connect: ") + e.what());
    }

    if (h->ocr_enabled) {
        h->assistant->SetOcr(std::make_unique<maba::SidecarOcr>(
            h->launcher, maba::SidecarOptions{h->ocr_command}, h->assistant->Languages(), log));
    }
    h->last_error.clear();
    h->LogEvent(maba::LogLevel::kInfo, "connected (" + type + ")");
    return 1;
}

MABA_API int MABA_CALL MabaStart(MabaHandle h, const char* json_text) {
    if (!h) return 0;
    if (h->running) return Fail(h, "already running");
    if (!h->loaded) return Fail(h, h->last_error.empty() ? "data not loaded" : h->last_error);
    if (!h->assistant->Controller()) return Fail(h, "not connected");
    std::string error;
    const json args = Parse(json_text, error);
    if (!error.empty()) return Fail(h, error);
    std::vector<std::string> tasks;
    try {
        tasks = args.value("tasks", std::vector<std::string>{});
    } catch (const json::exception& e) {
        return Fail(h, std::string("tasks: ") + e.what());
    }
    if (tasks.empty()) return Fail(h, "no tasks selected");

    h->JoinWorker();  // a finished previous job
    h->cancel = false;
    h->running = true;
    h->last_error.clear();
    h->worker = std::thread([h, tasks] {
        h->Emit({{"type", "run"}, {"status", "started"}, {"tasks", tasks}});
        auto on_task = [h](const maba::TaskRunResult& r) {
            json values = json::object();
            for (const auto& [k, v] : r.values) values[k] = v;
            h->Emit({{"type", "task"},
                     {"id", r.id},
                     {"status", maba::ToString(r.status)},
                     {"message", r.message},
                     {"values", values}});
        };
        auto on_failure = [h](const maba::Frame& frame, const std::string& reason) {
#if defined(MABA_WITH_OPENCV)
            if (frame.empty()) return;
            // Failure frames stay under captures/ (gitignored): they may show personal data.
            const fs::path out =
                h->assistant->Config().root / "captures" / ("fail_" + Timestamp() + ".png");
            if (maba::SavePng(frame, out))
                h->Emit({{"type", "failure_frame"}, {"path", out.u8string()}, {"reason", reason}});
#else
            (void)frame;
            (void)reason;
#endif
        };
        maba::RunReport report;
        try {
            report = h->assistant->Run(tasks, &h->cancel, on_task, on_failure);
        } catch (const std::exception& e) {
            report.error = std::string("internal error: ") + e.what();
        }
        h->running = false;  // before the event, so a handler sees the job as finished
        h->Emit({{"type", "run"},
                 {"status", "finished"},
                 {"ok", report.ok},
                 {"cancelled", report.cancelled},
                 {"error", report.error}});
    });
    return 1;
}

MABA_API int MABA_CALL MabaStop(MabaHandle h) {
    if (!h || !h->running) return 0;
    h->cancel = true;
    return 1;
}

MABA_API int MABA_CALL MabaIsRunning(MabaHandle h) { return h && h->running ? 1 : 0; }

MABA_API const char* MABA_CALL MabaListAdbDevices(MabaHandle h, const char* adb_path) {
    if (!h) return "[]";
    const std::string adb = adb_path && *adb_path ? adb_path : "adb";
    const maba::ProcessResult r =
        h->runner.Run(maba::adb::DevicesCommand(adb), std::chrono::milliseconds{10000});
    json list = json::array();
    if (r.ok()) {
        for (const auto& d : maba::adb::ParseDevices(r.out))
            list.push_back({{"serial", d.serial}, {"state", d.state}});
        h->last_error.clear();
    } else {
        h->last_error = "adb devices failed: " + (r.error.empty() ? r.err : r.error);
    }
    h->result = list.dump();
    return h->result.c_str();
}

MABA_API const char* MABA_CALL MabaScheduleNext(const char* json_text) {
    json out;
    try {
        const json in = json::parse(json_text ? json_text : "{}");
        maba::DailySchedule s;
        s.reset_hour_utc = in.value("reset_hour_utc", s.reset_hour_utc);
        s.reset_minute_utc = in.value("reset_minute_utc", s.reset_minute_utc);
        s.delay_minutes = in.value("delay_minutes", s.delay_minutes);
        if (!maba::IsValid(s)) throw std::runtime_error("schedule values out of range");
        const std::int64_t now = in.at("now").get<std::int64_t>();
        const std::int64_t last = in.value("last_run", std::int64_t{0});
        out = {{"ok", true},
               {"due", maba::IsDue(now, last, s)},
               {"next", maba::NextRun(now, last, s)},
               {"slot", maba::CurrentSlot(now, s)}};
    } catch (const std::exception& e) {
        out = {{"ok", false}, {"error", e.what()}};
    }
    g_thread_result = out.dump();
    return g_thread_result.c_str();
}

}  // extern "C"
