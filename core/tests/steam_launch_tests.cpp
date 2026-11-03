// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Steam game start/close against a fake window host.
#include <atomic>
#include <chrono>
#include <string>
#include <vector>

#include "maba/steam_launch.h"
#include "test_framework.h"

namespace {

// The window appears after `appear_after` FindGame calls once the URL is opened, and goes away
// after `close_after` calls once a close was requested (-1: never).
class FakeWindow : public maba::IGameWindowHost {
public:
    bool OpenUrl(const std::string& url, std::string& error) override {
        opened.push_back(url);
        if (!open_ok) error = "no handler";
        if (open_ok) url_opened = true;
        return open_ok;
    }
    long FindGame() override {
        ++finds;
        if (running && close_requested && close_after >= 0 && finds_since_close++ >= close_after) running = false;
        if (!running && url_opened && !close_requested && finds_since_open++ >= appear_after) running = true;
        return running ? 777 : 0;
    }
    bool RequestClose(std::string&) override {
        ++close_requests;
        close_requested = true;
        return true;
    }
    bool running = false;
    bool open_ok = true;
    int appear_after = 2;
    int close_after = 1;
    int finds = 0, finds_since_open = 0, finds_since_close = 0, close_requests = 0;
    bool url_opened = false, close_requested = false;
    std::vector<std::string> opened;
};

class KillRecorder : public maba::IDetachedProcesses {
public:
    long Start(const std::vector<std::string>&, const std::string&, std::string&) override { return 0; }
    bool Alive(long) override { return false; }
    bool KillTree(long pid, std::string&) override {
        killed.push_back(pid);
        return true;
    }
    std::vector<long> killed;
};

struct Rig {
    FakeWindow window;
    KillRecorder processes;
    std::vector<std::chrono::milliseconds> sleeps;
    maba::SteamLaunchOptions options;
    maba::SteamSession Make() {
        return maba::SteamSession(window, processes, options, {},
                                  [this](std::chrono::milliseconds d) { sleeps.push_back(d); });
    }
    long long Slept() const {
        long long total = 0;
        for (auto d : sleeps) total += d.count();
        return total;
    }
};

}  // namespace

MABA_TEST(SteamGameAlreadyRunning) {
    Rig rig;
    rig.window.running = true;
    auto s = rig.Make();
    CHECK(s.EnsureGame());
    CHECK(rig.window.opened.empty());
    CHECK(!s.StartedGame());
    CHECK(rig.sleeps.empty());
}

MABA_TEST(SteamOpensUrlAndWaitsForWindow) {
    Rig rig;
    rig.options.poll_ms = 1000;
    rig.options.settle_ms = 2500;
    auto s = rig.Make();
    CHECK(s.EnsureGame());
    CHECK(rig.window.opened.size() == 1 && rig.window.opened[0] == "steam://rungameid/3557620");
    CHECK(s.StartedGame());
    // Two polls before the window appears, then 1000 + 1000 + 500 ms of settling.
    CHECK(rig.Slept() == 2000 + 2500);
}

MABA_TEST(SteamWindowTimeoutAndOpenFailure) {
    Rig rig;
    rig.window.appear_after = 1000;
    rig.options.window_timeout_ms = 10000;
    rig.options.poll_ms = 2000;
    auto s = rig.Make();
    CHECK(!s.EnsureGame());
    CHECK(s.LastError().find("did not appear within 10 s") != std::string::npos);
    CHECK(rig.Slept() == 10000);

    Rig bad;
    bad.window.open_ok = false;
    auto b = bad.Make();
    CHECK(!b.EnsureGame());
    CHECK(b.LastError().find("no handler") != std::string::npos);
    CHECK(!b.StartedGame());
}

MABA_TEST(SteamWaitStopsOnCancel) {
    Rig rig;
    rig.window.appear_after = 1000;
    std::atomic<bool> cancel{true};
    auto s = rig.Make();
    CHECK(!s.EnsureGame(&cancel));
    CHECK(s.LastError() == "stopped");
    CHECK(rig.sleeps.empty());
}

MABA_TEST(SteamCloseGame) {
    // Closes on WM_CLOSE: nothing killed.
    Rig rig;
    rig.window.running = true;
    auto s = rig.Make();
    CHECK(s.CloseGame());
    CHECK(rig.window.close_requests == 1);
    CHECK(rig.processes.killed.empty());

    // Ignores WM_CLOSE (quit confirmation): killed after the timeout.
    Rig stuck;
    stuck.window.running = true;
    stuck.window.close_after = -1;
    stuck.options.close_timeout_ms = 6000;
    stuck.options.poll_ms = 2000;
    auto t = stuck.Make();
    CHECK(t.CloseGame());
    CHECK(stuck.processes.killed.size() == 1 && stuck.processes.killed[0] == 777);
    CHECK(stuck.Slept() == 6000);

    // No game: nothing to do.
    Rig none;
    auto n = none.Make();
    CHECK(n.CloseGame());
    CHECK(none.window.close_requests == 0);
}
