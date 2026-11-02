// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#pragma once

#include <string>
#include <vector>

namespace maba {

struct TaskDef {
    std::string id;
    std::vector<std::string> after;  // ids that must run before this task
};

struct OrderResult {
    bool ok = false;
    std::vector<std::string> order;  // execution order when ok
    std::string error;               // reason when !ok
};

// Dependency-respecting order. Among ready tasks the input order is preserved.
// Fails on duplicate ids, unknown dependencies and cycles (including self-dependency).
OrderResult ResolveOrder(const std::vector<TaskDef>& tasks);

}  // namespace maba
