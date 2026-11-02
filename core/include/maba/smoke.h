// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// End-to-end smoke check: detect a page, click its transition, confirm the target page appears.
// M0 uses it for main_menu -> mail. Popups are not handled here (that is the Navigator's job, M1):
// an unexpected popup makes the check fail with a saved frame.
#pragma once

#include <chrono>
#include <functional>
#include <map>
#include <string>

#include "maba/controller.h"
#include "maba/matcher.h"
#include "maba/page.h"
#include "maba/wait.h"

namespace maba {

struct PageDetection {
    bool detected = false;
    double min_score = 0;       // lowest anchor score (the weakest link)
    std::string failed_anchor;  // first anchor below its threshold
};

// A page is detected when every anchor matches.
PageDetection DetectPage(IMatcher& matcher, const Frame& frame, const PageDef& page);

struct SmokeOptions {
    std::string from_page = "main_menu";
    std::string to_page = "mail";
    WaitOptions detect{std::chrono::milliseconds{5000}, std::chrono::milliseconds{250}, 40};
    WaitOptions arrive{std::chrono::milliseconds{8000}, std::chrono::milliseconds{300}, 40};
    bool dry_run = false;  // detect and locate the click, but do not click
    // Called once with the last frame when the check fails (e.g. save it under captures/).
    std::function<void(const Frame&, const std::string& reason)> on_failure;
    std::function<void(const std::string&)> log;
};

struct SmokeResult {
    bool ok = false;
    std::string error;
    bool clicked = false;
    Point click_point;
    double from_score = 0;  // detection score of the start page
    double click_score = 0;  // match score of the clicked template (0 for fixed points)
    double to_score = 0;    // detection score of the target page
};

SmokeResult RunTransitionSmoke(IController& controller, IMatcher& matcher,
                               const std::map<std::string, PageDef>& pages,
                               const SmokeOptions& options, IClock& clock);

}  // namespace maba
