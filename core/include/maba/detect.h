// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Anchor checks shared by page detection, popup detection and the Navigator.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "maba/matcher.h"
#include "maba/page.h"

namespace maba {

struct PageDetection {
    bool detected = false;
    double min_score = 0;       // lowest anchor score (the weakest link)
    std::string failed_anchor;  // first anchor below its threshold
};

// Detected when every anchor matches (an empty list never matches).
PageDetection CheckAnchors(IMatcher& matcher, const Frame& frame,
                           const std::vector<TemplateRef>& anchors);

// A page is detected when every anchor matches.
PageDetection DetectPage(IMatcher& matcher, const Frame& frame, const PageDef& page);

struct CurrentPage {
    std::string id;        // empty when no page is detected
    double score = 0;      // min anchor score of the chosen page
};

// The detected page with the highest score; ties go to the page with more anchors
// (the more specific one), then to the smaller id.
CurrentPage DetectCurrentPage(IMatcher& matcher, const Frame& frame,
                              const std::map<std::string, PageDef>& pages);

}  // namespace maba
