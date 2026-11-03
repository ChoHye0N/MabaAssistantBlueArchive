// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Starting and stopping the emulator and the game around a run: start the emulator when its
// device is missing, wait for Android to boot, bring the game to the front, and optionally close
// both afterwards. Portable (adb through IProcessRunner, the emulator through
// IDetachedProcesses), unit-tested with fakes. Behavior notes: docs/notes/emulator-launch.md.
#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <string>
#include <vector>

#include "maba/log.h"
#include "maba/process.h"

namespace maba {

namespace emu {

// Global client on Android (verify on each emulator).
inline constexpr const char* kGlobalActivity = "com.nexon.bluearchive/.MxUnityPlayerActivity";

// "com.example/.Main" -> "com.example".
std::string PackageOf(const std::string& activity);

// Package of the window in front, from `dumpsys window` output: the last non-null
// mCurrentFocus line, else mFocusedApp. "" if none.
std::string ParseFocusedPackage(const std::string& dumpsys);

// `getprop sys.boot_completed` output.
bool ParseBootCompleted(const std::string& getprop);

// Splits an argument string with the Windows rules (spaces separate, double quotes group,
// backslashes escape quotes only before a quote).
std::vector<std::string> SplitCommandLine(const std::string& text);

// MuMu builds with an `nx_main` folder: the launcher `.../nx_main/MuMuNxMain.exe` opens the
// instance manager; the instance is `.../nx_device/<version>/shell/MuMuNxDevice.exe`. Returns the
// device path for `version`, or "" if `path` is not that launcher. Separators may be / or \.
std::string MuMuDevicePath(const std::string& path, const std::string& version);
// The MuMu installation root ("..." above nx_main) of a launcher path, or "".
std::string MuMuRoot(const std::string& path);
// Highest dotted version ("12.0" < "15.0"); entries that are not versions are ignored.
std::string NewestVersion(const std::vector<std::string>& versions);

}  // namespace emu

struct EmulatorOptions {
    std::string adb_path = "adb";
    std::string serial;
    std::string emulator_path;              // empty: never start the emulator
    std::vector<std::string> emulator_args;
    std::string game_activity = emu::kGlobalActivity;
    int boot_timeout_ms = 180000;
    int game_timeout_ms = 90000;
    int poll_ms = 3000;
    int game_restart_ms = 15000;  // repeat the start command this often while waiting
    int command_timeout_ms = 15000;
};

class EmulatorSession {
public:
    using SleepFn = std::function<void(std::chrono::milliseconds)>;
    // `sleep` defaults to a real sleep (tests pass a no-op).
    EmulatorSession(IProcessRunner& runner, IDetachedProcesses& processes, EmulatorOptions options,
                    LogFn log = {}, SleepFn sleep = {});

    // The device is listed as `device` and Android has booted; starts the emulator first if the
    // device is missing and a path is configured.
    bool EnsureDevice(const std::atomic<bool>* cancel = nullptr);
    // The game is the app in front; starts it if not.
    bool EnsureGame(const std::atomic<bool>* cancel = nullptr);
    bool CloseGame();
    // Kills the emulator only if this session started it. True if nothing had to be done.
    bool CloseEmulator();

    bool StartedEmulator() const { return started_pid_ != 0; }
    const std::string& LastError() const { return last_error_; }
    // The emulator command after path rewriting (MuMu); for logs and tests.
    std::vector<std::string> EmulatorCommand();

private:
    bool DeviceListed(bool& offline);
    bool Booted();
    std::string FocusedPackage();
    bool StartGame();
    bool Cancelled(const std::atomic<bool>* cancel);
    bool Fail(std::string message);
    std::chrono::milliseconds Timeout() const { return std::chrono::milliseconds{options_.command_timeout_ms}; }

    IProcessRunner& runner_;
    IDetachedProcesses& processes_;
    EmulatorOptions options_;
    LogFn log_;
    SleepFn sleep_;
    long started_pid_ = 0;
    std::string last_error_;
};

}  // namespace maba
