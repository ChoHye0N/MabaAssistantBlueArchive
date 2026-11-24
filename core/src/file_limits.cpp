// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#include "maba/file_limits.h"

#include <algorithm>
#include <system_error>
#include <vector>

#include "maba/utf8.h"

namespace maba {

namespace fs = std::filesystem;

std::size_t PruneOldFiles(const fs::path& dir, const std::string& prefix, const std::string& suffix,
                          std::size_t keep) {
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return 0;
    std::vector<std::pair<std::string, fs::path>> files;
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        const std::string name = PathToUtf8(it->path().filename());
        if (name.size() < prefix.size() + suffix.size()) continue;
        if (name.compare(0, prefix.size(), prefix) != 0) continue;
        if (name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0) continue;
        files.emplace_back(name, it->path());
    }
    if (files.size() <= keep) return 0;
    std::sort(files.begin(), files.end());
    std::size_t removed = 0;
    for (std::size_t i = 0; i + keep < files.size(); ++i) {
        if (fs::remove(files[i].second, ec)) ++removed;
    }
    return removed;
}

}  // namespace maba
