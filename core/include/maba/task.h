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

#include "maba/color.h"
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
    kWaitStable,      // wait until the screen stops moving (bounded, a screen that never rests is not an error)
    kReadText,        // OCR on a region, the text is stored for `save`
    kReadNumber,      // OCR digits in `roi` (native-resolution crop) into `store`
    kClickColor,      // click colour blobs one by one (re-detected each time), up to `max`
    kClickText,       // OCR `roi`, click the first wanted text found; scroll with `point`->`to`
    kSave,            // write stored values to a JSON record under the data dir
    kSwipe,           // drag from `point` to `to`
    kWheel,           // mouse wheel at `point`, `notches` steps
    kIf,              // run `steps` when `when` holds, else `else_steps`
    kLoop,            // run `steps` up to `max` times; stops early when `until` holds
    kEach,            // for every value of a list setting: its `cases` entry, then `steps`
    kEnd,             // finish the task now (succeeded or skipped)
    kFail,            // fail the task with `message`
    kBreak,           // leave the innermost loop or each
};

const char* ToString(StepAction action);

inline constexpr int kMaxStepRepeat = 50;
inline constexpr int kMaxSleepMs = 10000;
inline constexpr int kMaxStepTimeoutMs = 60000;
inline constexpr int kMaxLoop = 50;
inline constexpr int kMaxClickRetries = 5;
inline constexpr int kMaxStepDepth = 8;

// Task settings: declared per task with a type and a default; a run may override them.
enum class SettingType { kBool, kInt, kChoice, kList };

// Value as text: bool "true"/"false", int in decimal, choice as is; lists use `list`.
struct SettingValue {
    std::string scalar;
    std::vector<std::string> list;
    bool operator==(const SettingValue&) const = default;
};

struct SettingDef {
    std::string key;
    SettingType type = SettingType::kBool;
    SettingValue default_value;
    int min = 0, max = 0;              // kInt
    std::vector<std::string> choices;  // kChoice, and allowed list items when non-empty
    std::string description;
    std::string label_key;             // choice labels: strings[<label_key> + choice]
    std::string ui;                    // GUI hint: "checklist" for a list of check boxes
};

using TaskSettings = std::map<std::string, SettingValue>;

enum class ConditionKind { kSetting, kPresent, kAbsent, kColor, kNoColor, kValue };
enum class CompareOp { kEquals, kNotEquals, kContains, kLess, kLessEqual, kGreater, kGreaterEqual };

struct Condition {
    ConditionKind kind = ConditionKind::kSetting;
    std::string key;                   // setting key (kSetting) or stored value name (kValue)
    CompareOp op = CompareOp::kEquals;
    std::string text;                  // kSetting: compared value as text
    long long number = 0;              // kValue: compared number
    std::optional<TemplateRef> templ;  // kPresent, kAbsent
    std::optional<ColorSpec> color;    // kColor, kNoColor
    std::vector<Rect> rois_by_value;   // kColor, kNoColor: the area for the each value (1 = first)
    int timeout_ms = 0;                // kPresent/kColor: keep looking this long (0 = one look)
};

// Which match a template click uses when several are visible.
enum class Pick { kBest, kTop, kBottom, kLeft, kRight };

struct Step {
    StepAction action = StepAction::kSleep;
    std::string page;                  // kGoto, kWaitPage
    std::optional<TemplateRef> templ;  // kClick, kClickIfPresent, kWaitTemplate; "{value}" in the
                                       // file name is replaced inside kEach
    std::optional<Point> point;        // kClick (fixed point), kSwipe (start)
    std::vector<Point> points;         // kClick: chosen by the innermost loop iteration (cycled)
    bool points_by_value = false;      // kClick: chosen by the innermost each value instead (1 = first)
    Point offset;                      // kClick, kClickColor, kClickText: added to the target
    std::optional<Point> retry_offset; // kClickColor: second try on a blob that stayed
    std::vector<std::string> texts;    // kClickText: wanted texts, first found wins
    std::string text_setting;          // kClickText: or take them from a list or choice setting
    std::optional<int> click_x;        // kClickText: click at this x instead (row buttons)
    std::string text_key;              // kClickText: look for strings[<key>] ("{value}" filled in)
    std::string file;                  // kSave: record path under the data dir ("{name}" = value)
    std::vector<std::string> values;   // kSave: stored numbers to write
    bool history = false;              // kSave: also append the numbers with a timestamp
    int max_scrolls = 0;               // kClickText: pages to scroll through
    Pick pick = Pick::kBest;           // kClick, kClickIfPresent
    std::optional<TemplateRef> expect; // after a click: wait for it, click again if it is missing
    std::optional<TemplateRef> expect_gone;  // after a click: wait until it left, or click again
    int retries = 2;                   // extra clicks when the screen did not react
    bool no_verify = false;            // kClick, kClickIfPresent, kBack: no reaction check
    std::optional<Rect> change_roi;    // reaction check: look only here (small changes)
    std::string on_page;               // kClick, kClickIfPresent: fail unless this page is shown
    std::optional<TemplateRef> require;  // kClick, kClickIfPresent: fail unless this is visible
    std::optional<ColorSpec> color;    // kClickColor
    Point to;                          // kSwipe end, kClickText scroll end
    int duration_ms = 400;             // kSwipe
    int notches = 0;                   // kWheel: negative turns it towards the user
    std::optional<Rect> roi;           // kReadNumber, kClickText
    std::string store;                 // kReadNumber: result name
    bool lenient = false;              // kReadNumber: read the number at the end of the text
                                       // (a label such as 'tickets 5/5' in front is dropped)
    int repeat = 1;                    // run the action this many times (1..kMaxStepRepeat)
    int sleep_ms = 0;                  // kSleep; also the pause after each repetition
    int timeout_ms = 5000;             // waits inside the step
    bool optional = false;             // a failure is reported in the result, the task continues
    bool quiet = false;                // optional: failing here is normal, nothing to report
    bool fuzzy = false;                // kClickText: a text of 4+ characters may differ in one
    std::optional<Condition> when;     // the step runs only when this holds (kIf: the branch test)
    std::optional<Condition> until;    // kLoop: checked before each iteration, stops when it holds
    int max = 1;                       // kLoop iterations, kClickColor clicks
    std::string max_setting;           // kLoop: take `max` from an int setting
    std::string max_value;             // kLoop: take `max` from a stored value (read_number)
    int max_adjust = 0;                // kLoop: added to the count (a stepper that starts at 1: -1)
    std::string setting;               // kEach: list setting
    std::vector<Step> steps;           // kIf (then), kLoop and kEach bodies
    std::vector<Step> else_steps;      // kIf
    std::vector<std::pair<std::string, std::vector<Step>>> cases;  // kEach: per value
    std::string message;               // kEnd, kFail
    bool end_skipped = false;          // kEnd: report the task as skipped
};

inline constexpr int kDefaultTaskOrder = 1000;

struct TaskSpec {
    std::string id;
    std::string name;
    std::vector<std::string> after;  // must run first; a failed one blocks this task
    int order = kDefaultTaskOrder;   // run order among tasks that do not depend on each other
    std::string status;   // todo | wip | ready (informational)
    bool needs_ocr = false;
    std::vector<SettingDef> settings;
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
// Loads every *.json in `dir`, sorted by SortTasksByOrder. Fails on the first invalid file or a
// duplicate id.
TaskSetLoadResult LoadTaskDir(const std::filesystem::path& dir);

// Stable sort by `order`: ResolveOrder keeps this order among tasks whose dependencies are met,
// so `order` sets the run order without the blocking that `after` implies.
void SortTasksByOrder(std::vector<TaskSpec>& tasks);

// Default values of every declared setting.
TaskSettings DefaultSettings(const TaskSpec& task);

// Overrides from JSON `{"<task id>": {"<key>": value, ...}, ...}` applied on top of the defaults.
// Unknown tasks or keys and values of the wrong type are errors. An empty text means no overrides.
bool ResolveSettings(const std::vector<TaskSpec>& tasks, const std::string& overrides_json,
                     std::map<std::string, TaskSettings>& out, std::string& error);

// Data checks against the page set: kGoto/kWaitPage pages exist, `after` ids exist, settings
// used by steps are declared.
std::vector<std::string> ValidateTasks(const std::vector<TaskSpec>& tasks,
                                       const std::map<std::string, PageDef>& pages);

}  // namespace maba
