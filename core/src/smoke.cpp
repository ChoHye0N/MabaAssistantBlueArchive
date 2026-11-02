// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#include "maba/smoke.h"

#include <algorithm>
#include <cstdio>

namespace maba {

namespace {

std::string Score(double s) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%.3f", s);
    return buf;
}

}  // namespace

SmokeResult RunTransitionSmoke(IController& controller, IMatcher& matcher,
                               const std::map<std::string, PageDef>& pages,
                               const SmokeOptions& options, IClock& clock) {
    SmokeResult result;
    Frame last;
    auto log = [&](const std::string& msg) {
        if (options.log) options.log(msg);
    };
    auto fail = [&](std::string reason) {
        result.error = std::move(reason);
        log("FAILED: " + result.error);
        if (options.on_failure) options.on_failure(last, result.error);
        return result;
    };

    const auto from_it = pages.find(options.from_page);
    const auto to_it = pages.find(options.to_page);
    if (from_it == pages.end()) return fail("unknown page: " + options.from_page);
    if (to_it == pages.end()) return fail("unknown page: " + options.to_page);
    const PageDef& from = from_it->second;
    const PageDef& to = to_it->second;
    const Transition* transition = from.FindTransition(to.id);
    if (!transition) return fail("page '" + from.id + "' has no transition to '" + to.id + "'");

    // 1. Wait for the start page.
    PageDetection detection;
    int empty_frames = 0;
    const WaitResult found = WaitUntil(
        [&] {
            Frame f = controller.Screenshot();
            if (f.empty()) {
                ++empty_frames;
                return false;
            }
            last = std::move(f);
            detection = DetectPage(matcher, last, from);
            return detection.detected;
        },
        options.detect, clock);
    result.from_score = detection.min_score;
    if (!found.ok) {
        if (last.empty()) return fail("no frame captured (" + std::to_string(empty_frames) + " attempts)");
        std::string why = "page '" + from.id + "' not detected after " +
                          std::to_string(found.attempts) + " attempts (best score " +
                          Score(detection.min_score) + ")";
        if (!detection.failed_anchor.empty()) why += ", anchor " + detection.failed_anchor;
        if (!matcher.LastError().empty()) why += ": " + matcher.LastError();
        return fail(why);
    }
    log("detected '" + from.id + "' (score " + Score(detection.min_score) + ")");

    // 2. Locate the click target.
    if (transition->click_template) {
        const MatchResult target = matcher.Find(last, *transition->click_template);
        result.click_score = target.score;
        if (!target.found) {
            return fail("click target " + transition->click_template->file + " not found (score " +
                        Score(target.score) + ")");
        }
        result.click_point = target.center;
    } else {
        result.click_point = *transition->click_point;
    }
    log("click target (" + std::to_string(result.click_point.x) + ", " +
        std::to_string(result.click_point.y) + ")");

    if (options.dry_run) {
        result.ok = true;
        log("dry run: not clicking");
        return result;
    }

    // 3. Click and wait for the target page.
    if (!controller.Click(result.click_point)) return fail("click failed");
    result.clicked = true;

    detection = {};
    const WaitResult arrived = WaitUntil(
        [&] {
            Frame f = controller.Screenshot();
            if (f.empty()) return false;
            last = std::move(f);
            detection = DetectPage(matcher, last, to);
            return detection.detected;
        },
        options.arrive, clock);
    result.to_score = detection.min_score;
    if (!arrived.ok) {
        return fail("page '" + to.id + "' did not appear after the click (best score " +
                    Score(detection.min_score) + ")");
    }
    log("arrived at '" + to.id + "' (score " + Score(detection.min_score) + ")");
    result.ok = true;
    return result;
}

}  // namespace maba
