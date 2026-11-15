// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Anchor checks shared by page detection, popup detection and the Navigator.
#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
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

struct LanguageGuess {
    std::string language;  // the other game language the screen looks like
    std::string page;      // the page its templates detect
};

// The game language set in the program does not match the game: the matcher of the set language
// sees no page on `frame`, while the matcher of another language detects one through a template
// of that language (`own_template(language, file)`; a page found only through templates shared by
// every language proves nothing). `others` are tried in order. Empty when no other language fits.
std::optional<LanguageGuess> GuessGameLanguage(
    IMatcher& current, const std::vector<std::pair<std::string, IMatcher*>>& others, const Frame& frame,
    const std::map<std::string, PageDef>& pages,
    const std::function<bool(const std::string& language, const std::string& file)>& own_template);

}  // namespace maba
