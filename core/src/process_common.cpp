// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#include "maba/process.h"

namespace maba {

std::string QuoteWindowsArg(const std::string& arg) {
    if (!arg.empty() && arg.find_first_of(" \t\n\v\"") == std::string::npos) return arg;
    std::string out = "\"";
    std::size_t backslashes = 0;
    for (char c : arg) {
        if (c == '\\') {
            ++backslashes;
            continue;
        }
        if (c == '"') {
            out.append(backslashes * 2 + 1, '\\');  // escape the backslashes and the quote
        } else {
            out.append(backslashes, '\\');
        }
        backslashes = 0;
        out += c;
    }
    out.append(backslashes * 2, '\\');  // backslashes before the closing quote are doubled
    out += '"';
    return out;
}

}  // namespace maba
