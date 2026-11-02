// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Template-matching interface. Portable: logic above Vision depends only on this,
// so it can be tested with a fake matcher in builds without OpenCV.
#pragma once

#include <string>
#include <vector>

#include "maba/page.h"
#include "maba/types.h"

namespace maba {

struct MatchResult {
    bool found = false;
    Rect box;          // normalized 1280x720 coordinates of the best match
    Point center;      // click point: center of `box`
    double score = 0;  // best score even when not found (useful for tuning thresholds)
};

class IMatcher {
public:
    virtual ~IMatcher() = default;

    // Best match of `ref` in a 1280x720 frame, restricted to ref.roi when set.
    virtual MatchResult Find(const Frame& frame, const TemplateRef& ref) = 0;

    // Up to `max_count` non-overlapping matches, best first.
    virtual std::vector<MatchResult> FindAll(const Frame& frame, const TemplateRef& ref,
                                             int max_count) = 0;

    // Reason for the last failure that was not just "below threshold" (missing file, bad frame).
    virtual const std::string& LastError() const = 0;
};

}  // namespace maba
