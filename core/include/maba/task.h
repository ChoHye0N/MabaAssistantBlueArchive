// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Task definitions loaded from tasks/*.json (format: docs/PAGE_FORMAT.md, "Tasks").
// A task is a list of declarative steps; flow is data, not C++.
#pragma once

#include <chrono>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "maba/page.h"
#include "maba/task_order.h"
#include "maba/types.h"

namespace maba {

enum class StepAction {
    kGoto,            // navigate to `page`
    kReturnHome,      // navigate to the home page
    kWaitPage,        // wait until `page` is the current page
    kClick,           // click `templ` (waits until visible) or `point`; fails if never visible
    kClickIfPresent,  // like kClick but a missing template is not an error
    kWaitTemplate,    // wait until `templ` is visible
    kBack,            // back key
    kSleep,           // fixed pause (capped)
    kReadNumber,      // OCR digits in `roi` (native-resolution crop) into `store`
};

const char* ToString(StepAction action);

inline constexpr int kMaxStepRepeat = 50;
inline constexpr int kMaxSleepMs = 10000;
inline constexpr int kMaxStepTimeoutMs = 60000;

struct Step {
    StepAction action = StepAction::kSleep;
    std::string page;                  // kGoto, kWaitPage
    std::optional<TemplateRef> templ;  // kClick, kClickIfPresent, kWaitTemplate
    std::optional<Point> point;        // kClick (fixed point)
    std::optional<Rect> roi;           // kReadNumber
    std::string store;                 // kReadNumber: result name
    int repeat = 1;                    // run the action this many times (1..kMaxStepRepeat)
    int sleep_ms = 0;                  // kSleep; also the pause after each repetition
    int timeout_ms = 5000;             // waits inside the step
    bool optional = false;             // a failure is logged, the task continues
};

struct TaskSpec {
    std::string id;
    std::string name;
    std::vector<std::string> after;
    std::string status;   // todo | wip | ready (informational)
    bool needs_ocr = false;
    std::vector<Step> steps;  // empty = not implemented yet (the runner skips it)

    TaskDef Order() const { return {id, after}; }
};

struct TaskLoadResult {
    bool ok = false;
    TaskSpec task;
    std::string error;
};

struct TaskSetLoadResult {
    bool ok = false;
    std::vector<TaskSpec> tasks;  // file-name order
    std::string error;
};

TaskLoadResult ParseTask(const std::string& json_text);
// Loads every *.json in `dir`. Fails on the first invalid file or a duplicate id.
TaskSetLoadResult LoadTaskDir(const std::filesystem::path& dir);

// Data checks against the page set: kGoto/kWaitPage pages exist, `after` ids exist.
std::vector<std::string> ValidateTasks(const std::vector<TaskSpec>& tasks,
                                       const std::map<std::string, PageDef>& pages);

}  // namespace maba
