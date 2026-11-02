// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#include "json_util.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <system_error>

namespace maba::detail {

namespace fs = std::filesystem;
using nlohmann::json;

const json& Require(const json& obj, const char* key, const std::string& where) {
    auto it = obj.find(key);
    if (it == obj.end()) throw ParseError(where + ": missing '" + key + "'");
    return *it;
}

std::string RequireString(const json& obj, const char* key, const std::string& where) {
    const json& v = Require(obj, key, where);
    if (!v.is_string() || v.get<std::string>().empty())
        throw ParseError(where + ": '" + key + "' must be a non-empty string");
    return v.get<std::string>();
}

int RequireInt(const json& obj, const char* key, const std::string& where) {
    const json& v = Require(obj, key, where);
    if (!v.is_number_integer()) throw ParseError(where + ": '" + key + "' must be an integer");
    return v.get<int>();
}

std::string OptString(const json& obj, const char* key, const std::string& where,
                      const std::string& fallback) {
    auto it = obj.find(key);
    if (it == obj.end() || it->is_null()) return fallback;
    if (!it->is_string()) throw ParseError(where + ": '" + key + "' must be a string");
    return it->get<std::string>();
}

int OptInt(const json& obj, const char* key, const std::string& where, int fallback, int min,
           int max) {
    auto it = obj.find(key);
    if (it == obj.end() || it->is_null()) return fallback;
    if (!it->is_number_integer()) throw ParseError(where + ": '" + key + "' must be an integer");
    const auto v = it->get<long long>();
    if (v < min || v > max)
        throw ParseError(where + ": '" + key + "' must be in [" + std::to_string(min) + ", " +
                         std::to_string(max) + "]");
    return static_cast<int>(v);
}

bool OptBool(const json& obj, const char* key, const std::string& where, bool fallback) {
    auto it = obj.find(key);
    if (it == obj.end() || it->is_null()) return fallback;
    if (!it->is_boolean()) throw ParseError(where + ": '" + key + "' must be true or false");
    return it->get<bool>();
}

std::vector<std::string> OptStringArray(const json& obj, const char* key, const std::string& where) {
    std::vector<std::string> out;
    auto it = obj.find(key);
    if (it == obj.end() || it->is_null()) return out;
    if (!it->is_array()) throw ParseError(where + ": '" + key + "' must be an array of strings");
    for (const auto& v : *it) {
        if (!v.is_string() || v.get<std::string>().empty())
            throw ParseError(where + ": '" + key + "' must contain non-empty strings");
        out.push_back(v.get<std::string>());
    }
    return out;
}

std::optional<Rect> ParseRoi(const json& obj, const std::string& where, const char* key) {
    auto it = obj.find(key);
    if (it == obj.end() || it->is_null()) return std::nullopt;
    const std::string w = where + "." + key;
    if (!it->is_object()) throw ParseError(where + ": '" + key + "' must be an object");
    const Rect r{RequireInt(*it, "x", w), RequireInt(*it, "y", w), RequireInt(*it, "w", w),
                 RequireInt(*it, "h", w)};
    if (r.width == 0 || r.height == 0) return std::nullopt;
    if (r.x < 0 || r.y < 0 || r.width < 0 || r.height < 0 || r.x + r.width > kNormWidth ||
        r.y + r.height > kNormHeight)
        throw ParseError(where + ": '" + key + "' must lie inside 1280x720");
    return r;
}

TemplateRef ParseTemplateRef(const json& obj, const std::string& where) {
    if (!obj.is_object()) throw ParseError(where + ": must be an object");
    TemplateRef ref;
    ref.file = RequireString(obj, "template", where);
    ref.roi = ParseRoi(obj, where);
    if (auto it = obj.find("threshold"); it != obj.end()) {
        if (!it->is_number()) throw ParseError(where + ": 'threshold' must be a number");
        ref.threshold = it->get<double>();
        if (!(ref.threshold > 0.0 && ref.threshold <= 1.0))
            throw ParseError(where + ": 'threshold' must be in (0, 1]");
    }
    return ref;
}

Point ParsePoint(const json& obj, const std::string& where) {
    const Point p{RequireInt(obj, "x", where), RequireInt(obj, "y", where)};
    if (p.x < 0 || p.y < 0 || p.x >= kNormWidth || p.y >= kNormHeight)
        throw ParseError(where + ": point must lie inside 1280x720");
    return p;
}

std::string ReadFile(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw ParseError("cannot open " + path.generic_string());
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

std::vector<fs::path> JsonFilesIn(const fs::path& dir) {
    std::error_code ec;
    std::vector<fs::path> files;
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        if (it->is_regular_file(ec) && it->path().extension() == ".json") files.push_back(it->path());
    }
    if (ec) throw ParseError("cannot read " + dir.generic_string() + ": " + ec.message());
    std::sort(files.begin(), files.end());
    return files;
}

}  // namespace maba::detail
