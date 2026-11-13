// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#include "maba/update_apply.h"

#include <chrono>
#include <thread>

#include <nlohmann/json.hpp>

#include "maba/utf8.h"

namespace maba {
namespace {

namespace fs = std::filesystem;
using nlohmann::json;

bool SafeRelative(const std::string& entry) {
    if (entry.empty() || entry.find('\\') != std::string::npos || entry.find(':') != std::string::npos) return false;
    const fs::path p = PathFromUtf8(entry);
    if (p.is_absolute() || p.has_root_name() || p.has_root_directory()) return false;
    for (const auto& part : p) {
        const std::string s = PathToUtf8(part);
        if (s.empty() || s == "." || s == "..") return false;
    }
    return true;
}

// Renames `from` to `to`, trying again while the file is in use.
bool MoveWithRetry(const fs::path& from, const fs::path& to, int retries, int retry_ms, std::string& why) {
    std::error_code ec;
    for (int attempt = 0;; ++attempt) {
        fs::create_directories(to.parent_path(), ec);
        fs::rename(from, to, ec);
        if (!ec) return true;
        if (attempt >= retries) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(retry_ms));
    }
    why = PathToUtf8(from) + " -> " + PathToUtf8(to) + ": " + ec.message();
    return false;
}

}  // namespace

bool ParseUpdatePlan(const std::string& json_text, UpdatePlan& plan, std::string& error) {
    try {
        const json j = json::parse(json_text);
        plan.root = PathFromUtf8(j.at("root").get<std::string>());
        plan.staging = PathFromUtf8(j.at("staging").get<std::string>());
        plan.backup = PathFromUtf8(j.at("backup").get<std::string>());
        plan.entries = j.at("entries").get<std::vector<std::string>>();
        plan.relaunch = j.value("relaunch", std::string{});
        plan.pid = j.value("pid", 0ul);
        plan.status = PathFromUtf8(j.value("status", std::string{}));
        plan.version = j.value("version", std::string{});
    } catch (const json::exception& e) {
        error = std::string("plan: ") + e.what();
        return false;
    }
    if (plan.root.empty() || plan.staging.empty() || plan.backup.empty() || plan.entries.empty()) {
        error = "plan: root, staging, backup and entries are required";
        return false;
    }
    if (!plan.root.is_absolute() || !plan.staging.is_absolute() || !plan.backup.is_absolute()) {
        error = "plan: root, staging and backup must be absolute paths";
        return false;
    }
    for (const auto& entry : plan.entries) {
        if (!SafeRelative(entry)) {
            error = "plan: unsafe entry '" + entry + "'";
            return false;
        }
    }
    if (!plan.relaunch.empty() && !SafeRelative(plan.relaunch)) {
        error = "plan: unsafe relaunch path '" + plan.relaunch + "'";
        return false;
    }
    return true;
}

bool ApplyUpdate(const UpdatePlan& plan, std::string& error, int retries, int retry_ms) {
    std::error_code ec;
    for (const auto& entry : plan.entries) {
        if (!fs::exists(plan.staging / PathFromUtf8(entry), ec)) {
            error = "missing in the update: " + entry;
            return false;
        }
    }
    fs::remove_all(plan.backup, ec);
    fs::create_directories(plan.backup, ec);

    struct Done {
        std::string entry;
        bool had_old;
    };
    std::vector<Done> done;
    const auto undo = [&] {
        for (auto it = done.rbegin(); it != done.rend(); ++it) {
            const fs::path rel = PathFromUtf8(it->entry);
            std::string ignored;
            // The new file goes back to staging, the old one back to its place.
            MoveWithRetry(plan.root / rel, plan.staging / rel, 2, retry_ms, ignored);
            if (it->had_old) MoveWithRetry(plan.backup / rel, plan.root / rel, 2, retry_ms, ignored);
        }
    };
    for (const auto& entry : plan.entries) {
        const fs::path rel = PathFromUtf8(entry);
        const fs::path live = plan.root / rel;
        const bool had_old = fs::exists(live, ec);
        std::string why;
        if (had_old && !MoveWithRetry(live, plan.backup / rel, retries, retry_ms, why)) {
            undo();
            error = "could not move the old file away: " + why;
            return false;
        }
        if (!MoveWithRetry(plan.staging / rel, live, retries, retry_ms, why)) {
            if (had_old) {
                std::string ignored;
                MoveWithRetry(plan.backup / rel, live, 2, retry_ms, ignored);
            }
            undo();
            error = "could not move the new file in: " + why;
            return false;
        }
        done.push_back({entry, had_old});
    }
    fs::remove_all(plan.staging, ec);
    return true;
}

std::string UpdateStatusJson(bool ok, const std::string& version, const std::string& error) {
    return json{{"ok", ok}, {"version", version}, {"error", error}}.dump();
}

}  // namespace maba
