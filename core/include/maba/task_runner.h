// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Task engine: runs selected tasks in dependency order, interpreting their steps through the
// Navigator (which settles popups around every look at the screen).
#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "maba/log.h"
#include "maba/navigator.h"
#include "maba/ocr.h"
#include "maba/task.h"

namespace maba {

enum class TaskStatus { kPending, kRunning, kSucceeded, kFailed, kSkipped, kCancelled };

const char* ToString(TaskStatus status);

struct TaskRunResult {
    std::string id;
    TaskStatus status = TaskStatus::kPending;
    std::string message;
    std::map<std::string, long long> values;  // read_number results
};

struct RunReport {
    bool ok = false;          // every selected task succeeded or was skipped as not implemented
    bool cancelled = false;
    std::string error;        // set when the run could not start (bad order, unknown ids)
    std::vector<TaskRunResult> tasks;  // in execution order
};

struct RunnerOptions {
    std::string language = "ko";
    const std::atomic<bool>* cancel = nullptr;
    // Return to the home page before each task and after a failure (recovery).
    bool return_home_between_tasks = true;
    std::function<void(const TaskRunResult&)> on_task;  // status changes (running, finished)
};

class TaskRunner {
public:
    // `ocr` may be null; tasks with read_number steps then fail with a clear message.
    TaskRunner(Navigator& navigator, IOcr* ocr, RunnerOptions options = {}, LogFn log = {});

    // Runs `selected` (ids) in the order ResolveOrder gives for all tasks. A selected task whose
    // dependency was also selected and failed (or was skipped for that reason) is skipped, e.g.
    // mail waits for AP spending. Tasks without steps are skipped as "not implemented"; such a
    // skip does not block dependents and does not fail the run.
    RunReport Run(const std::vector<TaskSpec>& tasks, const std::vector<std::string>& selected);

    // Runs one task's steps (no ordering, no recovery). Exposed for tests and the CLI.
    TaskRunResult RunTask(const TaskSpec& task);

private:
    bool RunStep(const Step& step, TaskRunResult& result, std::string& error);
    bool Cancelled() const { return options_.cancel && options_.cancel->load(); }
    void Notify(const TaskRunResult& r);

    Navigator& nav_;
    IOcr* ocr_;
    RunnerOptions options_;
    LogFn log_;
};

}  // namespace maba
