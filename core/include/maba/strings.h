// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// UI text that logic compares (OCR targets such as schedule location names), by key and
// language: strings/<lang>.json = {"language": "ko", "strings": {"<key>": "<text>"}}.
#pragma once

#include <filesystem>
#include <map>
#include <string>

namespace maba {

using StringTable = std::map<std::string, std::string>;

// Loads strings/<lang>.json under `root`. A missing file is an empty table (ok); invalid JSON
// or a non-string value is an error.
bool LoadStrings(const std::filesystem::path& root, const std::string& lang, StringTable& out,
                 std::string& error);

}  // namespace maba
