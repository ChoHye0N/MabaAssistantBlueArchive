// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#include "maba/task.h"

#include <map>
#include <set>

#include <nlohmann/json.hpp>

#include "json_util.h"

namespace maba {

namespace fs = std::filesystem;
using nlohmann::json;
using namespace detail;

const char* ToString(StepAction action) {
    switch (action) {
        case StepAction::kGoto: return "goto";
        case StepAction::kReturnHome: return "return_home";
        case StepAction::kWaitPage: return "wait_page";
        case StepAction::kClick: return "click";
        case StepAction::kClickIfPresent: return "click_if_present";
        case StepAction::kWaitTemplate: return "wait_template";
        case StepAction::kBack: return "back";
        case StepAction::kSleep: return "sleep";
        case StepAction::kReadNumber: return "read_number";
    }
    return "?";
}

namespace {

const std::map<std::string, StepAction>& Actions() {
    static const std::map<std::string, StepAction> actions = {
        {"goto", StepAction::kGoto},
        {"return_home", StepAction::kReturnHome},
        {"wait_page", StepAction::kWaitPage},
        {"click", StepAction::kClick},
        {"click_if_present", StepAction::kClickIfPresent},
        {"wait_template", StepAction::kWaitTemplate},
        {"back", StepAction::kBack},
        {"sleep", StepAction::kSleep},
        {"read_number", StepAction::kReadNumber},
    };
    return actions;
}

Step ParseStep(const json& obj, const std::string& where) {
    if (!obj.is_object()) throw ParseError(where + ": must be an object");
    Step s;
    const std::string name = RequireString(obj, "action", where);
    auto it = Actions().find(name);
    if (it == Actions().end()) throw ParseError(where + ": unknown action '" + name + "'");
    s.action = it->second;
    s.repeat = OptInt(obj, "repeat", where, 1, 1, kMaxStepRepeat);
    s.sleep_ms = OptInt(obj, "sleep_ms", where, 0, 0, kMaxSleepMs);
    s.timeout_ms = OptInt(obj, "timeout_ms", where, 5000, 100, kMaxStepTimeoutMs);
    s.optional = OptBool(obj, "optional", where, false);

    switch (s.action) {
        case StepAction::kGoto:
        case StepAction::kWaitPage:
            s.page = RequireString(obj, "page", where);
            break;
        case StepAction::kClick:
            if (obj.contains("template")) {
                s.templ = ParseTemplateRef(obj, where);
            } else if (obj.contains("x") || obj.contains("y")) {
                s.point = ParsePoint(obj, where);
            } else {
                throw ParseError(where + ": click needs 'template' or 'x'/'y'");
            }
            break;
        case StepAction::kClickIfPresent:
        case StepAction::kWaitTemplate:
            s.templ = ParseTemplateRef(obj, where);
            break;
        case StepAction::kSleep:
            s.sleep_ms = RequireInt(obj, "sleep_ms", where);
            if (s.sleep_ms < 1 || s.sleep_ms > kMaxSleepMs)
                throw ParseError(where + ": 'sleep_ms' must be in [1, " +
                                 std::to_string(kMaxSleepMs) + "]");
            break;
        case StepAction::kReadNumber:
            s.roi = ParseRoi(obj, where);
            if (!s.roi) throw ParseError(where + ": read_number needs a non-empty 'roi'");
            s.store = RequireString(obj, "store", where);
            break;
        case StepAction::kReturnHome:
        case StepAction::kBack:
            break;
    }
    return s;
}

TaskSpec ParseTaskJson(const json& root) {
    if (!root.is_object()) throw ParseError("task: root must be an object");
    TaskSpec t;
    t.id = RequireString(root, "id", "task");
    const std::string where = "task '" + t.id + "'";
    t.name = OptString(root, "name", where, t.id);
    t.after = OptStringArray(root, "after", where);
    t.status = OptString(root, "status", where, "todo");
    t.needs_ocr = OptBool(root, "needs_ocr", where, false);
    if (auto it = root.find("steps"); it != root.end() && !it->is_null()) {
        if (!it->is_array()) throw ParseError(where + ": 'steps' must be an array");
        for (std::size_t i = 0; i < it->size(); ++i)
            t.steps.push_back(ParseStep((*it)[i], where + ".steps[" + std::to_string(i) + "]"));
    }
    return t;
}

}  // namespace

TaskLoadResult ParseTask(const std::string& json_text) {
    TaskLoadResult result;
    try {
        result.task = ParseTaskJson(json::parse(json_text));
        result.ok = true;
    } catch (const json::exception& e) {
        result.error = std::string("invalid JSON: ") + e.what();
    } catch (const ParseError& e) {
        result.error = e.what();
    }
    return result;
}

TaskSetLoadResult LoadTaskDir(const fs::path& dir) {
    TaskSetLoadResult result;
    std::set<std::string> ids;
    try {
        for (const auto& f : JsonFilesIn(dir)) {
            TaskLoadResult t = ParseTask(ReadFile(f));
            if (!t.ok) {
                result.error = f.filename().generic_string() + ": " + t.error;
                return result;
            }
            if (!ids.insert(t.task.id).second) {
                result.error = "duplicate task id: " + t.task.id;
                return result;
            }
            result.tasks.push_back(std::move(t.task));
        }
    } catch (const ParseError& e) {
        result.error = e.what();
        return result;
    }
    result.ok = true;
    return result;
}

std::vector<std::string> ValidateTasks(const std::vector<TaskSpec>& tasks,
                                       const std::map<std::string, PageDef>& pages) {
    std::vector<std::string> problems;
    std::set<std::string> ids;
    for (const auto& t : tasks) ids.insert(t.id);
    for (const auto& t : tasks) {
        for (const auto& dep : t.after) {
            if (!ids.count(dep))
                problems.push_back("task '" + t.id + "' runs after unknown task '" + dep + "'");
        }
        for (std::size_t i = 0; i < t.steps.size(); ++i) {
            const Step& s = t.steps[i];
            if ((s.action == StepAction::kGoto || s.action == StepAction::kWaitPage) &&
                !pages.count(s.page)) {
                problems.push_back("task '" + t.id + "' step " + std::to_string(i) +
                                   " uses unknown page '" + s.page + "'");
            }
        }
    }
    return problems;
}

}  // namespace maba
