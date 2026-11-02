// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// config/languages.json: UI languages of the game client and their OCR model codes.
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace maba {

struct Language {
    std::string code;      // "ko", "en", ...
    std::string name;      // display name
    std::string status;    // "confirmed" | "unverified"
    std::string ocr_lang;  // recognition model code passed to the OCR engine ("korean", "en", ...)
};

struct LanguageConfig {
    std::string primary = "ko";
    std::vector<Language> languages;

    const Language* Find(const std::string& code) const;
};

struct LanguageLoadResult {
    bool ok = false;
    LanguageConfig config;
    std::string error;
};

LanguageLoadResult ParseLanguages(const std::string& json_text);
LanguageLoadResult LoadLanguages(const std::filesystem::path& file);

}  // namespace maba
