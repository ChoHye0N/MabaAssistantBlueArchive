// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// UTF-8 <-> std::filesystem::path. Core keeps text as UTF-8 in std::string; since C++20 the
// path API uses std::u8string (char8_t), so conversions go through these two functions.
#pragma once

#include <filesystem>
#include <string>

namespace maba {

inline std::filesystem::path PathFromUtf8(const std::string& utf8) {
    return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}

inline std::string PathToUtf8(const std::filesystem::path& path) {
    const std::u8string s = path.u8string();
    return std::string(s.begin(), s.end());
}

}  // namespace maba
