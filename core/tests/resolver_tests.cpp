// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#include <filesystem>
#include <fstream>
#include <random>
#include <string>

#include "maba/template_resolver.h"
#include "test_framework.h"

namespace {

namespace fs = std::filesystem;

struct TempDir {
    TempDir() {
        std::random_device rd;
        path = fs::temp_directory_path() / ("maba_resolver_" + std::to_string(rd()));
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
    void Touch(const fs::path& rel) const {
        fs::create_directories((path / rel).parent_path());
        std::ofstream(path / rel) << "x";
    }
    fs::path path;
};

MABA_TEST(ResolverPriorityOrder) {
    TempDir root;
    const std::string name = "main_menu_mail_icon.png";
    CHECK(maba::ResolveTemplate(root.path, "ko", name).empty());

    root.Touch(fs::path("assets") / "common" / name);
    CHECK(maba::ResolveTemplate(root.path, "ko", name) == root.path / "assets" / "common" / name);

    root.Touch(fs::path("assets-local") / "common" / name);
    CHECK(maba::ResolveTemplate(root.path, "ko", name) ==
          root.path / "assets-local" / "common" / name);

    root.Touch(fs::path("assets") / "ko" / name);
    CHECK(maba::ResolveTemplate(root.path, "ko", name) == root.path / "assets" / "ko" / name);

    root.Touch(fs::path("assets-local") / "ko" / name);
    CHECK(maba::ResolveTemplate(root.path, "ko", name) ==
          root.path / "assets-local" / "ko" / name);

    // Another language does not see ko files; it falls back to common only.
    CHECK(maba::ResolveTemplate(root.path, "en", name) ==
          root.path / "assets-local" / "common" / name);
}

MABA_TEST(ResolverRejectsPaths) {
    TempDir root;
    root.Touch(fs::path("assets") / "common" / "a.png");
    CHECK(maba::ResolveTemplate(root.path, "ko", "../common/a.png").empty());
    CHECK(maba::ResolveTemplate(root.path, "ko", "common/a.png").empty());
    CHECK(maba::ResolveTemplate(root.path, "ko", "").empty());
    CHECK(maba::ResolveTemplate(root.path, "ko", "..").empty());
}

MABA_TEST(ResolverSearchDirsForCommon) {
    const auto dirs = maba::TemplateSearchDirs("r", "common");
    CHECK(dirs.size() == 2);
    CHECK(maba::TemplateSearchDirs("r", "ko").size() == 4);
}

}  // namespace
