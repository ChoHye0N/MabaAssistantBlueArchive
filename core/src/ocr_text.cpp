// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#include "maba/ocr_text.h"

#include <algorithm>

namespace maba {

namespace {

std::string Trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    const auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

bool IsDigit(char c) { return c >= '0' && c <= '9'; }

}  // namespace

std::optional<long long> ParseInteger(const std::string& raw) {
    const std::string text = Trim(raw);
    if (text.empty()) return std::nullopt;
    // Split into digit groups by one kind of separator.
    char sep = 0;
    std::vector<std::string> groups(1);
    for (char c : text) {
        if (IsDigit(c)) {
            groups.back() += c;
        } else if (c == ',' || c == '.' || c == ' ') {
            if (sep != 0 && c != sep) return std::nullopt;
            sep = c;
            if (groups.back().empty()) return std::nullopt;  // leading or doubled separator
            groups.emplace_back();
        } else {
            return std::nullopt;
        }
    }
    if (groups.back().empty()) return std::nullopt;  // trailing separator
    if (groups.size() > 1) {
        // Thousands grouping: first group 1-3 digits, the rest exactly 3.
        if (groups[0].size() > 3) return std::nullopt;
        for (std::size_t i = 1; i < groups.size(); ++i)
            if (groups[i].size() != 3) return std::nullopt;
    }
    std::string digits;
    for (const auto& g : groups) digits += g;
    if (digits.size() > 12) return std::nullopt;
    long long v = 0;
    for (char c : digits) v = v * 10 + (c - '0');
    return v;
}

std::optional<Fraction> ParseFraction(const std::string& text) {
    const auto slash = text.find('/');
    if (slash == std::string::npos || text.find('/', slash + 1) != std::string::npos)
        return std::nullopt;
    const auto value = ParseInteger(text.substr(0, slash));
    const auto max = ParseInteger(text.substr(slash + 1));
    if (!value || !max) return std::nullopt;
    return Fraction{*value, *max};
}

std::string JoinLeftToRight(std::vector<OcrResult> pieces) {
    std::stable_sort(pieces.begin(), pieces.end(),
                     [](const OcrResult& a, const OcrResult& b) { return a.box.x < b.box.x; });
    std::string out;
    for (const auto& p : pieces) out += Trim(p.text);
    return out;
}

}  // namespace maba
