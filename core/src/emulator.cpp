// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Written from docs/notes/emulator-launch.md. The start/stop sequence (start the emulator when
// the device is missing, `am start` with a `monkey` fallback, focus check through
// `dumpsys window`, MuMu launcher-to-instance path rewriting, killing the started process tree)
// follows an MIT-licensed project; see docs/SOURCES.md.
#include "maba/emulator.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>
#include <system_error>
#include <thread>

#include <nlohmann/json.hpp>

#include "maba/adb.h"
#include "maba/utf8.h"

namespace maba {

namespace emu {

namespace {

std::string Trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    const auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

std::string Lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string Slashes(std::string s) {
    std::replace(s.begin(), s.end(), '\\', '/');
    return s;
}

// "Window{1a2b u0 com.x/.Main}" or "ActivityRecord{1a2b u0 com.x/.Main t12}" -> "com.x".
std::string PackageInRecord(const std::string& record) {
    std::string inner = record;
    if (const auto b = inner.find('{'); b != std::string::npos) inner = inner.substr(b + 1);
    if (const auto e = inner.find('}'); e != std::string::npos) inner = inner.substr(0, e);
    std::istringstream in(inner);
    std::vector<std::string> tokens;
    for (std::string t; in >> t;) tokens.push_back(t);
    for (const auto& t : tokens) {
        if (const auto slash = t.find('/'); slash != std::string::npos && slash > 0) return t.substr(0, slash);
    }
    // A window without an activity still names its package (one with dots).
    if (tokens.size() >= 3 && tokens.back().find('.') != std::string::npos) return tokens.back();
    return {};
}

std::string AfterKey(const std::string& line, const std::string& key) {
    const auto pos = line.find(key);
    return pos == std::string::npos ? std::string() : Trim(line.substr(pos + key.size()));
}

}  // namespace

std::string PackageOf(const std::string& activity) {
    const auto slash = activity.find('/');
    return Trim(slash == std::string::npos ? activity : activity.substr(0, slash));
}

std::string ParseFocusedPackage(const std::string& dumpsys) {
    std::string focus, app;
    std::istringstream in(dumpsys);
    for (std::string line; std::getline(in, line);) {
        if (std::string v = AfterKey(line, "mCurrentFocus="); !v.empty()) {
            if (v.rfind("null", 0) == 0) continue;
            if (std::string p = PackageInRecord(v); !p.empty()) focus = p;  // the last one wins
        } else if (std::string a = AfterKey(line, "mFocusedApp="); !a.empty()) {
            if (a.rfind("null", 0) == 0) continue;
            if (std::string p = PackageInRecord(a); !p.empty()) app = p;
        }
    }
    return focus.empty() ? app : focus;
}

bool ParseBootCompleted(const std::string& getprop) { return Trim(getprop) == "1"; }

std::vector<std::string> SplitCommandLine(const std::string& text) {
    std::vector<std::string> out;
    std::string cur;
    bool have = false, quoted = false;
    std::size_t backslashes = 0;
    for (char c : text) {
        if (c == '\\') {
            ++backslashes;
            continue;
        }
        if (c == '"') {
            cur.append(backslashes / 2, '\\');
            if (backslashes % 2) {
                cur += '"';
            } else {
                quoted = !quoted;
            }
            backslashes = 0;
            have = true;
            continue;
        }
        cur.append(backslashes, '\\');
        backslashes = 0;
        if ((c == ' ' || c == '\t' || c == '\r' || c == '\n') && !quoted) {
            if (have || !cur.empty()) out.push_back(cur);
            cur.clear();
            have = false;
            continue;
        }
        cur += c;
        have = true;
    }
    cur.append(backslashes, '\\');
    if (have || !cur.empty()) out.push_back(cur);
    return out;
}

std::string MuMuRoot(const std::string& path) {
    const std::string norm = Slashes(Trim(path));
    const std::string tail = "/nx_main/mumunxmain.exe";
    const std::string lower = Lower(norm);
    if (lower.size() <= tail.size() || lower.compare(lower.size() - tail.size(), tail.size(), tail) != 0)
        return {};
    return Trim(path).substr(0, norm.size() - tail.size());
}

std::string MuMuDevicePath(const std::string& path, const std::string& version) {
    const std::string root = MuMuRoot(path);
    if (root.empty() || version.empty()) return {};
    const char sep = path.find('\\') != std::string::npos ? '\\' : '/';
    std::string out = root;
    for (const char* part : {"nx_device", version.c_str(), "shell", "MuMuNxDevice.exe"}) {
        out += sep;
        out += part;
    }
    return out;
}

std::string NewestVersion(const std::vector<std::string>& versions) {
    std::string best;
    std::vector<long> best_parts;
    for (const auto& v : versions) {
        std::vector<long> parts;
        std::istringstream in(v);
        bool ok = !v.empty();
        for (std::string p; ok && std::getline(in, p, '.');) {
            if (p.empty() || !std::all_of(p.begin(), p.end(), [](unsigned char c) { return std::isdigit(c); })) {
                ok = false;
                break;
            }
            parts.push_back(std::stol(p));
        }
        if (!ok || parts.empty()) continue;
        if (best.empty() || parts > best_parts) {
            best = v;
            best_parts = std::move(parts);
        }
    }
    return best;
}

}  // namespace emu

EmulatorSession::EmulatorSession(IProcessRunner& runner, IDetachedProcesses& processes,
                                 EmulatorOptions options, LogFn log, SleepFn sleep)
    : runner_(runner),
      processes_(processes),
      options_(std::move(options)),
      log_(std::move(log)),
      sleep_(std::move(sleep)) {
    if (!sleep_) sleep_ = [](std::chrono::milliseconds d) { std::this_thread::sleep_for(d); };
    options_.poll_ms = std::max(options_.poll_ms, 1);
}

bool EmulatorSession::Fail(std::string message) {
    last_error_ = std::move(message);
    Log(log_, LogLevel::kError, "emulator: " + last_error_);
    return false;
}

bool EmulatorSession::Cancelled(const std::atomic<bool>* cancel) {
    if (cancel && cancel->load()) {
        last_error_ = "stopped";
        return true;
    }
    return false;
}

std::vector<std::string> EmulatorSession::EmulatorCommand() {
    std::string path = options_.emulator_path;
    std::vector<std::string> args = options_.emulator_args;
    const std::string root = emu::MuMuRoot(path);
    if (root.empty()) {
        args.insert(args.begin(), path);
        return args;
    }
    // MuMu launcher: start the instance directly (see the notes).
    std::string version, index = "0";
    std::vector<std::string> kept;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        if (a == "--from-shortcut") continue;
        if (a.rfind("--engine_series=", 0) == 0) {
            version = a.substr(16);
            continue;
        }
        if (a == "--engine_series" && i + 1 < args.size()) {
            version = args[++i];
            continue;
        }
        if (a == "-v" && i + 1 < args.size()) index = args[i + 1];
        kept.push_back(a);
    }
    const std::filesystem::path root_dir = PathFromUtf8(root);
    if (version.empty()) {
        const std::filesystem::path manager = root_dir / "nx_main" / "MuMuManager.exe";
        const ProcessResult r = runner_.Run({PathToUtf8(manager), "info", "-v", index}, Timeout());
        if (r.ok()) {
            try {
                const auto j = nlohmann::json::parse(r.out);
                if (j.is_object() && j.contains("android_version") && j["android_version"].is_string())
                    version = j["android_version"].get<std::string>();
            } catch (const nlohmann::json::exception&) {
            }
        }
    }
    if (version.empty()) {
        std::vector<std::string> found;
        std::error_code ec;
        for (std::filesystem::directory_iterator it(root_dir / "nx_device", ec), end; !ec && it != end;
             it.increment(ec)) {
            if (it->is_directory(ec)) found.push_back(PathToUtf8(it->path().filename()));
        }
        version = emu::NewestVersion(found);
    }
    const std::string device = emu::MuMuDevicePath(path, version);
    std::error_code ec;
    if (!device.empty() && std::filesystem::exists(PathFromUtf8(device), ec)) {
        Log(log_, LogLevel::kInfo, "emulator: MuMu launcher replaced by the instance " + device);
        path = device;
    } else {
        Log(log_, LogLevel::kWarn, "emulator: MuMu instance for Android " +
                                       (version.empty() ? std::string("?") : version) +
                                       " not found; starting the launcher as configured");
        kept = options_.emulator_args;
    }
    kept.insert(kept.begin(), path);
    return kept;
}

bool EmulatorSession::DeviceListed(bool& offline) {
    const bool network = options_.serial.find(':') != std::string::npos;
    if (network) {
        if (offline) runner_.Run({options_.adb_path, "disconnect", options_.serial}, Timeout());
        runner_.Run(adb::ConnectCommand(options_.adb_path, options_.serial), Timeout());
    }
    offline = false;
    const ProcessResult r = runner_.Run(adb::DevicesCommand(options_.adb_path), Timeout());
    if (!r.ok()) return false;
    for (const auto& d : adb::ParseDevices(r.out)) {
        if (d.serial != options_.serial) continue;
        if (d.state == "device") return true;
        offline = d.state == "offline";
    }
    return false;
}

bool EmulatorSession::Booted() {
    const ProcessResult r =
        runner_.Run(adb::ShellCommand(options_.adb_path, options_.serial, "getprop sys.boot_completed"), Timeout());
    return r.ok() && emu::ParseBootCompleted(r.out);
}

bool EmulatorSession::EnsureDevice(const std::atomic<bool>* cancel) {
    last_error_.clear();
    if (options_.serial.empty()) return Fail("no device serial configured");
    const int polls = std::max(1, options_.boot_timeout_ms / options_.poll_ms);
    const std::chrono::milliseconds poll{options_.poll_ms};
    int used = 0;

    bool offline = false;
    bool listed = DeviceListed(offline);
    if (!listed) {
        if (options_.emulator_path.empty())
            return Fail("device " + options_.serial +
                        " not found in `adb devices`; start the emulator or set its path in the settings");
        const std::vector<std::string> cmd = EmulatorCommand();
        std::string error;
        const std::string dir = PathToUtf8(PathFromUtf8(cmd.front()).parent_path());
        const long pid = processes_.Start(cmd, dir, error);
        if (!pid) return Fail("cannot start the emulator: " + error);
        started_pid_ = pid;
        Log(log_, LogLevel::kInfo, "emulator: started " + cmd.front() + " (pid " + std::to_string(pid) +
                                       "), waiting for " + options_.serial);
        bool checked_alive = false;
        while (!listed) {
            if (Cancelled(cancel)) return false;
            if (used++ >= polls)
                return Fail("device " + options_.serial + " did not appear within " +
                            std::to_string(options_.boot_timeout_ms / 1000) + " s");
            sleep_(poll);
            if (!checked_alive) {
                checked_alive = true;
                if (!processes_.Alive(started_pid_)) {
                    // A launcher that hands over to another process, or an emulator that was
                    // already starting. Keep waiting for the device; it cannot be closed later.
                    Log(log_, LogLevel::kWarn,
                        "emulator: the started process has already exited; the emulator will not be closed after the run");
                    started_pid_ = 0;
                }
            }
            listed = DeviceListed(offline);
        }
    }
    while (!Booted()) {
        if (Cancelled(cancel)) return false;
        if (used++ >= polls)
            return Fail("Android on " + options_.serial + " did not finish booting within " +
                        std::to_string(options_.boot_timeout_ms / 1000) + " s");
        sleep_(poll);
    }
    Log(log_, LogLevel::kInfo, "emulator: device " + options_.serial + " ready");
    return true;
}

std::string EmulatorSession::FocusedPackage() {
    const ProcessResult r =
        runner_.Run(adb::ShellCommand(options_.adb_path, options_.serial, "dumpsys window"), Timeout());
    return r.ok() ? emu::ParseFocusedPackage(r.out) : std::string();
}

bool EmulatorSession::StartGame() {
    const std::string pkg = emu::PackageOf(options_.game_activity);
    const ProcessResult r = runner_.Run(
        adb::ShellCommand(options_.adb_path, options_.serial, "am start -n " + options_.game_activity), Timeout());
    if (r.ok() && r.out.find("Error") == std::string::npos && r.err.find("Error") == std::string::npos)
        return true;
    Log(log_, LogLevel::kWarn, "emulator: am start failed, trying the launcher intent");
    const ProcessResult m = runner_.Run(
        adb::ShellCommand(options_.adb_path, options_.serial,
                          "monkey -p " + pkg + " -c android.intent.category.LAUNCHER 1"),
        Timeout());
    return m.ok();
}

bool EmulatorSession::EnsureGame(const std::atomic<bool>* cancel) {
    last_error_.clear();
    const std::string pkg = emu::PackageOf(options_.game_activity);
    if (pkg.empty()) return Fail("no game activity configured");
    if (FocusedPackage() == pkg) {
        Log(log_, LogLevel::kInfo, "emulator: the game is already running");
        return true;
    }
    const int polls = std::max(1, options_.game_timeout_ms / options_.poll_ms);
    const int restart_every = std::max(1, options_.game_restart_ms / options_.poll_ms);
    Log(log_, LogLevel::kInfo, "emulator: starting " + options_.game_activity);
    for (int i = 0; i < polls; ++i) {
        if (i % restart_every == 0) StartGame();
        if (Cancelled(cancel)) return false;
        sleep_(std::chrono::milliseconds{options_.poll_ms});
        if (FocusedPackage() == pkg) {
            Log(log_, LogLevel::kInfo, "emulator: the game is in front");
            return true;
        }
    }
    return Fail("the game (" + pkg + ") did not come to the front within " +
                std::to_string(options_.game_timeout_ms / 1000) + " s; is it installed?");
}

bool EmulatorSession::CloseGame() {
    const std::string pkg = emu::PackageOf(options_.game_activity);
    const ProcessResult r =
        runner_.Run(adb::ShellCommand(options_.adb_path, options_.serial, "am force-stop " + pkg), Timeout());
    if (!r.ok()) return Fail("cannot close the game: " + (r.error.empty() ? r.err : r.error));
    Log(log_, LogLevel::kInfo, "emulator: game closed");
    return true;
}

bool EmulatorSession::CloseEmulator() {
    if (!started_pid_) {
        Log(log_, LogLevel::kInfo, "emulator: not started by this run; left running");
        return true;
    }
    std::string error;
    const long pid = started_pid_;
    started_pid_ = 0;
    if (!processes_.KillTree(pid, error)) return Fail("cannot close the emulator: " + error);
    Log(log_, LogLevel::kInfo, "emulator: closed (pid " + std::to_string(pid) + ")");
    return true;
}

}  // namespace maba
