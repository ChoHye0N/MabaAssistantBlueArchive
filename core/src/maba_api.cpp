// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#include "maba/maba_api.h"

#include <atomic>
#include <ctime>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include "maba/adb.h"
#include "maba/assistant.h"
#include "maba/emulator.h"
#include "maba/ocr_sidecar.h"
#include "maba/process.h"
#include "maba/schedule.h"
#include "maba/steam_launch.h"
#include "maba/utf8.h"

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
    maba::SystemDetachedProcesses processes;

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

fs::path Utf8Path(const std::string& s) { return maba::PathFromUtf8(s); }

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

// Builds and connects the controller described by `args` (MabaConnect format), sets it and
// the OCR on the assistant. Runs on the caller's thread (the worker for MabaStart).
bool ConnectController(MabaInstance* h, const json& args, std::string& error) {
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
            if (!c->Connect()) {
                error = c->LastError();
                return false;
            }
            h->assistant->SetController(std::move(c));
#else
            error = "this build has no emulator controller (built without OpenCV)";
            return false;
#endif
        } else if (type == "steam") {
#if defined(_WIN32) && defined(MABA_WITH_OPENCV) && defined(MABA_HAS_WINDOWS_PLATFORM)
            maba::win::SteamControllerOptions o;
            o.capture = args.value("capture", std::string("bitblt")) == "printwindow"
                            ? maba::win::CaptureMethod::PrintWindow
                            : maba::win::CaptureMethod::BitBlt;
            auto c = std::make_unique<maba::win::SteamController>(o);
            if (!c->Connect()) {
                error = c->LastError();
                return false;
            }
            h->assistant->SetController(std::move(c));
#else
            error = "the Steam controller is only available in the Windows build";
            return false;
#endif
        } else {
            error = "unknown controller type '" + type + "' (steam or adb)";
            return false;
        }
    } catch (const json::exception& e) {
        error = std::string("connect: ") + e.what();
        return false;
    }
    if (h->ocr_enabled) {
        h->assistant->SetOcr(std::make_unique<maba::SidecarOcr>(
            h->launcher, maba::SidecarOptions{h->ocr_command}, h->assistant->Languages(), log));
    }
    h->LogEvent(maba::LogLevel::kInfo, "connected (" + type + ")");
    return true;
}

// MabaStart "launch" (emulator only) -> session options, with adb path and serial from "connect".
bool ParseLaunch(const json& connect, const json& launch, maba::EmulatorOptions& o, std::string& error) {
    try {
        if (connect.value("type", std::string()) != "adb") {
            error = "launch: only the emulator (adb) can be started for now";
            return false;
        }
        o.adb_path = connect.value("adb_path", o.adb_path);
        o.serial = connect.value("serial", std::string("127.0.0.1:16384"));
        o.emulator_path = launch.value("emulator_path", std::string());
        if (auto a = launch.find("emulator_args"); a != launch.end()) {
            if (a->is_string()) o.emulator_args = maba::emu::SplitCommandLine(a->get<std::string>());
            else if (a->is_array()) o.emulator_args = a->get<std::vector<std::string>>();
            else if (!a->is_null()) throw std::runtime_error("emulator_args must be a string or an array");
        }
        o.game_activity = launch.value("game_activity", o.game_activity);
        const int boot_s = launch.value("boot_timeout_s", o.boot_timeout_ms / 1000);
        const int game_s = launch.value("game_timeout_s", o.game_timeout_ms / 1000);
        if (boot_s < 10 || boot_s > 1800 || game_s < 10 || game_s > 1800)
            throw std::runtime_error("timeouts must be between 10 and 1800 seconds");
        o.boot_timeout_ms = boot_s * 1000;
        o.game_timeout_ms = game_s * 1000;
    } catch (const std::exception& e) {
        error = std::string("launch: ") + e.what();
        return false;
    }
    return true;
}

// MabaStart "launch" for the Steam client -> session options.
bool ParseSteamLaunch(const json& launch, maba::SteamLaunchOptions& o, std::string& error) {
    try {
        o.url = launch.value("steam_url", o.url);
        if (o.url.rfind("steam://", 0) != 0) throw std::runtime_error("steam_url must start with steam://");
        const int game_s = launch.value("game_timeout_s", o.window_timeout_ms / 1000);
        if (game_s < 10 || game_s > 1800) throw std::runtime_error("timeouts must be between 10 and 1800 seconds");
        o.window_timeout_ms = game_s * 1000;
    } catch (const std::exception& e) {
        error = std::string("launch: ") + e.what();
        return false;
    }
    return true;
}

}  // namespace

extern "C" {

MABA_API const char* MABA_CALL MabaGetVersion(void) { return MABA_VERSION_STRING; }

MABA_API int MABA_CALL MabaApiVersion(void) { return 5; }

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
        config.data_dir = Utf8Path(cfg.value("data_dir", std::string()));
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
            json settings = json::array();
            for (const auto& d : t.settings) {
                static const char* const types[] = {"bool", "int", "choice", "list"};
                json def = {{"key", d.key},
                            {"type", types[static_cast<int>(d.type)]},
                            {"choices", d.choices},
                            {"description", d.description},
                            {"ui", d.ui}};
                if (!d.label_key.empty()) {
                    json labels = json::object();
                    for (const auto& c : d.choices) {
                        auto s = h->assistant->Strings().find(d.label_key + c);
                        labels[c] = s != h->assistant->Strings().end() ? s->second : c;
                    }
                    def["labels"] = std::move(labels);
                }
                switch (d.type) {
                    case maba::SettingType::kBool: def["default"] = d.default_value.scalar == "true"; break;
                    case maba::SettingType::kInt:
                        def["default"] = std::stoll(d.default_value.scalar);
                        def["min"] = d.min;
                        def["max"] = d.max;
                        break;
                    case maba::SettingType::kChoice: def["default"] = d.default_value.scalar; break;
                    case maba::SettingType::kList: def["default"] = d.default_value.list; break;
                }
                settings.push_back(std::move(def));
            }
            list.push_back({{"id", t.id},
                            {"name", t.name},
                            {"order", t.order},
                            {"after", t.after},
                            {"status", t.status},
                            {"implemented", !t.steps.empty()},
                            {"needs_ocr", t.needs_ocr},
                            {"settings", std::move(settings)}});
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
    if (!ConnectController(h, args, error)) return Fail(h, error);
    h->last_error.clear();
    return 1;
}

MABA_API int MABA_CALL MabaStart(MabaHandle h, const char* json_text) {
    if (!h) return 0;
    if (h->running) return Fail(h, "already running");
    if (!h->loaded) return Fail(h, h->last_error.empty() ? "data not loaded" : h->last_error);
    std::string error;
    const json args = Parse(json_text, error);
    if (!error.empty()) return Fail(h, error);
    // Optional preparation on the worker: start the emulator and the game, then connect.
    json connect, launch, finish;
    for (auto [key, out] : {std::pair<const char*, json*>{"connect", &connect}, {"launch", &launch}, {"finish", &finish}}) {
        if (auto it = args.find(key); it != args.end() && !it->is_null()) {
            if (!it->is_object()) return Fail(h, std::string(key) + " must be an object");
            *out = *it;
        }
    }
    if (connect.is_null() && !h->assistant->Controller()) return Fail(h, "not connected");
    const std::string target = connect.is_object() ? connect.value("type", std::string()) : std::string();
    const bool prepare_any = !launch.is_null() || !finish.is_null();
    if (prepare_any && target != "adb" && target != "steam")
        return Fail(h, "launch and finish need \"connect\" with type adb or steam");
    std::optional<maba::EmulatorOptions> emulator;
    std::optional<maba::SteamLaunchOptions> steam;
    bool start_game = false, close_game = false, close_emulator = false;
    if (prepare_any) {
        try {
            start_game = !launch.is_null() && launch.value("start_game", true);
            close_game = !finish.is_null() && finish.value("close_game", false);
            close_emulator = !finish.is_null() && finish.value("close_emulator", false);
        } catch (const json::exception& e) {
            return Fail(h, std::string("launch/finish: ") + e.what());
        }
        const json launch_args = launch.is_null() ? json::object() : launch;
        if (target == "adb") {
            maba::EmulatorOptions o;
            if (!ParseLaunch(connect, launch_args, o, error)) return Fail(h, error);
            emulator = o;
        } else {
            if (close_emulator) return Fail(h, "finish: close_emulator is for the emulator (adb) only");
            maba::SteamLaunchOptions o;
            if (!ParseSteamLaunch(launch_args, o, error)) return Fail(h, error);
            steam = o;
        }
    }
    std::vector<std::string> tasks;
    try {
        tasks = args.value("tasks", std::vector<std::string>{});
    } catch (const json::exception& e) {
        return Fail(h, std::string("tasks: ") + e.what());
    }
    if (tasks.empty()) return Fail(h, "no tasks selected");
    std::string settings_json;
    if (auto it = args.find("settings"); it != args.end() && !it->is_null()) {
        if (!it->is_object()) return Fail(h, "settings must be an object");
        settings_json = it->dump();
        std::map<std::string, maba::TaskSettings> resolved;
        if (!maba::ResolveSettings(h->assistant->Tasks(), settings_json, resolved, error)) return Fail(h, error);
    }

    h->JoinWorker();  // a finished previous job
    h->cancel = false;
    h->running = true;
    h->last_error.clear();
    const bool prepare = !launch.is_null();
    h->worker = std::thread([h, tasks, settings_json, connect, emulator, steam, prepare, start_game, close_game,
                             close_emulator] {
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
                h->Emit({{"type", "failure_frame"}, {"path", maba::PathToUtf8(out)}, {"reason", reason}});
#else
            (void)frame;
            (void)reason;
#endif
        };
        maba::RunReport report;
        auto log = [h](maba::LogLevel level, const std::string& msg) { h->LogEvent(level, msg); };
        std::optional<maba::EmulatorSession> session;
        if (emulator) session.emplace(h->runner, h->processes, *emulator, log);
        maba::SystemGameWindow window;
        std::optional<maba::SteamSession> steam_session;
        if (steam) steam_session.emplace(window, h->processes, *steam, log);
        try {
            bool ready = true;
            if (session && prepare) {
                ready = session->EnsureDevice(&h->cancel) && (!start_game || session->EnsureGame(&h->cancel));
                if (!ready) report.error = session->LastError();
            }
            if (steam_session && prepare && start_game) {
                ready = steam_session->EnsureGame(&h->cancel);
                if (!ready) report.error = steam_session->LastError();
            }
            if (ready && !connect.is_null() && !h->cancel) {
                std::string error;
                ready = ConnectController(h, connect, error);
                if (!ready) report.error = error;
            }
            if (ready && h->cancel) {
                report.cancelled = true;
            } else if (ready) {
                report = h->assistant->Run(tasks, &h->cancel, on_task, on_failure, settings_json);
            }
            report.cancelled = report.cancelled || h->cancel.load();
            // A run stopped by the user leaves the game and the emulator as they are.
            if (session && !report.cancelled) {
                if (close_game) session->CloseGame();
                if (close_emulator) session->CloseEmulator();
            }
            if (steam_session && !report.cancelled && close_game) steam_session->CloseGame();
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
        maba::DailySchedule server;
        server.reset_hour_utc = in.value("reset_hour_utc", server.reset_hour_utc);
        server.reset_minute_utc = in.value("reset_minute_utc", server.reset_minute_utc);
        server.delay_minutes = in.value("delay_minutes", server.delay_minutes);
        if (!maba::IsValid(server)) throw std::runtime_error("schedule values out of range");
        const std::string mode = in.value("mode", std::string("reset"));
        maba::DailySchedule s = server;
        if (mode == "clock") {
            if (!maba::ClockSchedule(in.at("hour").get<int>(), in.at("minute").get<int>(),
                                     in.value("utc_offset_minutes", 0), s))
                throw std::runtime_error("clock time out of range");
        } else if (mode != "reset") {
            throw std::runtime_error("unknown schedule mode: " + mode);
        }
        const std::int64_t now = in.at("now").get<std::int64_t>();
        const std::int64_t last = in.value("last_run", std::int64_t{0});
        out = {{"ok", true},
               {"due", maba::IsDue(now, last, s)},
               {"next", maba::NextRun(now, last, s)},
               {"slot", maba::CurrentSlot(now, s)}};
        if (mode == "clock") out["minutes_before_reset"] = maba::MinutesBeforeReset(s, server);
    } catch (const std::exception& e) {
        out = {{"ok", false}, {"error", e.what()}};
    }
    g_thread_result = out.dump();
    return g_thread_result.c_str();
}

}  // extern "C"
