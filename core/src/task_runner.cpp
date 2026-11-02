// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#include "maba/task_runner.h"

#include <set>

#include "maba/ocr_text.h"

namespace maba {

using std::chrono::milliseconds;

const char* ToString(TaskStatus status) {
    switch (status) {
        case TaskStatus::kPending: return "pending";
        case TaskStatus::kRunning: return "running";
        case TaskStatus::kSucceeded: return "succeeded";
        case TaskStatus::kFailed: return "failed";
        case TaskStatus::kSkipped: return "skipped";
        case TaskStatus::kCancelled: return "cancelled";
    }
    return "?";
}

TaskRunner::TaskRunner(Navigator& navigator, IOcr* ocr, RunnerOptions options, LogFn log)
    : nav_(navigator), ocr_(ocr), options_(std::move(options)), log_(std::move(log)) {}

void TaskRunner::Notify(const TaskRunResult& r) {
    if (options_.on_task) options_.on_task(r);
}

namespace {

WaitOptions StepWait(const Step& s, const std::atomic<bool>* cancel) {
    WaitOptions w;
    w.timeout = milliseconds{s.timeout_ms};
    w.interval = milliseconds{300};
    w.max_attempts = s.timeout_ms / 100 + 2;
    w.cancel = cancel;
    return w;
}

}  // namespace

bool TaskRunner::RunStep(const Step& s, TaskRunResult& result, std::string& error) {
    IController& controller = nav_.Controller();
    switch (s.action) {
        case StepAction::kGoto: {
            const NavResult r = nav_.GoTo(s.page);
            if (!r.ok) error = r.error;
            return r.ok;
        }
        case StepAction::kReturnHome: {
            const NavResult r = nav_.ReturnHome();
            if (!r.ok) error = r.error;
            return r.ok;
        }
        case StepAction::kWaitPage: {
            bool reached = false;
            nav_.ObserveUntil([&](const Observation& o) { return o.page == s.page; },
                              StepWait(s, options_.cancel), reached);
            if (!reached) error = "page '" + s.page + "' did not appear";
            return reached;
        }
        case StepAction::kClick:
        case StepAction::kClickIfPresent: {
            Point target;
            if (s.templ) {
                const auto m = nav_.WaitForTemplate(*s.templ, StepWait(s, options_.cancel));
                if (!m) {
                    if (s.action == StepAction::kClickIfPresent) {
                        Log(log_, LogLevel::kDebug, s.templ->file + " not present, skipped");
                        return true;
                    }
                    error = s.templ->file + " not found";
                    return false;
                }
                target = m->center;
            } else {
                target = *s.point;
            }
            if (!controller.Click(target)) {
                error = "click failed";
                return false;
            }
            return true;
        }
        case StepAction::kWaitTemplate: {
            const auto m = nav_.WaitForTemplate(*s.templ, StepWait(s, options_.cancel));
            if (!m) error = s.templ->file + " did not appear";
            return m.has_value();
        }
        case StepAction::kBack:
            if (!controller.Back()) {
                error = "back failed";
                return false;
            }
            return true;
        case StepAction::kSleep:
            nav_.Clock().Sleep(milliseconds{s.sleep_ms});
            return true;
        case StepAction::kReadNumber: {
            if (!ocr_) {
                error = "OCR is not available (start the OCR sidecar)";
                return false;
            }
            const Frame crop = controller.CaptureRegion(*s.roi);
            if (crop.empty()) {
                error = "region capture failed";
                return false;
            }
            const auto pieces = ocr_->Recognize(crop, Rect{}, options_.language);
            const std::string text = JoinLeftToRight(pieces);
            long long value = 0;
            if (const auto frac = ParseFraction(text)) {
                value = frac->value;
                result.values[s.store + "_max"] = frac->max;
            } else if (const auto n = ParseInteger(text)) {
                value = *n;
            } else {
                error = "could not read a number from '" + text + "'";
                if (!ocr_->LastError().empty()) error += ": " + ocr_->LastError();
                return false;
            }
            result.values[s.store] = value;
            Log(log_, LogLevel::kInfo, s.store + " = " + std::to_string(value));
            return true;
        }
    }
    error = "unknown action";
    return false;
}

TaskRunResult TaskRunner::RunTask(const TaskSpec& task) {
    TaskRunResult result;
    result.id = task.id;
    if (task.steps.empty()) {
        result.status = TaskStatus::kSkipped;
        result.message = "not implemented yet";
        return result;
    }
    for (std::size_t i = 0; i < task.steps.size(); ++i) {
        const Step& s = task.steps[i];
        for (int rep = 0; rep < s.repeat; ++rep) {
            if (Cancelled()) {
                result.status = TaskStatus::kCancelled;
                result.message = "cancelled";
                return result;
            }
            std::string error;
            const bool ok = RunStep(s, result, error);
            if (Cancelled()) {
                result.status = TaskStatus::kCancelled;
                result.message = "cancelled";
                return result;
            }
            if (!ok) {
                const std::string where = "step " + std::to_string(i + 1) + " (" +
                                          ToString(s.action) + "): " + error;
                if (s.optional) {
                    Log(log_, LogLevel::kInfo, task.id + ": optional " + where);
                    break;  // stop repeating this step, continue with the next one
                }
                result.status = TaskStatus::kFailed;
                result.message = where;
                return result;
            }
            if (s.action != StepAction::kSleep && s.sleep_ms > 0)
                nav_.Clock().Sleep(milliseconds{s.sleep_ms});
        }
    }
    result.status = TaskStatus::kSucceeded;
    return result;
}

RunReport TaskRunner::Run(const std::vector<TaskSpec>& tasks,
                          const std::vector<std::string>& selected) {
    RunReport report;
    std::vector<TaskDef> defs;
    std::map<std::string, const TaskSpec*> by_id;
    for (const auto& t : tasks) {
        defs.push_back(t.Order());
        by_id[t.id] = &t;
    }
    const OrderResult order = ResolveOrder(defs);
    if (!order.ok) {
        report.error = order.error;
        return report;
    }
    const std::set<std::string> wanted(selected.begin(), selected.end());
    for (const auto& id : wanted) {
        if (!by_id.count(id)) {
            report.error = "unknown task: " + id;
            return report;
        }
    }

    // Selected tasks that ran and did not pass. "Not implemented" skips do not block dependents:
    // they did nothing, so the game state is as if they were not selected.
    std::set<std::string> blocked;
    report.ok = true;
    for (const auto& id : order.order) {
        if (!wanted.count(id)) continue;
        const TaskSpec& task = *by_id.at(id);
        TaskRunResult r;
        r.id = id;

        if (report.cancelled || Cancelled()) {
            report.cancelled = true;
            r.status = TaskStatus::kCancelled;
            r.message = "cancelled";
        } else {
            // A selected dependency that did not succeed blocks this task.
            for (const auto& dep : task.after) {
                if (blocked.count(dep)) {
                    r.status = TaskStatus::kSkipped;
                    r.message = "dependency '" + dep + "' did not succeed";
                    break;
                }
            }
        }

        if (r.status == TaskStatus::kPending && task.steps.empty()) {
            r = RunTask(task);  // skipped: not implemented
        } else if (r.status == TaskStatus::kPending) {
            r.status = TaskStatus::kRunning;
            Notify(r);
            Log(log_, LogLevel::kInfo, "task '" + id + "' started");
            if (options_.return_home_between_tasks && !task.steps.empty()) {
                const NavResult home = nav_.ReturnHome();
                if (!home.ok) {
                    r.status = home.cancelled ? TaskStatus::kCancelled : TaskStatus::kFailed;
                    r.message = "could not return home: " + home.error;
                }
            }
            if (r.status == TaskStatus::kRunning) r = RunTask(task);
            if (r.status == TaskStatus::kFailed && options_.return_home_between_tasks &&
                !Cancelled()) {
                const NavResult home = nav_.ReturnHome();  // recovery for the next task
                if (!home.ok) Log(log_, LogLevel::kWarn, "recovery failed: " + home.error);
            }
        }

        if (r.status == TaskStatus::kCancelled) report.cancelled = true;
        // "not implemented" skips do not fail the run; dependency skips and failures do.
        const bool passed = r.status == TaskStatus::kSucceeded ||
                            (r.status == TaskStatus::kSkipped && task.steps.empty());
        if (!passed) {
            report.ok = false;
            blocked.insert(id);
        }
        Log(log_, r.status == TaskStatus::kFailed ? LogLevel::kWarn : LogLevel::kInfo,
            "task '" + id + "' " + ToString(r.status) + (r.message.empty() ? "" : ": " + r.message));
        Notify(r);
        report.tasks.push_back(std::move(r));
    }
    return report;
}

}  // namespace maba
