// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#include "maba/task.h"

#include <algorithm>
#include <map>
#include <set>

#include <nlohmann/json.hpp>

#include "json_util.h"

namespace maba {

namespace fs = std::filesystem;
using nlohmann::json;
using namespace detail;

const char* ToString(StepAction action) {
    switch (action) {
        case StepAction::kGoto: return "goto";
        case StepAction::kReturnHome: return "return_home";
        case StepAction::kWaitPage: return "wait_page";
        case StepAction::kClick: return "click";
        case StepAction::kClickIfPresent: return "click_if_present";
        case StepAction::kWaitTemplate: return "wait_template";
        case StepAction::kBack: return "back";
        case StepAction::kSleep: return "sleep";
        case StepAction::kWaitStable: return "wait_stable";
        case StepAction::kReadNumber: return "read_number";
        case StepAction::kReadText: return "read_text";
        case StepAction::kClickColor: return "click_color";
        case StepAction::kClickText: return "click_text";
        case StepAction::kSave: return "save";
        case StepAction::kSwipe: return "swipe";
        case StepAction::kWheel: return "wheel";
        case StepAction::kIf: return "if";
        case StepAction::kLoop: return "loop";
        case StepAction::kEach: return "each";
        case StepAction::kEnd: return "end";
        case StepAction::kFail: return "fail";
        case StepAction::kBreak: return "break";
        case StepAction::kScheduleRooms: return "schedule_rooms";
    }
    return "?";
}

namespace {

const std::map<std::string, StepAction>& Actions() {
    static const std::map<std::string, StepAction> actions = {
        {"goto", StepAction::kGoto},
        {"return_home", StepAction::kReturnHome},
        {"wait_page", StepAction::kWaitPage},
        {"click", StepAction::kClick},
        {"click_if_present", StepAction::kClickIfPresent},
        {"wait_template", StepAction::kWaitTemplate},
        {"back", StepAction::kBack},
        {"sleep", StepAction::kSleep},
        {"wait_stable", StepAction::kWaitStable},
        {"read_number", StepAction::kReadNumber},
        {"read_text", StepAction::kReadText},
        {"click_color", StepAction::kClickColor},
        {"click_text", StepAction::kClickText},
        {"save", StepAction::kSave},
        {"swipe", StepAction::kSwipe},
        {"wheel", StepAction::kWheel},
        {"if", StepAction::kIf},
        {"loop", StepAction::kLoop},
        {"each", StepAction::kEach},
        {"end", StepAction::kEnd},
        {"fail", StepAction::kFail},
        {"break", StepAction::kBreak},
        {"schedule_rooms", StepAction::kScheduleRooms},
    };
    return actions;
}

// Offsets may be negative, so they are not points of the frame.
Point ParseOffset(const json& obj, const std::string& where, const char* key = "offset") {
    auto it = obj.find(key);
    if (it == obj.end() || it->is_null()) return {};
    const std::string w = where + "." + key;
    const Point p{RequireInt(*it, "x", w), RequireInt(*it, "y", w)};
    if (p.x <= -kNormWidth || p.x >= kNormWidth || p.y <= -kNormHeight || p.y >= kNormHeight)
        throw ParseError(w + ": offset must be smaller than the frame");
    return p;
}

// The compared value of a setting condition as text: bools "true"/"false", numbers in decimal.
std::string ValueText(const json& v, const std::string& where) {
    if (v.is_boolean()) return v.get<bool>() ? "true" : "false";
    if (v.is_number_integer()) return std::to_string(v.get<long long>());
    if (v.is_string()) return v.get<std::string>();
    throw ParseError(where + ": compared value must be a bool, an integer or a string");
}

Condition ParseCondition(const json& obj, const std::string& where) {
    if (!obj.is_object()) throw ParseError(where + ": must be an object");
    Condition c;
    c.timeout_ms = OptInt(obj, "timeout_ms", where, 0, 0, kMaxStepTimeoutMs);
    if (obj.contains("setting")) {
        c.kind = ConditionKind::kSetting;
        c.key = RequireString(obj, "setting", where);
        static const std::pair<const char*, CompareOp> ops[] = {
            {"equals", CompareOp::kEquals}, {"not_equals", CompareOp::kNotEquals},
            {"contains", CompareOp::kContains}};
        for (const auto& [name, op] : ops) {
            if (auto it = obj.find(name); it != obj.end()) {
                c.op = op;
                c.text = ValueText(*it, where);
                return c;
            }
        }
        throw ParseError(where + ": setting condition needs 'equals', 'not_equals' or 'contains'");
    }
    if (obj.contains("present") || obj.contains("absent")) {
        const bool present = obj.contains("present");
        c.kind = present ? ConditionKind::kPresent : ConditionKind::kAbsent;
        c.templ = ParseTemplateRef(obj.at(present ? "present" : "absent"),
                                   where + (present ? ".present" : ".absent"));
        return c;
    }
    if (obj.contains("color") || obj.contains("no_color")) {
        const bool seen = obj.contains("color");
        c.kind = seen ? ConditionKind::kColor : ConditionKind::kNoColor;
        c.color = ParseColor(obj.at(seen ? "color" : "no_color"), where + (seen ? ".color" : ".no_color"));
        if (auto it = obj.find("roi_by_loop"); it != obj.end()) {
            if (!it->is_array() || it->empty()) throw ParseError(where + ": 'roi_by_loop' must be a non-empty array");
            for (std::size_t i = 0; i < it->size(); ++i) {
                const json wrapper = {{"roi", (*it)[i]}};
                if (auto r = ParseRoi(wrapper, where + ".roi_by_loop[" + std::to_string(i) + "]")) c.rois_by_loop.push_back(*r);
            }
        }
        if (seen) {
            c.min_count = OptInt(obj, "min_count", where, 1, 0, 64);
            if (auto it = obj.find("min_count_by_outer_loop"); it != obj.end()) {
                if (!it->is_array() || it->empty()) throw ParseError(where + ": 'min_count_by_outer_loop' must be a non-empty array");
                for (const auto& n : *it) {
                    if (!n.is_number_integer() || n.get<int>() < 0 || n.get<int>() > 64)
                        throw ParseError(where + ": 'min_count_by_outer_loop' holds counts from 0 to 64");
                    c.min_counts_by_outer_loop.push_back(n.get<int>());
                }
            }
        }
        if (auto it = obj.find("roi_by_value"); it != obj.end()) {
            if (!it->is_array() || it->empty()) throw ParseError(where + ": 'roi_by_value' must be a non-empty array");
            for (std::size_t i = 0; i < it->size(); ++i) {
                const json wrapper = {{"roi", (*it)[i]}};
                if (auto r = ParseRoi(wrapper, where + ".roi_by_value[" + std::to_string(i) + "]")) c.rois_by_value.push_back(*r);
            }
        }
        return c;
    }
    if (obj.contains("value")) {
        c.kind = ConditionKind::kValue;
        c.key = RequireString(obj, "value", where);
        static const std::pair<const char*, CompareOp> ops[] = {
            {"equals", CompareOp::kEquals}, {"not_equals", CompareOp::kNotEquals},
            {"lt", CompareOp::kLess},       {"le", CompareOp::kLessEqual},
            {"gt", CompareOp::kGreater},    {"ge", CompareOp::kGreaterEqual}};
        for (const auto& [name, op] : ops) {
            if (auto it = obj.find(name); it != obj.end()) {
                c.op = op;
                if (it->is_object() && it->contains("setting")) {
                    c.number_setting = RequireString(*it, "setting", where + "." + name);
                    return c;
                }
                if (!it->is_number_integer())
                    throw ParseError(where + ": '" + name + "' must be an integer or {\"setting\": key}");
                c.number = it->get<long long>();
                return c;
            }
        }
        throw ParseError(where + ": value condition needs equals/not_equals/lt/le/gt/ge");
    }
    if (obj.contains("first_iteration")) {
        c.kind = ConditionKind::kFirstIteration;
        c.first = OptBool(obj, "first_iteration", where, true);
        return c;
    }
    throw ParseError(where + ": condition needs 'setting', 'present', 'absent', 'color', 'no_color', 'value' or 'first_iteration'");
}

std::vector<Step> ParseSteps(const json& arr, const std::string& where, int depth);

Step ParseStep(const json& obj, const std::string& where, int depth) {
    if (!obj.is_object()) throw ParseError(where + ": must be an object");
    if (depth > kMaxStepDepth)
        throw ParseError(where + ": steps nested deeper than " + std::to_string(kMaxStepDepth));
    Step s;
    const std::string name = RequireString(obj, "action", where);
    auto it = Actions().find(name);
    if (it == Actions().end()) throw ParseError(where + ": unknown action '" + name + "'");
    s.action = it->second;
    s.repeat = OptInt(obj, "repeat", where, 1, 1, kMaxStepRepeat);
    s.sleep_ms = OptInt(obj, "sleep_ms", where, 0, 0, kMaxSleepMs);
    s.timeout_ms = OptInt(obj, "timeout_ms", where, 5000, 100, kMaxStepTimeoutMs);
    s.optional = OptBool(obj, "optional", where, false);
    s.quiet = OptBool(obj, "quiet", where, false);
    s.fuzzy = OptBool(obj, "fuzzy", where, false);
    s.no_verify = OptBool(obj, "no_verify", where, false);
    s.change_roi = ParseRoi(obj, where, "change_roi");
    if (auto w = obj.find("when"); w != obj.end() && !w->is_null())
        s.when = ParseCondition(*w, where + ".when");

    switch (s.action) {
        case StepAction::kGoto:
        case StepAction::kWaitPage:
            s.page = RequireString(obj, "page", where);
            break;
        case StepAction::kClick:
        case StepAction::kClickIfPresent: {
            if (obj.contains("template")) {
                s.templ = ParseTemplateRef(obj, where);
            } else if (s.action == StepAction::kClick && obj.contains("points")) {
                const json& pts = obj.at("points");
                if (!pts.is_array() || pts.empty())
                    throw ParseError(where + ": 'points' must be a non-empty array");
                for (std::size_t i = 0; i < pts.size(); ++i)
                    s.points.push_back(ParsePoint(pts[i], where + ".points[" + std::to_string(i) + "]"));
                const std::string by = OptString(obj, "point_by", where, "loop");
                if (by != "loop" && by != "value") throw ParseError(where + ": 'point_by' must be 'loop' or 'value'");
                s.points_by_value = by == "value";
            } else if (s.action == StepAction::kClick && (obj.contains("x") || obj.contains("y"))) {
                s.point = ParsePoint(obj, where);
            } else {
                throw ParseError(where + ": " + name +
                                 (s.action == StepAction::kClick ? " needs 'template', 'points' or 'x'/'y'"
                                                                 : " needs 'template'"));
            }
            s.offset = ParseOffset(obj, where);
            const std::string pick = OptString(obj, "pick", where, "best");
            static const std::map<std::string, Pick> picks = {
                {"best", Pick::kBest}, {"top", Pick::kTop}, {"bottom", Pick::kBottom},
                {"left", Pick::kLeft}, {"right", Pick::kRight}};
            auto p = picks.find(pick);
            if (p == picks.end()) throw ParseError(where + ": unknown pick '" + pick + "'");
            s.pick = p->second;
            if (auto e = obj.find("expect"); e != obj.end() && !e->is_null())
                s.expect = ParseTemplateRef(*e, where + ".expect");
            if (auto e = obj.find("expect_gone"); e != obj.end() && !e->is_null())
                s.expect_gone = ParseTemplateRef(*e, where + ".expect_gone");
            s.retries = OptInt(obj, "retries", where, 2, 0, kMaxClickRetries);
            s.on_page = OptString(obj, "on_page", where, "");
            if (auto r = obj.find("require"); r != obj.end() && !r->is_null())
                s.require = ParseTemplateRef(*r, where + ".require");
            break;
        }
        case StepAction::kWaitTemplate:
            s.templ = ParseTemplateRef(obj, where);
            break;
        case StepAction::kSleep:
            s.sleep_ms = RequireInt(obj, "sleep_ms", where);
            if (s.sleep_ms < 1 || s.sleep_ms > kMaxSleepMs)
                throw ParseError(where + ": 'sleep_ms' must be in [1, " +
                                 std::to_string(kMaxSleepMs) + "]");
            break;
        case StepAction::kReadNumber:
        case StepAction::kReadText:
            s.roi = ParseRoi(obj, where);
            if (!s.roi) throw ParseError(where + ": read_number needs a non-empty 'roi'");
            s.store = RequireString(obj, "store", where);
            s.lenient = OptBool(obj, "lenient", where, false);
            break;
        case StepAction::kClickColor:
            s.color = ParseColor(Require(obj, "color", where), where + ".color");
            s.offset = ParseOffset(obj, where);
            if (obj.contains("retry_offset")) s.retry_offset = ParseOffset(obj, where, "retry_offset");
            s.max = OptInt(obj, "max", where, 10, 1, kMaxLoop);
            break;
        case StepAction::kClickText:
            s.roi = ParseRoi(obj, where);
            if (!s.roi) throw ParseError(where + ": click_text needs a non-empty 'roi'");
            s.texts = OptStringArray(obj, "texts", where);
            s.text_setting = OptString(obj, "text_setting", where, "");
            s.text_key = OptString(obj, "text_key", where, "");
            if (!s.texts.empty() + !s.text_setting.empty() + !s.text_key.empty() != 1)
                throw ParseError(where + ": click_text needs exactly one of 'texts', 'text_setting' and 'text_key'");
            s.offset = ParseOffset(obj, where);
            if (obj.contains("click_x")) {
                s.click_x = RequireInt(obj, "click_x", where);
                if (*s.click_x < 0 || *s.click_x >= kNormWidth) throw ParseError(where + ": 'click_x' outside the frame");
            }
            s.max_scrolls = OptInt(obj, "max_scrolls", where, 0, 0, 80);
            if (s.max_scrolls > 0) {
                s.point = ParsePoint(Require(obj, "from", where), where + ".from");
                s.to = ParsePoint(Require(obj, "to", where), where + ".to");
            }
            s.duration_ms = OptInt(obj, "duration_ms", where, 400, 50, 5000);
            break;
        case StepAction::kScheduleRooms: {
            ScheduleSpec sp;
            const auto rect = [&](const json& v, const std::string& at) {
                const json wrapper = {{"roi", v}};
                const auto r = ParseRoi(wrapper, at);
                if (!r) throw ParseError(at + ": needs a non-empty area");
                return *r;
            };
            const auto rects = [&](const char* key) {
                std::vector<Rect> out;
                const json& arr = Require(obj, key, where);
                if (!arr.is_array()) throw ParseError(where + ": '" + key + "' must be an array");
                for (std::size_t i = 0; i < arr.size(); ++i)
                    out.push_back(rect(arr[i], where + "." + key + "[" + std::to_string(i) + "]"));
                return out;
            };
            sp.locations_setting = RequireString(obj, "locations_setting", where);
            sp.prefer_setting = RequireString(obj, "prefer_setting", where);
            sp.name_key = RequireString(obj, "name_key", where);
            sp.tickets = RequireString(obj, "tickets", where);
            sp.name_roi = rect(Require(obj, "name_roi", where), where + ".name_roi");
            sp.next = ParsePoint(Require(obj, "next", where), where + ".next");
            sp.open = ParseTemplateRef(Require(obj, "open", where), where + ".open");
            sp.list = ParseTemplateRef(Require(obj, "list", where), where + ".list");
            sp.info = ParseTemplateRef(Require(obj, "info", where), where + ".info");
            sp.start = ParseTemplateRef(Require(obj, "start", where), where + ".start");
            sp.ticket_roi = rect(Require(obj, "ticket_roi", where), where + ".ticket_roi");
            const json& rooms = Require(obj, "rooms", where);
            if (!rooms.is_array() || rooms.empty() || rooms.size() > 12)
                throw ParseError(where + ": 'rooms' must list 1 to 12 points");
            for (std::size_t i = 0; i < rooms.size(); ++i)
                sp.rooms.push_back(ParsePoint(rooms[i], where + ".rooms[" + std::to_string(i) + "]"));
            sp.card_rois = rects("card_rois");
            sp.owned_rois = rects("owned_rois");
            sp.done_rois = rects("done_rois");
            if (sp.card_rois.size() != sp.rooms.size() || sp.owned_rois.size() != sp.rooms.size() ||
                sp.done_rois.size() != sp.rooms.size())
                throw ParseError(where + ": 'card_rois', 'owned_rois' and 'done_rois' need one area per room");
            sp.card = ParseColor(Require(obj, "card", where), where + ".card");
            sp.owned = ParseColor(Require(obj, "owned", where), where + ".owned");
            sp.done = ParseColor(Require(obj, "done", where), where + ".done");
            s.schedule = std::move(sp);
            break;
        }
        case StepAction::kSwipe:
            s.point = ParsePoint(Require(obj, "from", where), where + ".from");
            s.to = ParsePoint(Require(obj, "to", where), where + ".to");
            s.duration_ms = OptInt(obj, "duration_ms", where, 400, 50, 5000);
            break;
        case StepAction::kWheel:
            s.point = ParsePoint(obj, where);
            s.notches = OptInt(obj, "notches", where, 0, -20, 20);
            if (s.notches == 0) throw ParseError(where + ": 'notches' must be -20..20 and not 0");
            break;
        case StepAction::kSave:
            s.file = RequireString(obj, "file", where);
            if (s.file.find("..") != std::string::npos || s.file.empty() || s.file[0] == '/' || s.file[0] == '\\' ||
                s.file.find(':') != std::string::npos)
                throw ParseError(where + ": 'file' must be a relative path inside the data dir");
            s.values = OptStringArray(obj, "values", where);
            s.history = OptBool(obj, "history", where, false);
            break;
        case StepAction::kIf:
            if (!s.when) throw ParseError(where + ": 'if' needs 'when'");
            s.steps = ParseSteps(Require(obj, "then", where), where + ".then", depth + 1);
            if (auto e = obj.find("else"); e != obj.end() && !e->is_null())
                s.else_steps = ParseSteps(*e, where + ".else", depth + 1);
            break;
        case StepAction::kLoop: {
            const int sources = obj.contains("max") + obj.contains("max_setting") + obj.contains("max_value");
            if (sources != 1)
                throw ParseError(where + ": loop needs exactly one of 'max', 'max_setting', 'max_value'");
            s.max = OptInt(obj, "max", where, 1, 1, kMaxLoop);
            s.max_setting = OptString(obj, "max_setting", where, "");
            s.max_value = OptString(obj, "max_value", where, "");
            s.max_adjust = OptInt(obj, "max_adjust", where, 0, -kMaxLoop, kMaxLoop);
            if (auto u = obj.find("until"); u != obj.end() && !u->is_null())
                s.until = ParseCondition(*u, where + ".until");
            s.steps = ParseSteps(Require(obj, "steps", where), where + ".steps", depth + 1);
            break;
        }
        case StepAction::kEach: {
            s.setting = RequireString(obj, "setting", where);
            s.rotate_setting = OptString(obj, "rotate_setting", where, "");
            if (auto c = obj.find("cases"); c != obj.end() && !c->is_null()) {
                if (!c->is_object()) throw ParseError(where + ": 'cases' must be an object");
                for (const auto& [value, body] : c->items())
                    s.cases.emplace_back(value, ParseSteps(body, where + ".cases." + value, depth + 1));
            }
            if (auto b = obj.find("steps"); b != obj.end() && !b->is_null())
                s.steps = ParseSteps(*b, where + ".steps", depth + 1);
            if (s.cases.empty() && s.steps.empty())
                throw ParseError(where + ": each needs 'cases' or 'steps'");
            break;
        }
        case StepAction::kEnd: {
            s.message = OptString(obj, "message", where, "");
            const std::string status = OptString(obj, "status", where, "succeeded");
            if (status != "succeeded" && status != "skipped")
                throw ParseError(where + ": 'status' must be succeeded or skipped");
            s.end_skipped = status == "skipped";
            break;
        }
        case StepAction::kFail:
            s.message = RequireString(obj, "message", where);
            break;
        case StepAction::kReturnHome:
        case StepAction::kBack:
        case StepAction::kBreak:
            break;
    }
    return s;
}

std::vector<Step> ParseSteps(const json& arr, const std::string& where, int depth) {
    if (!arr.is_array()) throw ParseError(where + ": must be an array");
    std::vector<Step> steps;
    for (std::size_t i = 0; i < arr.size(); ++i)
        steps.push_back(ParseStep(arr[i], where + "[" + std::to_string(i) + "]", depth));
    return steps;
}

const std::map<std::string, SettingType>& SettingTypes() {
    static const std::map<std::string, SettingType> types = {
        {"bool", SettingType::kBool}, {"int", SettingType::kInt},
        {"choice", SettingType::kChoice}, {"list", SettingType::kList}};
    return types;
}

// Checks `v` against the definition and converts it. Throws ParseError.
SettingValue ParseSettingValue(const SettingDef& def, const json& v, const std::string& where) {
    SettingValue out;
    switch (def.type) {
        case SettingType::kBool:
            if (!v.is_boolean()) throw ParseError(where + ": must be true or false");
            out.scalar = v.get<bool>() ? "true" : "false";
            break;
        case SettingType::kInt: {
            if (!v.is_number_integer()) throw ParseError(where + ": must be an integer");
            const long long n = v.get<long long>();
            if (n < def.min || n > def.max)
                throw ParseError(where + ": must be in [" + std::to_string(def.min) + ", " +
                                 std::to_string(def.max) + "]");
            out.scalar = std::to_string(n);
            break;
        }
        case SettingType::kChoice:
            if (!v.is_string()) throw ParseError(where + ": must be a string");
            out.scalar = v.get<std::string>();
            if (std::find(def.choices.begin(), def.choices.end(), out.scalar) == def.choices.end())
                throw ParseError(where + ": '" + out.scalar + "' is not one of the choices");
            break;
        case SettingType::kList:
            if (!v.is_array()) throw ParseError(where + ": must be an array of strings");
            for (const auto& item : v) {
                if (!item.is_string()) throw ParseError(where + ": must be an array of strings");
                const std::string s = item.get<std::string>();
                if (!def.choices.empty() &&
                    std::find(def.choices.begin(), def.choices.end(), s) == def.choices.end())
                    throw ParseError(where + ": '" + s + "' is not one of the choices");
                out.list.push_back(s);
            }
            break;
    }
    return out;
}

SettingDef ParseSettingDef(const json& obj, const std::string& where) {
    if (!obj.is_object()) throw ParseError(where + ": must be an object");
    SettingDef d;
    d.key = RequireString(obj, "key", where);
    const std::string type = RequireString(obj, "type", where);
    auto it = SettingTypes().find(type);
    if (it == SettingTypes().end()) throw ParseError(where + ": unknown setting type '" + type + "'");
    d.type = it->second;
    if (d.type == SettingType::kInt) {
        d.min = RequireInt(obj, "min", where);
        d.max = RequireInt(obj, "max", where);
        if (d.max < d.min) throw ParseError(where + ": 'max' is below 'min'");
    }
    d.choices = OptStringArray(obj, "choices", where);
    d.description = OptString(obj, "description", where, "");
    d.label_key = OptString(obj, "label_key", where, "");
    d.ui = OptString(obj, "ui", where, "");
    if (!d.ui.empty() && d.ui != "checklist") throw ParseError(where + ": unknown ui '" + d.ui + "'");
    if (d.type == SettingType::kChoice && d.choices.empty())
        throw ParseError(where + ": a choice setting needs 'choices'");
    d.default_value = ParseSettingValue(d, Require(obj, "default", where), where + ".default");
    return d;
}

TaskSpec ParseTaskJson(const json& root) {
    if (!root.is_object()) throw ParseError("task: root must be an object");
    TaskSpec t;
    t.id = RequireString(root, "id", "task");
    const std::string where = "task '" + t.id + "'";
    t.name = OptString(root, "name", where, t.id);
    t.after = OptStringArray(root, "after", where);
    t.order = OptInt(root, "order", where, kDefaultTaskOrder, 0, 100000);
    t.status = OptString(root, "status", where, "todo");
    t.needs_ocr = OptBool(root, "needs_ocr", where, false);
    if (auto it = root.find("keep_popups"); it != root.end() && !it->is_null()) {
        if (!it->is_array()) throw ParseError(where + ": 'keep_popups' must be a list of popup ids");
        for (const auto& id : *it) {
            if (!id.is_string()) throw ParseError(where + ": 'keep_popups' must be a list of popup ids");
            t.keep_popups.push_back(id.get<std::string>());
        }
    }
    if (auto it = root.find("settings"); it != root.end() && !it->is_null()) {
        if (!it->is_array()) throw ParseError(where + ": 'settings' must be an array");
        std::set<std::string> keys;
        for (std::size_t i = 0; i < it->size(); ++i) {
            SettingDef d = ParseSettingDef((*it)[i], where + ".settings[" + std::to_string(i) + "]");
            if (!keys.insert(d.key).second)
                throw ParseError(where + ": duplicate setting '" + d.key + "'");
            t.settings.push_back(std::move(d));
        }
    }
    if (auto it = root.find("steps"); it != root.end() && !it->is_null())
        t.steps = ParseSteps(*it, where + ".steps", 0);
    return t;
}

}  // namespace

TaskLoadResult ParseTask(const std::string& json_text) {
    TaskLoadResult result;
    try {
        result.task = ParseTaskJson(json::parse(json_text));
        result.ok = true;
    } catch (const json::exception& e) {
        result.error = std::string("invalid JSON: ") + e.what();
    } catch (const ParseError& e) {
        result.error = e.what();
    }
    return result;
}

TaskSetLoadResult LoadTaskDir(const fs::path& dir) {
    TaskSetLoadResult result;
    std::set<std::string> ids;
    try {
        for (const auto& f : JsonFilesIn(dir)) {
            TaskLoadResult t = ParseTask(ReadFile(f));
            if (!t.ok) {
                result.error = f.filename().generic_string() + ": " + t.error;
                return result;
            }
            if (!ids.insert(t.task.id).second) {
                result.error = "duplicate task id: " + t.task.id;
                return result;
            }
            result.tasks.push_back(std::move(t.task));
        }
    } catch (const ParseError& e) {
        result.error = e.what();
        return result;
    }
    SortTasksByOrder(result.tasks);
    result.ok = true;
    return result;
}

void SortTasksByOrder(std::vector<TaskSpec>& tasks) {
    std::stable_sort(tasks.begin(), tasks.end(),
                     [](const TaskSpec& a, const TaskSpec& b) { return a.order < b.order; });
}

TaskSettings DefaultSettings(const TaskSpec& task) {
    TaskSettings out;
    for (const auto& d : task.settings) out[d.key] = d.default_value;
    return out;
}

bool ResolveSettings(const std::vector<TaskSpec>& tasks, const std::string& overrides_json,
                     std::map<std::string, TaskSettings>& out, std::string& error) {
    out.clear();
    for (const auto& t : tasks) out[t.id] = DefaultSettings(t);
    if (overrides_json.empty()) return true;
    try {
        const json root = json::parse(overrides_json);
        if (!root.is_object()) throw ParseError("settings: root must be an object");
        for (const auto& [task_id, values] : root.items()) {
            auto task = std::find_if(tasks.begin(), tasks.end(),
                                     [&](const TaskSpec& t) { return t.id == task_id; });
            if (task == tasks.end()) throw ParseError("settings: unknown task '" + task_id + "'");
            if (!values.is_object())
                throw ParseError("settings." + task_id + ": must be an object");
            for (const auto& [key, value] : values.items()) {
                auto def = std::find_if(task->settings.begin(), task->settings.end(),
                                        [&](const SettingDef& d) { return d.key == key; });
                if (def == task->settings.end())
                    throw ParseError("settings." + task_id + ": unknown setting '" + key + "'");
                out[task_id][key] = ParseSettingValue(*def, value, "settings." + task_id + "." + key);
            }
        }
    } catch (const json::exception& e) {
        error = std::string("settings: invalid JSON: ") + e.what();
        return false;
    } catch (const ParseError& e) {
        error = e.what();
        return false;
    }
    return true;
}

namespace {

void ValidateSteps(const TaskSpec& t, const std::vector<Step>& steps, const std::string& where,
                   const std::map<std::string, PageDef>& pages, std::vector<std::string>& problems) {
    auto declared = [&](const std::string& key, SettingType type) {
        return std::any_of(t.settings.begin(), t.settings.end(),
                           [&](const SettingDef& d) { return d.key == key && d.type == type; });
    };
    auto check_condition = [&](const std::optional<Condition>& c, const std::string& at) {
        if (c && c->kind == ConditionKind::kSetting &&
            std::none_of(t.settings.begin(), t.settings.end(),
                         [&](const SettingDef& d) { return d.key == c->key; }))
            problems.push_back(at + " uses undeclared setting '" + c->key + "'");
    };
    for (std::size_t i = 0; i < steps.size(); ++i) {
        const Step& s = steps[i];
        const std::string at = where + "[" + std::to_string(i) + "]";
        if ((s.action == StepAction::kGoto || s.action == StepAction::kWaitPage) && !pages.count(s.page))
            problems.push_back(at + " uses unknown page '" + s.page + "'");
        if (!s.on_page.empty() && !pages.count(s.on_page))
            problems.push_back(at + " uses unknown page '" + s.on_page + "'");
        // A fixed point on the wrong screen hits whatever is there.
        if (s.action == StepAction::kClick && !s.templ && !s.expect && !s.expect_gone && !s.require && s.on_page.empty())
            problems.push_back(at + " clicks a fixed point without 'expect', 'require' or 'on_page'");
        check_condition(s.when, at);
        check_condition(s.until, at);
        if (!s.max_setting.empty() && !declared(s.max_setting, SettingType::kInt))
            problems.push_back(at + " takes its count from undeclared int setting '" + s.max_setting + "'");
        if (s.action == StepAction::kClickText && !s.text_setting.empty() &&
            !declared(s.text_setting, SettingType::kList) && !declared(s.text_setting, SettingType::kChoice))
            problems.push_back(at + " reads texts from undeclared list or choice setting '" + s.text_setting + "'");
        if (s.action == StepAction::kEach && !declared(s.setting, SettingType::kList))
            problems.push_back(at + " iterates undeclared list setting '" + s.setting + "'");
        ValidateSteps(t, s.steps, at, pages, problems);
        ValidateSteps(t, s.else_steps, at + ".else", pages, problems);
        for (const auto& [value, body] : s.cases) ValidateSteps(t, body, at + "." + value, pages, problems);
    }
}

}  // namespace

std::vector<std::string> ValidateTasks(const std::vector<TaskSpec>& tasks,
                                       const std::map<std::string, PageDef>& pages) {
    std::vector<std::string> problems;
    std::set<std::string> ids;
    for (const auto& t : tasks) ids.insert(t.id);
    for (const auto& t : tasks) {
        for (const auto& dep : t.after) {
            if (!ids.count(dep))
                problems.push_back("task '" + t.id + "' runs after unknown task '" + dep + "'");
        }
        ValidateSteps(t, t.steps, "task '" + t.id + "' step", pages, problems);
    }
    return problems;
}

}  // namespace maba
