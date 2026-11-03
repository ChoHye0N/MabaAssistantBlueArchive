// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Emulator and game start/stop against a scripted adb and a fake process table.
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

#include "maba/emulator.h"
#include "test_framework.h"

namespace {

maba::ProcessResult Ok(std::string out = {}) {
    maba::ProcessResult r;
    r.started = true;
    r.exit_code = 0;
    r.out = std::move(out);
    return r;
}

// adb whose answers depend on the state of the test (device up, game in front, ...).
class ScriptRunner : public maba::IProcessRunner {
public:
    maba::ProcessResult Run(const std::vector<std::string>& argv, std::chrono::milliseconds) override {
        std::string cmd;
        for (const auto& a : argv) cmd += (cmd.empty() ? "" : " ") + a;
        commands.push_back(cmd);
        return handler ? handler(cmd) : Ok();
    }
    int Count(const std::string& part) const {
        int n = 0;
        for (const auto& c : commands) n += c.find(part) != std::string::npos;
        return n;
    }
    std::function<maba::ProcessResult(const std::string&)> handler;
    std::vector<std::string> commands;
};

class FakeProcesses : public maba::IDetachedProcesses {
public:
    long Start(const std::vector<std::string>& argv, const std::string& dir, std::string&) override {
        started.push_back(argv);
        started_dir = dir;
        return next_pid;
    }
    bool Alive(long pid) override { return alive && pid == next_pid; }
    bool KillTree(long pid, std::string&) override {
        killed.push_back(pid);
        return true;
    }
    long next_pid = 4242;
    bool alive = true;
    std::vector<std::vector<std::string>> started;
    std::string started_dir;
    std::vector<long> killed;
};

const char* const kDevicesUp = "List of devices attached\n127.0.0.1:16384\tdevice\n\n";
const char* const kDevicesNone = "List of devices attached\n\n";
const char* const kFocusGame =
    "  mCurrentFocus=Window{5e1 u0 com.nexon.bluearchive/com.nexon.bluearchive.MxUnityPlayerActivity}\n";
const char* const kFocusLauncher = "  mCurrentFocus=Window{9a0 u0 com.android.launcher3/.Launcher}\n";

maba::EmulatorOptions Options() {
    maba::EmulatorOptions o;
    o.serial = "127.0.0.1:16384";
    o.poll_ms = 1000;
    o.boot_timeout_ms = 10000;
    o.game_timeout_ms = 10000;
    o.game_restart_ms = 3000;
    return o;
}

void NoSleep(std::chrono::milliseconds) {}

}  // namespace

MABA_TEST(EmuParsesFocusedPackage) {
    CHECK(maba::emu::ParseFocusedPackage(kFocusGame) == "com.nexon.bluearchive");
    // The last non-null focus line wins; null lines are skipped.
    CHECK(maba::emu::ParseFocusedPackage(std::string(kFocusLauncher) + kFocusGame +
                                         "  mCurrentFocus=null\r\n") == "com.nexon.bluearchive");
    // No focus (MuMu keep-alive): the focused app record.
    CHECK(maba::emu::ParseFocusedPackage(
              "  mCurrentFocus=null\n  mFocusedApp=ActivityRecord{77 u0 com.nexon.bluearchive/.MxUnityPlayerActivity t31}\n") ==
          "com.nexon.bluearchive");
    // A window without an activity still names its package; a system window does not.
    CHECK(maba::emu::ParseFocusedPackage("mCurrentFocus=Window{1 u0 com.nexon.bluearchive}") ==
          "com.nexon.bluearchive");
    CHECK(maba::emu::ParseFocusedPackage("mCurrentFocus=Window{1 u0 NotificationShade}").empty());
    CHECK(maba::emu::ParseFocusedPackage("").empty());
}

MABA_TEST(EmuSmallParsers) {
    CHECK(maba::emu::PackageOf("com.nexon.bluearchive/.MxUnityPlayerActivity") == "com.nexon.bluearchive");
    CHECK(maba::emu::PackageOf("com.example") == "com.example");
    CHECK(maba::emu::ParseBootCompleted("1\r\n"));
    CHECK(!maba::emu::ParseBootCompleted(""));
    CHECK(!maba::emu::ParseBootCompleted("0\n"));
    CHECK(maba::emu::NewestVersion({"12.0", "15.0", "9.1", "x", ""}) == "15.0");
    CHECK(maba::emu::NewestVersion({"12.10", "12.9"}) == "12.10");
    CHECK(maba::emu::NewestVersion({"abc"}).empty());
}

MABA_TEST(EmuSplitsCommandLine) {
    using V = std::vector<std::string>;
    CHECK(maba::emu::SplitCommandLine("") == V{});
    CHECK(maba::emu::SplitCommandLine("  -v 0  ") == (V{"-v", "0"}));
    CHECK(maba::emu::SplitCommandLine("--instance \"Nougat 64\" index=1") ==
          (V{"--instance", "Nougat 64", "index=1"}));
    CHECK(maba::emu::SplitCommandLine("a\\b \"c\\\"d\" \"\"") == (V{"a\\b", "c\"d", ""}));
}

MABA_TEST(EmuMuMuLauncherPath) {
    const std::string launcher = "C:\\Program Files\\Netease\\MuMu\\nx_main\\MuMuNxMain.exe";
    CHECK(maba::emu::MuMuRoot(launcher) == "C:\\Program Files\\Netease\\MuMu");
    CHECK(maba::emu::MuMuDevicePath(launcher, "12.0") ==
          "C:\\Program Files\\Netease\\MuMu\\nx_device\\12.0\\shell\\MuMuNxDevice.exe");
    CHECK(maba::emu::MuMuDevicePath("D:/MuMu/NX_MAIN/mumunxmain.exe", "15.0") ==
          "D:/MuMu/nx_device/15.0/shell/MuMuNxDevice.exe");
    CHECK(maba::emu::MuMuRoot("C:\\LDPlayer\\LDPlayer9\\dnplayer.exe").empty());
    CHECK(maba::emu::MuMuDevicePath("C:\\LDPlayer\\dnplayer.exe", "12.0").empty());
}

MABA_TEST(EmuMuMuCommandRewrite) {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "maba_emu_test_mumu";
    fs::remove_all(root);
    fs::create_directories(root / "nx_main");
    fs::create_directories(root / "nx_device" / "12.0" / "shell");
    fs::create_directories(root / "nx_device" / "15.0" / "shell");
    std::ofstream(root / "nx_device" / "12.0" / "shell" / "MuMuNxDevice.exe") << "x";
    std::ofstream(root / "nx_device" / "15.0" / "shell" / "MuMuNxDevice.exe") << "x";
    const std::string launcher = (root / "nx_main" / "MuMuNxMain.exe").string();
    const std::string sep(1, static_cast<char>(fs::path::preferred_separator));
    ScriptRunner runner;
    FakeProcesses procs;

    {  // --engine_series picks the version and is removed with --from-shortcut
        auto o = Options();
        o.emulator_path = launcher;
        o.emulator_args = {"-v", "1", "--engine_series=12.0", "--from-shortcut"};
        maba::EmulatorSession s(runner, procs, o, {}, NoSleep);
        const auto cmd = s.EmulatorCommand();
        CHECK(cmd.size() == 3);
        CHECK(cmd[0].find("12.0" + sep + "shell" + sep + "MuMuNxDevice.exe") != std::string::npos);
        CHECK(cmd[1] == "-v" && cmd[2] == "1");
        CHECK(runner.Count("MuMuManager") == 0);
    }
    {  // the manager reports the instance's Android version
        runner.handler = [](const std::string& c) {
            return c.find("MuMuManager.exe info -v 2") != std::string::npos
                       ? Ok("{\"android_version\": \"12.0\", \"index\": \"2\"}")
                       : Ok();
        };
        auto o = Options();
        o.emulator_path = launcher;
        o.emulator_args = {"-v", "2"};
        maba::EmulatorSession s(runner, procs, o, {}, NoSleep);
        const auto cmd = s.EmulatorCommand();
        CHECK(cmd[0].find("12.0") != std::string::npos);
    }
    {  // no manager answer: the newest nx_device folder
        runner.handler = {};
        auto o = Options();
        o.emulator_path = launcher;
        maba::EmulatorSession s(runner, procs, o, {}, NoSleep);
        CHECK(s.EmulatorCommand()[0].find("15.0") != std::string::npos);
    }
    {  // other emulators are started as configured
        auto o = Options();
        o.emulator_path = "C:\\LDPlayer\\LDPlayer9\\dnplayer.exe";
        o.emulator_args = {"index=0"};
        maba::EmulatorSession s(runner, procs, o, {}, NoSleep);
        CHECK(s.EmulatorCommand() == (std::vector<std::string>{o.emulator_path, "index=0"}));
    }
    fs::remove_all(root);
}

MABA_TEST(EmuDeviceAlreadyRunning) {
    ScriptRunner runner;
    FakeProcesses procs;
    runner.handler = [](const std::string& c) {
        if (c == "adb devices") return Ok(kDevicesUp);
        if (c.find("getprop sys.boot_completed") != std::string::npos) return Ok("1\n");
        return Ok();
    };
    auto o = Options();
    o.emulator_path = "C:\\emu\\emu.exe";
    maba::EmulatorSession s(runner, procs, o, {}, NoSleep);
    CHECK(s.EnsureDevice());
    CHECK(procs.started.empty());
    CHECK(!s.StartedEmulator());
    CHECK(runner.Count("adb connect 127.0.0.1:16384") == 1);
    // Not started here: never closed.
    CHECK(s.CloseEmulator());
    CHECK(procs.killed.empty());
}

MABA_TEST(EmuStartsEmulatorAndWaitsForBoot) {
    ScriptRunner runner;
    FakeProcesses procs;
    int devices_calls = 0, boot_calls = 0;
    runner.handler = [&](const std::string& c) {
        if (c == "adb devices") {
            ++devices_calls;
            if (devices_calls < 3) return Ok(kDevicesNone);
            if (devices_calls == 3) return Ok("List of devices attached\n127.0.0.1:16384\toffline\n");
            return Ok(kDevicesUp);
        }
        if (c.find("getprop sys.boot_completed") != std::string::npos) return Ok(++boot_calls < 3 ? "" : "1");
        return Ok();
    };
    auto o = Options();
    o.emulator_path = "C:\\emu\\emu.exe";
    o.emulator_args = {"-v", "0"};
    maba::EmulatorSession s(runner, procs, o, {}, NoSleep);
    CHECK(s.EnsureDevice());
    CHECK(procs.started.size() == 1);
    CHECK(procs.started[0] == (std::vector<std::string>{"C:\\emu\\emu.exe", "-v", "0"}));
    CHECK(s.StartedEmulator());
    CHECK(boot_calls == 3);
    // The offline device was disconnected before the next connect.
    CHECK(runner.Count("adb disconnect 127.0.0.1:16384") == 1);
    CHECK(s.CloseEmulator());
    CHECK(procs.killed == std::vector<long>{4242});
    CHECK(!s.StartedEmulator());
}

MABA_TEST(EmuDeviceMissingWithoutPathOrTimeout) {
    ScriptRunner runner;
    FakeProcesses procs;
    runner.handler = [](const std::string& c) { return Ok(c == "adb devices" ? kDevicesNone : ""); };
    {
        maba::EmulatorSession s(runner, procs, Options(), {}, NoSleep);
        CHECK(!s.EnsureDevice());
        CHECK(s.LastError().find("not found") != std::string::npos);
        CHECK(procs.started.empty());
    }
    {
        auto o = Options();
        o.emulator_path = "emu.exe";
        maba::EmulatorSession s(runner, procs, o, {}, NoSleep);
        CHECK(!s.EnsureDevice());
        CHECK(s.LastError().find("did not appear") != std::string::npos);
        // Bounded: boot_timeout / poll polls.
        CHECK(runner.Count("adb devices") <= 12);
    }
    {  // a stop request ends the wait
        auto o = Options();
        o.emulator_path = "emu.exe";
        std::atomic<bool> cancel{true};
        maba::EmulatorSession s(runner, procs, o, {}, NoSleep);
        CHECK(!s.EnsureDevice(&cancel));
        CHECK(s.LastError() == "stopped");
    }
}

MABA_TEST(EmuLauncherProcessExitsEarly) {
    ScriptRunner runner;
    FakeProcesses procs;
    procs.alive = false;  // e.g. a launcher that hands over to another process
    int devices_calls = 0;
    runner.handler = [&](const std::string& c) {
        if (c == "adb devices") return Ok(++devices_calls < 3 ? kDevicesNone : kDevicesUp);
        if (c.find("getprop") != std::string::npos) return Ok("1");
        return Ok();
    };
    auto o = Options();
    o.emulator_path = "emu.exe";
    maba::EmulatorSession s(runner, procs, o, {}, NoSleep);
    CHECK(s.EnsureDevice());
    CHECK(!s.StartedEmulator());
    CHECK(s.CloseEmulator());
    CHECK(procs.killed.empty());
}

MABA_TEST(EmuStartsGame) {
    ScriptRunner runner;
    FakeProcesses procs;
    int focus_calls = 0;
    runner.handler = [&](const std::string& c) {
        if (c.find("dumpsys window") != std::string::npos) return Ok(++focus_calls < 4 ? kFocusLauncher : kFocusGame);
        return Ok("Starting: Intent { cmp=com.nexon.bluearchive/.MxUnityPlayerActivity }\n");
    };
    maba::EmulatorSession s(runner, procs, Options(), {}, NoSleep);
    CHECK(s.EnsureGame());
    CHECK(runner.Count("am start -n com.nexon.bluearchive/.MxUnityPlayerActivity") == 1);
    CHECK(runner.Count("monkey") == 0);

    CHECK(s.CloseGame());
    CHECK(runner.Count("am force-stop com.nexon.bluearchive") == 1);
}

MABA_TEST(EmuGameAlreadyInFront) {
    ScriptRunner runner;
    FakeProcesses procs;
    runner.handler = [](const std::string& c) {
        return Ok(c.find("dumpsys window") != std::string::npos ? kFocusGame : "");
    };
    maba::EmulatorSession s(runner, procs, Options(), {}, NoSleep);
    CHECK(s.EnsureGame());
    CHECK(runner.Count("am start") == 0);
}

MABA_TEST(EmuGameStartFallbackAndTimeout) {
    ScriptRunner runner;
    FakeProcesses procs;
    runner.handler = [](const std::string& c) {
        if (c.find("dumpsys window") != std::string::npos) return Ok(kFocusLauncher);
        if (c.find("am start") != std::string::npos)
            return Ok("Error: Activity class {com.nexon.bluearchive/.MxUnityPlayerActivity} does not exist.\n");
        return Ok();
    };
    maba::EmulatorSession s(runner, procs, Options(), {}, NoSleep);
    CHECK(!s.EnsureGame());
    CHECK(s.LastError().find("did not come to the front") != std::string::npos);
    // 10 polls, the start repeated every 3: at polls 0, 3, 6, 9.
    CHECK(runner.Count("am start -n") == 4);
    CHECK(runner.Count("monkey -p com.nexon.bluearchive -c android.intent.category.LAUNCHER 1") == 4);
}
