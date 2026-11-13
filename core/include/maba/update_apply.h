// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
//
// Replacing the program's files with the ones of an unpacked update (docs/notes/updates.md).
// Portable and free of other Core code, so the updater stays a small program of its own.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace maba {

// What the program hands to the updater, as JSON in a plan file:
//   {"root": "...", "staging": "...", "backup": "...", "entries": ["app.exe", "bin", "data/tools"],
//    "relaunch": "app.exe", "pid": 1234, "status": "...", "version": "0.1.5"}
// `entries` are files or folders relative to the root, each present in `staging`.
struct UpdatePlan {
    std::filesystem::path root;     // the installed program
    std::filesystem::path staging;  // the unpacked new files, same layout as the root
    std::filesystem::path backup;   // receives what is replaced
    std::vector<std::string> entries;
    std::string relaunch;           // executable under the root to start afterwards ("" = none)
    unsigned long pid = 0;          // the program to wait for (0 = none)
    std::filesystem::path status;   // result note for the program's next start
    std::string version;
};

// Reads a plan. Every entry has to be a relative path without "." or ".." parts, and no entry
// may be the root itself; `error` says what is wrong.
bool ParseUpdatePlan(const std::string& json_text, UpdatePlan& plan, std::string& error);

// Moves every entry of the plan from staging into the root; what was there goes to the backup
// folder. When one entry fails, the entries already moved are put back, so the installation is
// either the old one or the new one. A file that is still in use is tried again `retries` times,
// `retry_ms` apart (a program that has just exited keeps its files locked for a moment).
bool ApplyUpdate(const UpdatePlan& plan, std::string& error, int retries = 20, int retry_ms = 500);

// The status note: {"ok": true|false, "version": "...", "error": "..."}.
std::string UpdateStatusJson(bool ok, const std::string& version, const std::string& error);

}  // namespace maba
