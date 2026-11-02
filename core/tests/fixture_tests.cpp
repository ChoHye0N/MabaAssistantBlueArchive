// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Page detection on real game screenshots, which are never committed (NEXON's images, possibly
// personal data). Put 1280x720 screenshots in assets-local/fixtures/<lang>/<page_id>__<any>.png;
// each must be detected as <page_id> and as no other page. Skipped (exit 77) when there are none.
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "maba/page.h"
#include "maba/smoke.h"
#include "maba/vision.h"

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    const fs::path root = argc > 1 ? fs::path(argv[1]) : fs::path(MABA_SOURCE_DIR);
    const fs::path fixtures = root / "assets-local" / "fixtures";
    std::error_code ec;
    if (!fs::is_directory(fixtures, ec)) {
        std::printf("no fixtures in %s: skipped\n", fixtures.generic_string().c_str());
        return 77;
    }
    const auto pages = maba::LoadPageDir(root / "pages");
    if (!pages.ok) {
        std::printf("FAIL cannot load pages: %s\n", pages.error.c_str());
        return 1;
    }

    int checked = 0;
    int failures = 0;
    for (fs::directory_iterator lang_it(fixtures, ec), end; !ec && lang_it != end; lang_it.increment(ec)) {
        if (!lang_it->is_directory()) continue;
        const std::string lang = lang_it->path().filename().string();
        maba::OpenCvMatcher matcher(root, lang);
        for (fs::directory_iterator it(lang_it->path(), ec); !ec && it != end; it.increment(ec)) {
            const fs::path file = it->path();
            const std::string name = file.stem().string();
            const auto sep = name.find("__");
            if (file.extension() != ".png" || sep == std::string::npos) continue;
            const std::string expected = name.substr(0, sep);
            const auto frame = maba::LoadPng(file);
            if (!frame || frame->width != maba::kNormWidth || frame->height != maba::kNormHeight) {
                std::printf("FAIL %s/%s: not a 1280x720 PNG\n", lang.c_str(), name.c_str());
                ++failures;
                continue;
            }
            for (const auto& [id, page] : pages.pages) {
                const auto d = maba::DetectPage(matcher, *frame, page);
                const bool want = id == expected;
                if (d.detected != want) {
                    std::printf("FAIL %s/%s: page '%s' %s (score %.3f%s%s)\n", lang.c_str(),
                                name.c_str(), id.c_str(), want ? "not detected" : "falsely detected",
                                d.min_score, matcher.LastError().empty() ? "" : ", ",
                                matcher.LastError().c_str());
                    ++failures;
                }
            }
            ++checked;
        }
    }
    if (checked == 0) {
        std::printf("no fixtures matched <page_id>__<name>.png: skipped\n");
        return 77;
    }
    std::printf("%d fixture(s) checked, %d failure(s)\n", checked, failures);
    return failures == 0 ? 0 : 1;
}
