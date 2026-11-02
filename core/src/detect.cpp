// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#include "maba/detect.h"

#include <algorithm>

namespace maba {

PageDetection CheckAnchors(IMatcher& matcher, const Frame& frame,
                           const std::vector<TemplateRef>& anchors) {
    PageDetection d;
    if (anchors.empty()) return d;
    d.min_score = 1.0;
    d.detected = true;
    for (const auto& anchor : anchors) {
        const MatchResult r = matcher.Find(frame, anchor);
        d.min_score = std::min(d.min_score, r.score);
        if (!r.found && d.detected) {
            d.detected = false;
            d.failed_anchor = anchor.file;
        }
    }
    return d;
}

PageDetection DetectPage(IMatcher& matcher, const Frame& frame, const PageDef& page) {
    return CheckAnchors(matcher, frame, page.anchors);
}

CurrentPage DetectCurrentPage(IMatcher& matcher, const Frame& frame,
                              const std::map<std::string, PageDef>& pages) {
    CurrentPage best;
    std::size_t best_anchors = 0;
    for (const auto& [id, page] : pages) {  // std::map: ascending id, so ties keep the smaller id
        const PageDetection d = DetectPage(matcher, frame, page);
        if (!d.detected) continue;
        const bool better = best.id.empty() || d.min_score > best.score ||
                            (d.min_score == best.score && page.anchors.size() > best_anchors);
        if (better) {
            best = {id, d.min_score};
            best_anchors = page.anchors.size();
        }
    }
    return best;
}

}  // namespace maba
