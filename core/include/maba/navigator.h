// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Navigator: page graph over pages/*.json, current-page detection, GoTo and return-to-home.
// Popups are settled through the global PopupHandler before every look at the screen.
// Every loop is bounded by step, back-press and wait caps.
#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "maba/controller.h"
#include "maba/log.h"
#include "maba/matcher.h"
#include "maba/page.h"
#include "maba/popup.h"
#include "maba/wait.h"

namespace maba {

// "<what> failed" plus the controller's reason when it has one.
std::string Failed(const char* what, const IController& controller);

// Shortest page path from `from` to `to` (both included) over page transitions (BFS).
// Transitions to unknown pages are ignored. Empty if unreachable or a page is unknown;
// {from} when from == to.
std::vector<std::string> FindPath(const std::map<std::string, PageDef>& pages,
                                  const std::string& from, const std::string& to);

// Data checks: every transition targets a known page and the home page exists.
// Returns one message per problem (empty = consistent).
std::vector<std::string> ValidatePageGraph(const std::map<std::string, PageDef>& pages,
                                           const std::string& home);

struct NavigatorOptions {
    std::string home = "main_menu";
    int max_steps = 12;         // clicks + back presses per GoTo
    int max_back_presses = 6;   // back presses on unknown screens per GoTo
    WaitOptions detect{std::chrono::milliseconds{4000}, std::chrono::milliseconds{300}, 30};
    WaitOptions arrive{std::chrono::milliseconds{8000}, std::chrono::milliseconds{300}, 40};
    std::chrono::milliseconds after_back{800};
    // Pause after arriving by a click: the game ignores input while a page is still sliding in.
    std::chrono::milliseconds after_arrive{700};
    const std::atomic<bool>* cancel = nullptr;
    // Called with the last frame when GoTo fails (e.g. save it under captures/).
    std::function<void(const Frame&, const std::string& reason)> on_failure;
};

struct Observation {
    bool ok = false;         // a frame was captured and no known popup is left on it
    Frame frame;
    std::string page;        // detected page id, empty if none
    double score = 0;
    std::string error;
    bool cancelled = false;
};

struct NavResult {
    bool ok = false;
    std::string page;        // page reached (or last recognized page on failure)
    std::string error;
    int steps = 0;
    bool cancelled = false;
};

class Navigator {
public:
    Navigator(IController& controller, IMatcher& matcher,
              const std::map<std::string, PageDef>& pages, PopupHandler& popups, IClock& clock,
              NavigatorOptions options = {}, LogFn log = {});

    // One look: settle popups, then detect the current page.
    Observation Observe();

    // Observe until `accept` returns true (bounded by `wait`). The last observation is returned;
    // check `accept` on it again or use the bool out-parameter.
    Observation ObserveUntil(const std::function<bool(const Observation&)>& accept,
                             const WaitOptions& wait, bool& accepted);

    NavResult GoTo(const std::string& target);
    NavResult ReturnHome() { return GoTo(options_.home); }

    // Start-up: while no known page is visible (title, event splash, loading), tap the home
    // page's `enter` point, at most `max_taps` times. Popups are settled on every look. Stops at
    // the first known page; succeeds at once when the home page has no `enter` entry.
    NavResult EnterGame();

    // Wait until `ref` is visible (popups settled on every attempt). nullopt on timeout.
    std::optional<MatchResult> WaitForTemplate(const TemplateRef& ref, const WaitOptions& wait);

    IController& Controller() { return controller_; }
    IMatcher& Matcher() { return matcher_; }
    IClock& Clock() { return clock_; }
    const std::map<std::string, PageDef>& Pages() const { return pages_; }
    const NavigatorOptions& Options() const { return options_; }
    const Frame& LastFrame() const { return last_frame_; }
    bool Cancelled() const { return options_.cancel && options_.cancel->load(); }

private:
    WaitOptions WithCancel(WaitOptions w) const;
    NavResult Fail(NavResult r, std::string reason);

    IController& controller_;
    IMatcher& matcher_;
    const std::map<std::string, PageDef>& pages_;
    PopupHandler& popups_;
    IClock& clock_;
    NavigatorOptions options_;
    LogFn log_;
    Frame last_frame_;
};

}  // namespace maba
