// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#include "maba/navigator.h"

#include "maba/screen_change.h"

#include <algorithm>
#include <deque>
#include <set>
#include <utility>

#include "maba/detect.h"

namespace maba {

std::vector<std::string> FindPath(const std::map<std::string, PageDef>& pages,
                                  const std::string& from, const std::string& to) {
    if (!pages.count(from) || !pages.count(to)) return {};
    if (from == to) return {from};
    std::map<std::string, std::string> parent;
    std::set<std::string> seen{from};
    std::deque<std::string> queue{from};
    while (!queue.empty()) {
        const std::string cur = queue.front();
        queue.pop_front();
        for (const auto& t : pages.at(cur).transitions) {  // file order = preference on ties
            if (!pages.count(t.to) || !seen.insert(t.to).second) continue;
            parent[t.to] = cur;
            if (t.to == to) {
                std::vector<std::string> path{to};
                for (std::string p = to; p != from;) {
                    p = parent[p];
                    path.insert(path.begin(), p);
                }
                return path;
            }
            queue.push_back(t.to);
        }
    }
    return {};
}

std::vector<std::string> ValidatePageGraph(const std::map<std::string, PageDef>& pages,
                                           const std::string& home) {
    std::vector<std::string> problems;
    if (!pages.count(home)) problems.push_back("home page '" + home + "' is not defined");
    for (const auto& [id, page] : pages) {
        for (const auto& t : page.transitions) {
            if (!pages.count(t.to))
                problems.push_back("page '" + id + "' has a transition to unknown page '" + t.to + "'");
        }
    }
    return problems;
}

Navigator::Navigator(IController& controller, IMatcher& matcher,
                     const std::map<std::string, PageDef>& pages, PopupHandler& popups,
                     IClock& clock, NavigatorOptions options, LogFn log)
    : controller_(controller),
      matcher_(matcher),
      pages_(pages),
      popups_(popups),
      clock_(clock),
      options_(std::move(options)),
      log_(std::move(log)) {}

WaitOptions Navigator::WithCancel(WaitOptions w) const {
    if (!w.cancel) w.cancel = options_.cancel;
    return w;
}

Observation Navigator::Observe() {
    Observation obs;
    if (Cancelled()) {
        obs.cancelled = true;
        obs.error = "cancelled";
        return obs;
    }
    SettleResult settle = popups_.Settle(controller_, clock_);
    obs.cancelled = settle.cancelled;
    if (!settle.frame.empty()) last_frame_ = settle.frame;
    obs.frame = std::move(settle.frame);
    if (!settle.ok) {
        obs.error = settle.error;
        return obs;
    }
    obs.ok = true;
    const CurrentPage cur = DetectCurrentPage(matcher_, obs.frame, pages_);
    obs.page = cur.id;
    obs.score = cur.score;
    return obs;
}

Observation Navigator::ObserveUntil(const std::function<bool(const Observation&)>& accept,
                                    const WaitOptions& wait, bool& accepted) {
    Observation last;
    const WaitResult r = WaitUntil(
        [&] {
            last = Observe();
            return last.ok && accept(last);
        },
        WithCancel(wait), clock_);
    accepted = r.ok;
    if (r.cancelled) {
        last.cancelled = true;
        last.error = "cancelled";
    }
    return last;
}

bool Navigator::WaitSettled(std::chrono::milliseconds timeout) {
    const SettleOptions& o = options_.settle;
    Frame previous = controller_.Screenshot();
    const auto start = clock_.Now();
    int resting = 0;
    // The attempt cap holds even if the clock misbehaves.
    const auto attempts = timeout.count() / std::max<long long>(o.interval.count(), 1) + 1;
    for (long long i = 0; i < attempts && clock_.Now() - start < timeout; ++i) {
        if (options_.cancel && options_.cancel->load()) return false;
        clock_.Sleep(o.interval);
        Frame current = controller_.Screenshot();
        if (current.empty()) return false;
        resting = !IsLoadingScreen(current) && ChangedShare(previous, current) < o.max_changed ? resting + 1 : 0;
        previous = std::move(current);
        if (resting >= o.looks) return true;
    }
    return false;
}

std::string Failed(const char* what, const IController& controller) {
    const std::string why = controller.Error();
    return std::string(what) + " failed" + (why.empty() ? "" : ": " + why);
}

NavResult Navigator::Fail(NavResult r, std::string reason) {
    r.ok = false;
    r.error = std::move(reason);
    Log(log_, r.cancelled ? LogLevel::kInfo : LogLevel::kWarn, "navigation: " + r.error);
    if (!r.cancelled && options_.on_failure) options_.on_failure(last_frame_, r.error);
    return r;
}

NavResult Navigator::GoTo(const std::string& target) {
    NavResult result;
    if (!pages_.count(target)) return Fail(result, "unknown page: " + target);
    int backs = 0;
    std::chrono::milliseconds waited{0};

    while (true) {
        bool recognized = false;
        const Observation obs = ObserveUntil(
            [](const Observation& o) { return !o.page.empty(); }, options_.detect, recognized);
        if (obs.cancelled) {
            result.cancelled = true;
            return Fail(result, "cancelled");
        }
        if (recognized) result.page = obs.page;

        if (recognized && obs.page == target) {
            if (result.steps > 0) {
                clock_.Sleep(options_.after_arrive);
                if (options_.settle.enabled) WaitSettled(options_.settle.timeout);
            }
            result.ok = true;
            Log(log_, LogLevel::kDebug, "at page '" + target + "'");
            return result;
        }
        // Never back out of a battle; it ends by itself.
        const bool waiting_page = recognized && pages_.at(obs.page).wait.count() > 0;
        if (waiting_page) waited_at_ = clock_.Now();
        const bool just_left = !recognized && waited_at_ && clock_.Now() - *waited_at_ < options_.wait_grace;
        if (waiting_page || just_left) {
            if (waiting_page && waited >= pages_.at(obs.page).wait)
                return Fail(result, "still waiting for '" + obs.page + "' to end");
            if (waited.count() == 0) Log(log_, LogLevel::kInfo, "waiting for the battle to end");
            clock_.Sleep(options_.wait_interval);
            waited += options_.wait_interval;
            continue;
        }
        if (result.steps >= options_.max_steps) {
            return Fail(result, "could not reach '" + target + "' within " +
                                    std::to_string(options_.max_steps) + " steps" +
                                    (recognized ? " (stuck at '" + obs.page + "')" : ""));
        }

        // Unknown screen, or no route from here: step back and look again.
        const std::vector<std::string> path =
            recognized ? FindPath(pages_, obs.page, target) : std::vector<std::string>{};
        if (path.size() < 2) {
            if (backs >= options_.max_back_presses) {
                std::string why = recognized ? "no route from '" + obs.page + "' to '" + target + "'"
                                             : "screen not recognized";
                if (!obs.error.empty()) why += " (" + obs.error + ")";
                return Fail(result, why + " after " + std::to_string(backs) + " back presses");
            }
            Log(log_, LogLevel::kInfo,
                recognized ? "no route from '" + obs.page + "', pressing back"
                           : "unknown screen, pressing back");
            if (!controller_.Back()) return Fail(result, Failed("back", controller_));
            ++backs;
            ++result.steps;
            clock_.Sleep(options_.after_back);
            continue;
        }

        const std::string& next = path[1];
        const Transition* t = pages_.at(obs.page).FindTransition(next);
        Point click;
        if (t->click_template) {
            const MatchResult m = matcher_.Find(obs.frame, *t->click_template);
            if (!m.found) {
                // Often an animation still running: spend a step and look again.
                Log(log_, LogLevel::kInfo, "transition target " + t->click_template->file +
                                               " not visible on '" + obs.page + "'");
                ++result.steps;
                clock_.Sleep(options_.detect.interval);
                continue;
            }
            click = m.center;
        } else {
            click = *t->click_point;
        }
        Log(log_, LogLevel::kInfo, "'" + obs.page + "' -> '" + next + "'");
        if (!controller_.Click(click)) return Fail(result, Failed("click", controller_));
        ++result.steps;

        const std::string from = obs.page;
        bool moved = false;
        const Observation after = ObserveUntil(
            [&](const Observation& o) { return !o.page.empty() && o.page != from; },
            options_.arrive, moved);
        if (after.cancelled) {
            result.cancelled = true;
            return Fail(result, "cancelled");
        }
        if (moved && after.page != next)
            Log(log_, LogLevel::kInfo, "expected '" + next + "', got '" + after.page + "'");
        // Not moved: the loop re-detects and retries within the step budget.
    }
}

NavResult Navigator::EnterGame() {
    NavResult result;
    const auto home = pages_.find(options_.home);
    if (home == pages_.end() || !home->second.enter) {
        result.ok = true;
        return result;
    }
    const EnterSpec& enter = *home->second.enter;
    std::string last_error;

    while (true) {
        bool recognized = false;
        const Observation obs = ObserveUntil(
            [](const Observation& o) { return !o.page.empty(); }, options_.detect, recognized);
        if (obs.cancelled) {
            result.cancelled = true;
            return Fail(result, "cancelled");
        }
        if (recognized) {
            result.ok = true;
            result.page = obs.page;
            if (result.steps > 0)
                Log(log_, LogLevel::kInfo, "entered the game at '" + obs.page + "' after " +
                                               std::to_string(result.steps) + " taps");
            return result;
        }
        if (obs.frame.empty())
            return Fail(result, "capture failed" + (obs.error.empty() ? "" : " (" + obs.error + ")"));
        if (result.steps >= enter.max_taps)
            return Fail(result, "no known page after " + std::to_string(enter.max_taps) + " taps" +
                                    (last_error.empty() ? "" : " (" + last_error + ")"));

        Point tap = enter.tap;
        std::string where = "start-up screen";
        for (const EnterTap& screen : enter.screens) {
            if (matcher_.Find(obs.frame, screen.when).found) {
                tap = screen.tap;
                where = "start-up screen (" + screen.when.file + ")";
                break;
            }
        }
        Log(log_, LogLevel::kInfo, where + ", tapping");
        // A game that is still loading does not answer for a while and refuses input then. Such a
        // tap counts like one that changed nothing: the next round tries again, up to the cap.
        if (!controller_.Click(tap)) {
            last_error = Failed("click", controller_);
            Log(log_, LogLevel::kInfo, "the tap was refused (" + last_error + "), trying again");
        }
        ++result.steps;
        clock_.Sleep(enter.interval);
    }
}

std::optional<MatchResult> Navigator::WaitForTemplate(const TemplateRef& ref,
                                                      const WaitOptions& wait) {
    MatchResult found;
    const WaitResult r = WaitUntil(
        [&] {
            const Observation obs = Observe();
            if (!obs.ok) return false;
            found = matcher_.Find(obs.frame, ref);
            return found.found;
        },
        WithCancel(wait), clock_);
    if (!r.ok) return std::nullopt;
    return found;
}

}  // namespace maba
