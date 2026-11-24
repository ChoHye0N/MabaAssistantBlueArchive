// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Keeps folders that grow on every run (failure frames) from filling the disk.
#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

namespace maba {

// Deletes the oldest regular files in `dir` whose names start with `prefix` and end with `suffix`,
// so that at most `keep` of them remain. Names carry a sortable timestamp, so the order is by name.
// Other files are never touched. Returns the number of files deleted; errors are skipped.
std::size_t PruneOldFiles(const std::filesystem::path& dir, const std::string& prefix,
                          const std::string& suffix, std::size_t keep);

// Failure frames kept in captures/ (one is about 1 MB).
inline constexpr std::size_t kMaxFailureFrames = 100;

}  // namespace maba
