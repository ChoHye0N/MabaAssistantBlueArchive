// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Page definitions loaded from pages/*.json (format: docs/PAGE_FORMAT.md).
// Fields read: id, anchors, transitions, rois. Other fields (notes, strings, ...) are ignored.
#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "maba/types.h"

namespace maba {

inline constexpr double kDefaultThreshold = 0.9;

// A template to look for, optionally restricted to a region of the 1280x720 frame.
struct TemplateRef {
    std::string file;          // file name, resolved with ResolveTemplate
    std::optional<Rect> roi;   // normalized; absent = whole frame
    double threshold = kDefaultThreshold;
};

struct Transition {
    std::string to;                           // target page id
    std::optional<TemplateRef> click_template;  // click the center of this match, or
    std::optional<Point> click_point;           // a fixed normalized point
};

struct PageDef {
    std::string id;
    std::vector<TemplateRef> anchors;  // the page is detected when every anchor matches
    std::vector<Transition> transitions;
    std::map<std::string, Rect> rois;  // named regions (e.g. OCR targets); unset ones are skipped

    const Transition* FindTransition(const std::string& to) const;
};

struct PageLoadResult {
    bool ok = false;
    PageDef page;
    std::string error;
};

struct PageSetLoadResult {
    bool ok = false;
    std::map<std::string, PageDef> pages;  // by id
    std::string error;
};

PageLoadResult ParsePage(const std::string& json_text);
PageLoadResult LoadPageFile(const std::filesystem::path& path);

// Loads every *.json in `dir`. Fails on the first invalid file or a duplicate id.
PageSetLoadResult LoadPageDir(const std::filesystem::path& dir);

}  // namespace maba
