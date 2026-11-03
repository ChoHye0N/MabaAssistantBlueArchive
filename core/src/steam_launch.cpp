// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#include "maba/steam_launch.h"

#include <algorithm>
#include <thread>
#include <utility>

namespace maba {

SteamSession::SteamSession(IGameWindowHost& window, IDetachedProcesses& processes, SteamLaunchOptions options,
                           LogFn log, SleepFn sleep)
    : window_(window),
      processes_(processes),
      options_(std::move(options)),
      log_(std::move(log)),
      sleep_(std::move(sleep)) {
    if (!sleep_) sleep_ = [](std::chrono::milliseconds d) { std::this_thread::sleep_for(d); };
    options_.poll_ms = std::max(options_.poll_ms, 1);
}

bool SteamSession::Fail(std::string message) {
    last_error_ = std::move(message);
    Log(log_, LogLevel::kError, "steam: " + last_error_);
    return false;
}

bool SteamSession::Cancelled(const std::atomic<bool>* cancel) {
    if (cancel && cancel->load()) {
        last_error_ = "stopped";
        return true;
    }
    return false;
}

bool SteamSession::EnsureGame(const std::atomic<bool>* cancel) {
    last_error_.clear();
    if (window_.FindGame() != 0) {
        Log(log_, LogLevel::kInfo, "steam: the game is already running");
        return true;
    }
    std::string error;
    if (!window_.OpenUrl(options_.url, error)) return Fail("cannot open " + options_.url + ": " + error);
    started_ = true;
    Log(log_, LogLevel::kInfo, "steam: opened " + options_.url + ", waiting for the game window");
    const std::chrono::milliseconds poll{options_.poll_ms};
    const int polls = std::max(1, options_.window_timeout_ms / options_.poll_ms);
    for (int used = 0; window_.FindGame() == 0; ++used) {
        if (Cancelled(cancel)) return false;
        if (used >= polls)
            return Fail("the game window did not appear within " + std::to_string(options_.window_timeout_ms / 1000) +
                        " s (is Steam signed in and the game installed?)");
        sleep_(poll);
    }
    // The window exists before the game draws anything; give it time to settle its size.
    for (int waited = 0; waited < options_.settle_ms; waited += options_.poll_ms) {
        if (Cancelled(cancel)) return false;
        sleep_(std::chrono::milliseconds{std::min(options_.poll_ms, options_.settle_ms - waited)});
    }
    Log(log_, LogLevel::kInfo, "steam: game window found");
    return true;
}

bool SteamSession::CloseGame() {
    last_error_.clear();
    const long pid = window_.FindGame();
    if (pid == 0) return true;
    std::string error;
    if (!window_.RequestClose(error)) Log(log_, LogLevel::kWarn, "steam: cannot ask the game to close: " + error);
    const std::chrono::milliseconds poll{options_.poll_ms};
    const int polls = std::max(1, options_.close_timeout_ms / options_.poll_ms);
    for (int used = 0; used < polls; ++used) {
        sleep_(poll);
        if (window_.FindGame() == 0) {
            Log(log_, LogLevel::kInfo, "steam: game closed");
            return true;
        }
    }
    // Still open (for example a quit confirmation): end the process.
    if (!processes_.KillTree(pid, error)) return Fail("cannot end the game (pid " + std::to_string(pid) + "): " + error);
    Log(log_, LogLevel::kInfo, "steam: game process ended (pid " + std::to_string(pid) + ")");
    return true;
}

}  // namespace maba
