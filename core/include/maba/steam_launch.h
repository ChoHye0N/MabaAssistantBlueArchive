// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Starting and closing the Steam PC client around a run: open the Steam game URL when the game
// window is missing, wait for the window, and optionally close the game afterwards. Window access
// goes through IGameWindowHost (Win32 in core/src/platform/windows/), so the logic is
// unit-tested with fakes. Behavior notes: docs/notes/emulator-launch.md (Steam section).
#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <string>

#include "maba/log.h"
#include "maba/process.h"

namespace maba {

// Steam app id of the global client (verify on the store page if it changes).
inline constexpr const char* kSteamAppId = "3557620";
inline constexpr const char* kSteamGameUrl = "steam://rungameid/3557620";

// The game window as seen by the operating system.
class IGameWindowHost {
public:
    virtual ~IGameWindowHost() = default;
    // Opens a URL with its registered handler (steam:// starts Steam and the game).
    virtual bool OpenUrl(const std::string& url, std::string& error) = 0;
    // Process id owning the visible game window; 0 when there is none.
    virtual long FindGame() = 0;
    // Restores a minimized game window (a minimized window cannot be captured). True if it was minimized.
    virtual bool RestoreGame() = 0;
    // Asks the game window to close, like the title-bar close button.
    virtual bool RequestClose(std::string& error) = 0;
};

// Win32: FindWindow by class and title (UTF-8), ShellExecute, WM_CLOSE. Elsewhere every call fails.
class SystemGameWindow : public IGameWindowHost {
public:
    SystemGameWindow(std::string window_class = "UnityWndClass", std::string window_title = "Blue Archive");
    bool OpenUrl(const std::string& url, std::string& error) override;
    long FindGame() override;
    bool RestoreGame() override;
    bool RequestClose(std::string& error) override;

private:
    std::string class_;
    std::string title_;
};

struct SteamLaunchOptions {
    std::string url = kSteamGameUrl;
    int window_timeout_ms = 180000;  // Steam may have to start and sign in first
    int settle_ms = 5000;            // after the window appears, before input (verify)
    int close_timeout_ms = 20000;    // after WM_CLOSE, before the process is killed
    int poll_ms = 2000;
};

class SteamSession {
public:
    using SleepFn = std::function<void(std::chrono::milliseconds)>;
    // `processes` kills the game when it does not close by itself. `sleep` defaults to a real sleep.
    SteamSession(IGameWindowHost& window, IDetachedProcesses& processes, SteamLaunchOptions options,
                 LogFn log = {}, SleepFn sleep = {});

    // The game window exists and is not minimized; opens the Steam URL and waits for it if not.
    bool EnsureGame(const std::atomic<bool>* cancel = nullptr);
    // Closes the game window (then kills the process after the timeout). True if no game is left.
    bool CloseGame();

    bool StartedGame() const { return started_; }
    const std::string& LastError() const { return last_error_; }

private:
    bool Cancelled(const std::atomic<bool>* cancel);
    bool Fail(std::string message);
    bool Settle(const std::atomic<bool>* cancel);

    IGameWindowHost& window_;
    IDetachedProcesses& processes_;
    SteamLaunchOptions options_;
    LogFn log_;
    SleepFn sleep_;
    bool started_ = false;
    std::string last_error_;
};

}  // namespace maba
