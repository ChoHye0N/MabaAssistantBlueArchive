// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace maba {

// Directories searched for a template, in priority order (see docs/PAGE_FORMAT.md):
// assets-local/<lang>, assets/<lang>, assets-local/common, assets/common.
std::vector<std::filesystem::path> TemplateSearchDirs(const std::filesystem::path& repo_root,
                                                      const std::string& lang);

// First existing file named `name` in TemplateSearchDirs. Empty path if none exists
// or if `name` is not a plain file name (no directories, no "..").
std::filesystem::path ResolveTemplate(const std::filesystem::path& repo_root,
                                      const std::string& lang, const std::string& name);

}  // namespace maba
