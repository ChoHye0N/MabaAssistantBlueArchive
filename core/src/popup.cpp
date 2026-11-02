// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#include "maba/popup.h"

#include <algorithm>
#include <set>
#include <system_error>

#include <nlohmann/json.hpp>

#include "json_util.h"
#include "maba/detect.h"

namespace maba {

namespace fs = std::filesystem;
using nlohmann::json;
using namespace detail;

namespace {

PopupDef ParsePopupJson(const json& root) {
    if (!root.is_object()) throw ParseError("popup: root must be an object");
    PopupDef p;
    p.id = RequireString(root, "id", "popup");
    const std::string where = "popup '" + p.id + "'";

    const json& anchors = Require(root, "anchors", where);
    if (!anchors.is_array() || anchors.empty())
        throw ParseError(where + ": 'anchors' must be a non-empty array");
    for (std::size_t i = 0; i < anchors.size(); ++i)
        p.anchors.push_back(
            ParseTemplateRef(anchors[i], where + ".anchors[" + std::to_string(i) + "]"));

    p.priority = OptInt(root, "priority", where, 0, -1000, 1000);

    const json& dismiss = Require(root, "dismiss", where);
    if (dismiss.is_string()) {
        if (dismiss.get<std::string>() != "back")
            throw ParseError(where + ": 'dismiss' must be \"back\" or a click object");
        p.dismiss = DismissKind::kBack;
    } else if (dismiss.is_object()) {
        p.dismiss = DismissKind::kClick;
        if (dismiss.contains("template")) {
            p.click_template = ParseTemplateRef(dismiss, where + ".dismiss");
        } else if (dismiss.contains("x") || dismiss.contains("y")) {
            p.click_point = ParsePoint(dismiss, where + ".dismiss");
        } else {
            throw ParseError(where + ".dismiss: needs 'template' or 'x'/'y'");
        }
    } else {
        throw ParseError(where + ": 'dismiss' must be \"back\" or a click object");
    }
    return p;
}

}  // namespace

PopupLoadResult ParsePopup(const std::string& json_text) {
    PopupLoadResult result;
    try {
        result.popup = ParsePopupJson(json::parse(json_text));
        result.ok = true;
    } catch (const json::exception& e) {
        result.error = std::string("invalid JSON: ") + e.what();
    } catch (const ParseError& e) {
        result.error = e.what();
    }
    return result;
}

PopupSetLoadResult LoadPopupDir(const fs::path& dir) {
    PopupSetLoadResult result;
    std::error_code ec;
    if (!fs::exists(dir, ec)) {
        result.ok = true;
        return result;
    }
    std::set<std::string> ids;
    try {
        for (const auto& f : JsonFilesIn(dir)) {
            PopupLoadResult p = ParsePopup(ReadFile(f));
            if (!p.ok) {
                result.error = f.filename().generic_string() + ": " + p.error;
                return result;
            }
            if (!ids.insert(p.popup.id).second) {
                result.error = "duplicate popup id: " + p.popup.id;
                return result;
            }
            result.popups.push_back(std::move(p.popup));
        }
    } catch (const ParseError& e) {
        result.error = e.what();
        return result;
    }
    std::stable_sort(result.popups.begin(), result.popups.end(),
                     [](const PopupDef& a, const PopupDef& b) {
                         if (a.priority != b.priority) return a.priority > b.priority;
                         return a.id < b.id;
                     });
    result.ok = true;
    return result;
}

PopupHandler::PopupHandler(IMatcher& matcher, std::vector<PopupDef> popups, PopupOptions options,
                           LogFn log)
    : matcher_(matcher), popups_(std::move(popups)), options_(options), log_(std::move(log)) {}

const PopupDef* PopupHandler::Detect(const Frame& frame) {
    if (frame.empty()) return nullptr;
    for (const auto& p : popups_) {
        if (CheckAnchors(matcher_, frame, p.anchors).detected) return &p;
    }
    return nullptr;
}

bool PopupHandler::Dismiss(IController& controller, const Frame& frame, const PopupDef& popup,
                           std::string& error) {
    if (popup.dismiss == DismissKind::kBack) {
        if (!controller.Back()) {
            error = "popup '" + popup.id + "': back failed";
            return false;
        }
        return true;
    }
    Point target;
    if (popup.click_template) {
        const MatchResult m = matcher_.Find(frame, *popup.click_template);
        if (!m.found) {
            error = "popup '" + popup.id + "': dismiss target " + popup.click_template->file +
                    " not found";
            return false;
        }
        target = m.center;
    } else {
        target = *popup.click_point;
    }
    if (!controller.Click(target)) {
        error = "popup '" + popup.id + "': click failed";
        return false;
    }
    return true;
}

SettleResult PopupHandler::Settle(IController& controller, IClock& clock) {
    SettleResult result;
    const int cap = std::max(options_.max_dismissals, 0);
    while (true) {
        if (options_.cancel && options_.cancel->load()) {
            result.cancelled = true;
            result.error = "cancelled";
            return result;
        }
        result.frame = controller.Screenshot();
        if (result.frame.empty()) {
            result.error = "no frame captured";
            return result;
        }
        const PopupDef* popup = Detect(result.frame);
        if (!popup) {
            result.ok = true;
            return result;
        }
        if (result.dismissed >= cap) {
            result.error = "popup '" + popup->id + "' still visible after " +
                           std::to_string(result.dismissed) + " dismissals";
            Log(log_, LogLevel::kWarn, result.error);
            return result;
        }
        Log(log_, LogLevel::kInfo, "dismissing popup '" + popup->id + "'");
        std::string error;
        if (!Dismiss(controller, result.frame, *popup, error)) {
            result.error = error;
            Log(log_, LogLevel::kWarn, error);
            return result;
        }
        ++result.dismissed;
        clock.Sleep(options_.settle);
    }
}

}  // namespace maba
