// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#include "maba/page.h"

#include <algorithm>

#include <nlohmann/json.hpp>

#include "json_util.h"

namespace maba {

namespace fs = std::filesystem;
using nlohmann::json;
using namespace detail;

namespace {

Transition ParseTransition(const json& obj, const std::string& where) {
    if (!obj.is_object()) throw ParseError(where + ": must be an object");
    Transition t;
    t.to = RequireString(obj, "to", where);
    const json& click = Require(obj, "click", where);
    if (!click.is_object()) throw ParseError(where + ": 'click' must be an object");
    if (click.contains("template")) {
        t.click_template = ParseTemplateRef(click, where + ".click");
    } else if (click.contains("x") || click.contains("y")) {
        t.click_point = ParsePoint(click, where + ".click");
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

    if (auto it = root.find("rois"); it != root.end() && !it->is_null()) {
        if (!it->is_object()) throw ParseError(where + ": 'rois' must be an object");
        for (const auto& [name, value] : it->items()) {
            const json wrapper = {{"roi", value}};
            if (auto r = ParseRoi(wrapper, where + ".rois." + name)) page.rois[name] = *r;
        }
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
    std::string text;
    try {
        text = ReadFile(path);
    } catch (const ParseError& e) {
        return {false, {}, e.what()};
    }
    PageLoadResult result = ParsePage(text);
    if (!result.ok) result.error = path.filename().generic_string() + ": " + result.error;
    return result;
}

PageSetLoadResult LoadPageDir(const fs::path& dir) {
    PageSetLoadResult result;
    std::vector<fs::path> files;
    try {
        files = JsonFilesIn(dir);
    } catch (const ParseError& e) {
        result.error = e.what();
        return result;
    }
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
