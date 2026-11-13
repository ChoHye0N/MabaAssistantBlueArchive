// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Page definitions loaded from pages/*.json (format: docs/PAGE_FORMAT.md).
// Fields read: id, anchors, transitions, rois, enter, wait_ms. Other fields (notes, strings, ...) are ignored.
#pragma once

#include <chrono>
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

// Start-up screens (title, event splash, loading) have no stable anchors: tap `tap` until a known
// page is visible. Read on the home page only.
// A start-up screen recognized by a template, tapped at its own point instead of the default
// (the title screen: its image notices close with "do not show today").
struct EnterTap {
    TemplateRef when;
    Point tap;
};

struct EnterSpec {
    Point tap;
    std::vector<EnterTap> screens;  // checked in order; the first match picks the point
    int max_taps = 30;
    std::chrono::milliseconds interval{2000};  // pause after each tap
};

struct PageDef {
    std::string id;
    std::vector<TemplateRef> anchors;  // the page is detected when every anchor matches
    std::vector<Transition> transitions;
    std::map<std::string, Rect> rois;  // named regions (e.g. OCR targets); unset ones are skipped
    std::optional<EnterSpec> enter;
    // Above 0: the screen ends by itself (a battle). Navigation waits this long and never presses
    // back here, because back opens the pause menu and can give the battle up.
    std::chrono::milliseconds wait{0};

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
