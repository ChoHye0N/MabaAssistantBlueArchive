// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#include "maba/languages.h"

#include <set>

#include <nlohmann/json.hpp>

#include "json_util.h"

namespace maba {

using nlohmann::json;
using namespace detail;

const Language* LanguageConfig::Find(const std::string& code) const {
    for (const auto& l : languages)
        if (l.code == code) return &l;
    return nullptr;
}

LanguageLoadResult ParseLanguages(const std::string& json_text) {
    LanguageLoadResult result;
    try {
        const json root = json::parse(json_text);
        if (!root.is_object()) throw ParseError("languages: root must be an object");
        result.config.primary = RequireString(root, "primary", "languages");
        const json list = Require(root, "languages", "languages");  // small; copy avoids a GCC false positive
        if (!list.is_array() || list.empty())
            throw ParseError("languages: 'languages' must be a non-empty array");
        std::set<std::string> codes;
        for (std::size_t i = 0; i < list.size(); ++i) {
            const std::string where = "languages[" + std::to_string(i) + "]";
            Language l;
            l.code = RequireString(list[i], "code", where);
            l.name = OptString(list[i], "name", where, l.code);
            l.status = OptString(list[i], "status", where, "unverified");
            l.ocr_lang = RequireString(list[i], "ocr_lang", where);
            if (!codes.insert(l.code).second) throw ParseError("duplicate language: " + l.code);
            result.config.languages.push_back(std::move(l));
        }
        if (!result.config.Find(result.config.primary))
            throw ParseError("primary language '" + result.config.primary + "' is not listed");
        result.ok = true;
    } catch (const json::exception& e) {
        result.error = std::string("invalid JSON: ") + e.what();
    } catch (const ParseError& e) {
        result.error = e.what();
    }
    return result;
}

LanguageLoadResult LoadLanguages(const std::filesystem::path& file) {
    try {
        return ParseLanguages(ReadFile(file));
    } catch (const ParseError& e) {
        return {false, {}, e.what()};
    }
}

}  // namespace maba
