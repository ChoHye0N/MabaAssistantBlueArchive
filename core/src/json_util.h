// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Internal helpers shared by the JSON data loaders (pages, popups, tasks, configs).
#pragma once

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "maba/page.h"
#include "maba/types.h"

namespace maba::detail {

struct ParseError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

const nlohmann::json& Require(const nlohmann::json& obj, const char* key, const std::string& where);
std::string RequireString(const nlohmann::json& obj, const char* key, const std::string& where);
int RequireInt(const nlohmann::json& obj, const char* key, const std::string& where);

// Optional members: the fallback when absent, ParseError when present with the wrong type
// or outside [min, max].
std::string OptString(const nlohmann::json& obj, const char* key, const std::string& where,
                      const std::string& fallback);
int OptInt(const nlohmann::json& obj, const char* key, const std::string& where, int fallback,
           int min, int max);
bool OptBool(const nlohmann::json& obj, const char* key, const std::string& where, bool fallback);
std::vector<std::string> OptStringArray(const nlohmann::json& obj, const char* key,
                                        const std::string& where);

// {"x","y","w","h"} inside 1280x720. A zero width or height means "not set".
std::optional<Rect> ParseRoi(const nlohmann::json& obj, const std::string& where,
                             const char* key = "roi");
// {"template", "roi"?, "threshold"?}
TemplateRef ParseTemplateRef(const nlohmann::json& obj, const std::string& where);
// {"x","y"} inside 1280x720.
Point ParsePoint(const nlohmann::json& obj, const std::string& where);

// Reads a whole file (binary). Throws ParseError when it cannot be opened.
std::string ReadFile(const std::filesystem::path& path);
// Sorted *.json files of a directory. Throws ParseError when it cannot be read.
std::vector<std::filesystem::path> JsonFilesIn(const std::filesystem::path& dir);

}  // namespace maba::detail
