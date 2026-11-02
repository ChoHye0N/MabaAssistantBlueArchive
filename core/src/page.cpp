// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#include "maba/page.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <system_error>

#include <nlohmann/json.hpp>

namespace maba {

namespace fs = std::filesystem;
using nlohmann::json;

namespace {

struct ParseError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

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

// A roi with zero width or height means "not set" (placeholders in the format draft).
std::optional<Rect> ParseRoi(const json& obj, const std::string& where) {
    auto it = obj.find("roi");
    if (it == obj.end() || it->is_null()) return std::nullopt;
    if (!it->is_object()) throw ParseError(where + ": 'roi' must be an object");
    const Rect r{RequireInt(*it, "x", where + ".roi"), RequireInt(*it, "y", where + ".roi"),
                 RequireInt(*it, "w", where + ".roi"), RequireInt(*it, "h", where + ".roi")};
    if (r.width == 0 || r.height == 0) return std::nullopt;
    if (r.x < 0 || r.y < 0 || r.width < 0 || r.height < 0 || r.x + r.width > kNormWidth ||
        r.y + r.height > kNormHeight)
        throw ParseError(where + ": 'roi' must lie inside 1280x720");
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

Transition ParseTransition(const json& obj, const std::string& where) {
    if (!obj.is_object()) throw ParseError(where + ": must be an object");
    Transition t;
    t.to = RequireString(obj, "to", where);
    const json& click = Require(obj, "click", where);
    if (!click.is_object()) throw ParseError(where + ": 'click' must be an object");
    if (click.contains("template")) {
        t.click_template = ParseTemplateRef(click, where + ".click");
    } else if (click.contains("x") || click.contains("y")) {
        const Point p{RequireInt(click, "x", where + ".click"),
                      RequireInt(click, "y", where + ".click")};
        if (p.x < 0 || p.y < 0 || p.x >= kNormWidth || p.y >= kNormHeight)
            throw ParseError(where + ".click: point must lie inside 1280x720");
        t.click_point = p;
    } else {
        throw ParseError(where + ".click: needs 'template' or 'x'/'y'");
    }
    return t;
}

PageDef ParsePageJson(const json& root) {
    if (!root.is_object()) throw ParseError("page: root must be an object");
    PageDef page;
    page.id = RequireString(root, "id", "page");
    const std::string where = "page '" + page.id + "'";

    const json& anchors = Require(root, "anchors", where);
    if (!anchors.is_array() || anchors.empty())
        throw ParseError(where + ": 'anchors' must be a non-empty array");
    for (std::size_t i = 0; i < anchors.size(); ++i)
        page.anchors.push_back(
            ParseTemplateRef(anchors[i], where + ".anchors[" + std::to_string(i) + "]"));

    if (auto it = root.find("transitions"); it != root.end()) {
        if (!it->is_array()) throw ParseError(where + ": 'transitions' must be an array");
        for (std::size_t i = 0; i < it->size(); ++i)
            page.transitions.push_back(
                ParseTransition((*it)[i], where + ".transitions[" + std::to_string(i) + "]"));
    }
    return page;
}

}  // namespace

const Transition* PageDef::FindTransition(const std::string& to) const {
    auto it = std::find_if(transitions.begin(), transitions.end(),
                           [&](const Transition& t) { return t.to == to; });
    return it == transitions.end() ? nullptr : &*it;
}

PageLoadResult ParsePage(const std::string& json_text) {
    PageLoadResult result;
    try {
        result.page = ParsePageJson(json::parse(json_text));
        result.ok = true;
    } catch (const json::exception& e) {
        result.error = std::string("invalid JSON: ") + e.what();
    } catch (const ParseError& e) {
        result.error = e.what();
    }
    return result;
}

PageLoadResult LoadPageFile(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {false, {}, "cannot open " + path.generic_string()};
    std::ostringstream text;
    text << in.rdbuf();
    PageLoadResult result = ParsePage(text.str());
    if (!result.ok) result.error = path.filename().generic_string() + ": " + result.error;
    return result;
}

PageSetLoadResult LoadPageDir(const fs::path& dir) {
    PageSetLoadResult result;
    std::error_code ec;
    std::vector<fs::path> files;
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        if (it->is_regular_file(ec) && it->path().extension() == ".json") files.push_back(it->path());
    }
    if (ec) {
        result.error = "cannot read " + dir.generic_string() + ": " + ec.message();
        return result;
    }
    std::sort(files.begin(), files.end());
    for (const auto& f : files) {
        PageLoadResult page = LoadPageFile(f);
        if (!page.ok) {
            result.error = page.error;
            return result;
        }
        const std::string id = page.page.id;
        if (!result.pages.emplace(id, std::move(page.page)).second) {
            result.error = "duplicate page id: " + id;
            return result;
        }
    }
    result.ok = true;
    return result;
}

}  // namespace maba
