// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#include "maba/strings.h"

#include <system_error>

#include <nlohmann/json.hpp>

#include "json_util.h"

namespace maba {

using nlohmann::json;
using namespace detail;

bool LoadStrings(const std::filesystem::path& root, const std::string& lang, StringTable& out,
                 std::string& error) {
    out.clear();
    const auto path = root / "strings" / (lang + ".json");
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return true;
    try {
        const json doc = json::parse(ReadFile(path));
        const json& table = Require(doc, "strings", "strings/" + lang + ".json");
        if (!table.is_object()) throw ParseError("strings/" + lang + ".json: 'strings' must be an object");
        for (const auto& [key, value] : table.items()) {
            if (!value.is_string())
                throw ParseError("strings/" + lang + ".json: '" + key + "' must be a string");
            out[key] = value.get<std::string>();
        }
    } catch (const json::exception& e) {
        error = "strings/" + lang + ".json: invalid JSON: " + e.what();
        return false;
    } catch (const ParseError& e) {
        error = e.what();
        return false;
    }
    return true;
}

}  // namespace maba
