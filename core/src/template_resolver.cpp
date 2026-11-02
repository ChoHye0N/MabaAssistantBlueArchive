// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#include "maba/template_resolver.h"

#include <system_error>

namespace maba {

namespace fs = std::filesystem;

std::vector<fs::path> TemplateSearchDirs(const fs::path& repo_root, const std::string& lang) {
    std::vector<fs::path> dirs;
    if (!lang.empty() && lang != "common") {
        dirs.push_back(repo_root / "assets-local" / lang);
        dirs.push_back(repo_root / "assets" / lang);
    }
    dirs.push_back(repo_root / "assets-local" / "common");
    dirs.push_back(repo_root / "assets" / "common");
    return dirs;
}

fs::path ResolveTemplate(const fs::path& repo_root, const std::string& lang,
                         const std::string& name) {
    const fs::path file(name);
    if (name.empty() || file != file.filename() || name == "." || name == "..") return {};
    for (const auto& dir : TemplateSearchDirs(repo_root, lang)) {
        std::error_code ec;
        const fs::path candidate = dir / file;
        if (fs::is_regular_file(candidate, ec)) return candidate;
    }
    return {};
}

}  // namespace maba
