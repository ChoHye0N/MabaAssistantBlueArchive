// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Task engine: runs selected tasks in dependency order, interpreting their steps through the
// Navigator (which settles popups around every look at the screen).
#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "maba/log.h"
#include "maba/navigator.h"
#include "maba/ocr.h"
#include "maba/strings.h"
#include "maba/task.h"

namespace maba {

enum class TaskStatus { kPending, kRunning, kSucceeded, kFailed, kSkipped, kCancelled };

const char* ToString(TaskStatus status);

struct TaskRunResult {
    std::string id;
    TaskStatus status = TaskStatus::kPending;
    std::string message;
    std::map<std::string, long long> values;  // read_number results
    std::map<std::string, std::string> texts;  // read_text results
};

struct RunReport {
    bool ok = false;          // no selected task failed or was held back by a dependency
    bool cancelled = false;
    std::string error;        // set when the run could not start (bad order, unknown ids)
    std::vector<TaskRunResult> tasks;  // in execution order
};

// Checks that a click or back press changed the screen. Off by default: unit tests replay
// fixed scenes; the assistant turns it on.
struct ReactionOptions {
    bool enabled = false;
    double changed_share = 15.0;      // ChangedShare of the whole frame that counts as a reaction
    double changed_share_roi = 2.0;   // the same inside `change_roi`
    std::chrono::milliseconds interval{250};
};

struct RunnerOptions {
    std::string language = "ko";
    const std::atomic<bool>* cancel = nullptr;
    // Return to the home page before each task and after a failure (recovery).
    bool return_home_between_tasks = true;
    std::function<void(const TaskRunResult&)> on_task;  // status changes (running, finished)
    // Setting values per task id (see ResolveSettings); a task without an entry uses its defaults.
    std::map<std::string, TaskSettings> settings;
    StringTable strings;               // compared UI text of the language (click_text text_key)
    std::filesystem::path data_dir;    // where save steps write records; empty = saving fails
    ReactionOptions reaction;
    // Game day number for `each` with `rotate_setting`; the default counts days from the server
    // reset at 19:00 UTC (04:00 KST).
    std::function<long long()> game_day;
};

class TaskRunner {
public:
    // `ocr` may be null; tasks with read_number steps then fail with a clear message.
    TaskRunner(Navigator& navigator, IOcr* ocr, RunnerOptions options = {}, LogFn log = {});

    // Runs `selected` (ids) in the order ResolveOrder gives for all tasks. A selected task whose
    // dependency was also selected and failed (or was skipped for that reason) is skipped, e.g.
    // mail waits for AP spending. Tasks without steps are skipped as "not implemented"; such a
    // skip does not block dependents and does not fail the run.
    // `selected_order`: the selected tasks run in the order given (a player's profile) instead of
    // their `order`; `after` still holds, so mail stays after AP spending.
    RunReport Run(const std::vector<TaskSpec>& tasks, const std::vector<std::string>& selected,
                  bool selected_order = false);

    // Runs one task's steps (no ordering, no recovery). Exposed for tests and the CLI.
    TaskRunResult RunTask(const TaskSpec& task);

private:
    enum class Flow { kNext, kBreak, kEnd, kFail, kCancel };
    struct Context;

    Flow RunSteps(const std::vector<Step>& steps, Context& ctx);
    Flow RunAction(const Step& step, Context& ctx, std::string& error);
    bool RunStep(const Step& step, Context& ctx, std::string& error);
    bool Holds(const Condition& c, Context& ctx);
    bool ClickTemplate(const Step& s, Context& ctx, std::string& error);
    bool RunScheduleRooms(const Step& s, Context& ctx, std::string& error);
    bool CheckedAction(const Step& s, Context& ctx, const std::function<bool(std::string&)>& act,
                       const std::string& what, std::string& error);
    bool WaitReaction(const Frame& before, const Step& s);
    bool WaitGone(const TemplateRef& ref, const Step& s);
    long long GameDay() const;
    bool CheckPrecondition(const Step& s, Context& ctx, std::string& error);
    void Settle();
    bool Cancelled() const { return options_.cancel && options_.cancel->load(); }
    void Notify(const TaskRunResult& r);

    Navigator& nav_;
    IOcr* ocr_;
    RunnerOptions options_;
    LogFn log_;
};

}  // namespace maba
