// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#include "maba/task_order.h"

#include <algorithm>
#include <unordered_set>

namespace maba {

OrderResult ResolveOrder(const std::vector<TaskDef>& tasks) {
    OrderResult result;

    std::unordered_set<std::string> ids;
    for (const auto& t : tasks) {
        if (!ids.insert(t.id).second) {
            result.error = "duplicate task id: " + t.id;
            return result;
        }
    }
    for (const auto& t : tasks) {
        for (const auto& dep : t.after) {
            if (ids.count(dep) == 0) {
                result.error = "task '" + t.id + "' depends on unknown task '" + dep + "'";
                return result;
            }
        }
    }

    std::unordered_set<std::string> done;
    std::vector<bool> emitted(tasks.size(), false);

    while (result.order.size() < tasks.size()) {
        bool progressed = false;
        for (std::size_t i = 0; i < tasks.size(); ++i) {
            if (emitted[i]) continue;
            const bool ready = std::all_of(
                tasks[i].after.begin(), tasks[i].after.end(),
                [&](const std::string& dep) { return done.count(dep) > 0; });
            if (!ready) continue;
            emitted[i] = true;
            done.insert(tasks[i].id);
            result.order.push_back(tasks[i].id);
            progressed = true;
            break;  // restart the scan so input order is preserved among ready tasks
        }
        if (!progressed) {
            result.order.clear();
            result.error = "dependency cycle detected";
            return result;
        }
    }

    result.ok = true;
    return result;
}

}  // namespace maba
