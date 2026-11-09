// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Global popup handler. Popups are data (popups/*.json, format in docs/PAGE_FORMAT.md) and are
// handled here only, never inside a task. The Navigator calls Settle() around every action.
#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "maba/color.h"
#include "maba/controller.h"
#include "maba/log.h"
#include "maba/matcher.h"
#include "maba/page.h"
#include "maba/wait.h"

namespace maba {

enum class DismissKind { kClick, kBack };

struct PopupDef {
    std::string id;
    std::vector<TemplateRef> anchors;  // detected when every anchor matches,
    std::vector<ColorSpec> colors;     // every one of these colours is visible,
    std::vector<ColorSpec> no_colors;  // and none of these is (e.g. "AUTO is not lit")
    DismissKind dismiss = DismissKind::kClick;
    std::optional<TemplateRef> click_template;  // kClick: center of this match, or
    std::optional<Point> click_point;           // a fixed point
    int priority = 0;                           // higher is checked first
};

struct PopupLoadResult {
    bool ok = false;
    PopupDef popup;
    std::string error;
};

struct PopupSetLoadResult {
    bool ok = false;
    std::vector<PopupDef> popups;  // sorted by priority (desc), then id
    std::string error;
};

PopupLoadResult ParsePopup(const std::string& json_text);
// Loads every *.json in `dir`. A missing directory is an empty set (no popups known yet).
PopupSetLoadResult LoadPopupDir(const std::filesystem::path& dir);

struct PopupOptions {
    int max_dismissals = 8;  // per Settle() call; more means something is stuck
    std::chrono::milliseconds settle{600};  // pause after a dismissal before looking again
    const std::atomic<bool>* cancel = nullptr;
};

struct SettleResult {
    bool ok = false;        // no known popup left on screen
    int dismissed = 0;
    Frame frame;            // last frame looked at (no known popup on it when ok)
    std::string error;
    bool cancelled = false;
};

class PopupHandler {
public:
    PopupHandler(IMatcher& matcher, std::vector<PopupDef> popups, PopupOptions options = {},
                 LogFn log = {});

    // First known popup visible in the frame, or nullptr.
    const PopupDef* Detect(const Frame& frame);

    // Screenshot -> dismiss known popups until none is visible, at most max_dismissals times.
    // An empty capture fails immediately (the caller's wait loop retries).
    SettleResult Settle(IController& controller, IClock& clock);

    const std::vector<PopupDef>& Popups() const { return popups_; }
    void SetCancel(const std::atomic<bool>* cancel) { options_.cancel = cancel; }
    // Popups left alone (a task that works in them, like buying AP); empty = none.
    void SetIgnored(std::set<std::string> ids) { ignored_ = std::move(ids); }

private:
    bool Dismiss(IController& controller, const Frame& frame, const PopupDef& popup,
                 std::string& error);

    IMatcher& matcher_;
    std::vector<PopupDef> popups_;
    PopupOptions options_;
    LogFn log_;
    std::set<std::string> ignored_;
};

}  // namespace maba
