// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#include "maba/task_runner.h"

#include "maba/screen_change.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <set>

#include <nlohmann/json.hpp>

#include "maba/color.h"
#include "maba/ocr_text.h"
#include "maba/utf8.h"

namespace maba {

using std::chrono::milliseconds;

const char* ToString(TaskStatus status) {
    switch (status) {
        case TaskStatus::kPending: return "pending";
        case TaskStatus::kRunning: return "running";
        case TaskStatus::kSucceeded: return "succeeded";
        case TaskStatus::kFailed: return "failed";
        case TaskStatus::kSkipped: return "skipped";
        case TaskStatus::kCancelled: return "cancelled";
    }
    return "?";
}

TaskRunner::TaskRunner(Navigator& navigator, IOcr* ocr, RunnerOptions options, LogFn log)
    : nav_(navigator), ocr_(ocr), options_(std::move(options)), log_(std::move(log)) {}

void TaskRunner::Notify(const TaskRunResult& r) {
    if (options_.on_task) options_.on_task(r);
}

namespace {

constexpr int kNumberCropMargin = 16;

WaitOptions StepWait(int timeout_ms, const std::atomic<bool>* cancel) {
    WaitOptions w;
    w.timeout = milliseconds{timeout_ms};
    w.interval = milliseconds{300};
    w.max_attempts = timeout_ms / 100 + 2;
    w.cancel = cancel;
    return w;
}

TemplateRef Substitute(TemplateRef ref, const std::string& value) {
    const std::string key = "{value}";
    if (const auto pos = ref.file.find(key); pos != std::string::npos) ref.file.replace(pos, key.size(), value);
    return ref;
}

Point Offset(Point p, Point d) {
    return {std::clamp(p.x + d.x, 0, kNormWidth - 1), std::clamp(p.y + d.y, 0, kNormHeight - 1)};
}

Point Center(const Rect& r) { return {r.x + r.width / 2, r.y + r.height / 2}; }

std::string UtcNow() {
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

// Merges the step's values into the JSON record at `path` (created when missing).
// History entries keep the numbers with a timestamp; the newest 1000 are kept.
bool SaveRecordUnchecked(const std::filesystem::path& path, const Step& s,
                         const TaskRunResult& result, std::string& error);

bool SaveRecord(const std::filesystem::path& path, const Step& s, const TaskRunResult& result,
                std::string& error) {
    try {
        return SaveRecordUnchecked(path, s, result, error);
    } catch (const std::exception& e) {
        error = std::string("cannot save the record: ") + e.what();
        return false;
    }
}

bool SaveRecordUnchecked(const std::filesystem::path& path, const Step& s,
                         const TaskRunResult& result, std::string& error) {
    using nlohmann::json;
    json record = json::object();
    std::error_code ec;
    if (std::filesystem::exists(path, ec)) {
        std::ifstream in(path, std::ios::binary);
        try {
            record = json::parse(in);
        } catch (const json::exception&) {
            record = json::object();  // a damaged record is rewritten
        }
        if (!record.is_object()) record = json::object();
    }
    const std::string now = UtcNow();
    record["updated"] = now;
    json entry = {{"time", now}};
    for (const auto& name : s.values) {
        auto it = result.values.find(name);
        if (it == result.values.end()) continue;  // not read this time: keep the old value
        record["values"][name] = it->second;
        entry[name] = it->second;
    }
    if (s.history && entry.size() > 1) {
        json& history = record["history"];
        if (!history.is_array()) history = json::array();
        history.push_back(entry);
        if (history.size() > 1000) history.erase(history.begin(), history.begin() + (history.size() - 1000));
    }
    std::filesystem::create_directories(path.parent_path(), ec);
    const auto tmp = std::filesystem::path(path).concat(".tmp");
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) {
            error = "cannot write " + PathToUtf8(tmp);
            return false;
        }
        out << record.dump(2) << '\n';
    }
    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        error = "cannot replace " + PathToUtf8(path) + ": " + ec.message();
        return false;
    }
    return true;
}

// The last run of digits and number punctuation: "tickets 5/5" -> "5/5".
std::string TrailingNumber(const std::string& text) {
    const std::string chars = "0123456789,./ ";
    std::size_t end = text.find_last_of("0123456789");
    if (end == std::string::npos) return text;
    std::size_t begin = end;
    while (begin > 0 && chars.find(text[begin - 1]) != std::string::npos) --begin;
    while (begin < end && text[begin] == ' ') ++begin;
    return text.substr(begin, end - begin + 1);
}

}  // namespace

struct TaskRunner::Context {
    const TaskSpec& task;
    const TaskSettings& settings;
    TaskRunResult& result;
    std::vector<int> loop_index;  // iteration of each enclosing loop, innermost last
    std::string each_value;       // current value of the innermost each
    std::string error;            // why the run stopped (kFail)
    std::vector<std::string> skipped;  // optional steps that failed, for the result
    int depth = 0;
};

bool TaskRunner::Holds(const Condition& c, Context& ctx) {
    switch (c.kind) {
        case ConditionKind::kSetting: {
            auto it = ctx.settings.find(c.key);
            if (it == ctx.settings.end()) return false;
            const SettingValue& v = it->second;
            switch (c.op) {
                case CompareOp::kEquals: return v.scalar == c.text;
                case CompareOp::kNotEquals: return v.scalar != c.text;
                case CompareOp::kContains:
                    return std::find(v.list.begin(), v.list.end(), c.text) != v.list.end();
                default: return false;
            }
        }
        case ConditionKind::kPresent:
        case ConditionKind::kAbsent: {
            const TemplateRef ref = Substitute(*c.templ, ctx.each_value);
            bool seen = false;
            if (c.timeout_ms > 0) {
                seen = nav_.WaitForTemplate(ref, StepWait(c.timeout_ms, options_.cancel)).has_value();
            } else {
                const Observation obs = nav_.Observe();
                seen = obs.ok && nav_.Matcher().Find(obs.frame, ref).found;
            }
            return c.kind == ConditionKind::kPresent ? seen : !seen;
        }
        case ConditionKind::kNoColor: {
            const Observation obs = nav_.Observe();
            return obs.ok && FindColorBlobs(obs.frame, *c.color, 1).empty();
        }
        case ConditionKind::kColor: {
            bool seen = false;
            WaitUntil(
                [&] {
                    const Observation obs = nav_.Observe();
                    seen = obs.ok && !FindColorBlobs(obs.frame, *c.color, 1).empty();
                    return seen;
                },
                StepWait(std::max(c.timeout_ms, 100), options_.cancel), nav_.Clock());
            return seen;
        }
        case ConditionKind::kValue: {
            auto it = ctx.result.values.find(c.key);
            if (it == ctx.result.values.end()) return false;
            const long long v = it->second;
            switch (c.op) {
                case CompareOp::kEquals: return v == c.number;
                case CompareOp::kNotEquals: return v != c.number;
                case CompareOp::kLess: return v < c.number;
                case CompareOp::kLessEqual: return v <= c.number;
                case CompareOp::kGreater: return v > c.number;
                case CompareOp::kGreaterEqual: return v >= c.number;
                default: return false;
            }
        }
    }
    return false;
}

bool TaskRunner::CheckPrecondition(const Step& s, Context& ctx, std::string& error) {
    if (!s.on_page.empty()) {
        bool reached = false;
        nav_.ObserveUntil([&](const Observation& o) { return o.page == s.on_page; },
                          StepWait(s.timeout_ms, options_.cancel), reached);
        if (!reached) {
            error = "not on page '" + s.on_page + "', nothing clicked";
            return false;
        }
    }
    if (s.require) {
        const TemplateRef ref = Substitute(*s.require, ctx.each_value);
        if (!nav_.WaitForTemplate(ref, StepWait(s.timeout_ms, options_.cancel))) {
            error = ref.file + " is not visible, nothing clicked";
            return false;
        }
    }
    return true;
}

bool TaskRunner::WaitReaction(const Frame& before, const Step& s) {
    const ReactionOptions& o = options_.reaction;
    IController& controller = nav_.Controller();
    const double needed = s.change_roi ? o.changed_share_roi : o.changed_share;
    const milliseconds timeout{s.timeout_ms};
    const auto start = nav_.Clock().Now();
    const auto attempts = timeout.count() / std::max<long long>(o.interval.count(), 1) + 1;
    for (long long i = 0; i < attempts && nav_.Clock().Now() - start < timeout; ++i) {
        if (Cancelled()) return false;
        nav_.Clock().Sleep(o.interval);
        const Frame current = controller.Screenshot();
        if (!current.empty() && ChangedShare(before, current, s.change_roi) >= needed) return true;
    }
    return false;
}

void TaskRunner::Settle() {
    if (!options_.reaction.enabled) return;
    if (!nav_.WaitSettled(nav_.Settings().settle.timeout))
        Log(log_, LogLevel::kDebug, "the screen kept moving, going on");
}

// Runs `act` (a click or back press) and makes sure the screen reacted: `expect` appeared and,
// with the reaction check on, the screen changed. Retried `retries` times, because the game
// ignores input while it animates; a step that never gets a reaction fails instead of carrying
// on from the wrong screen.
bool TaskRunner::CheckedAction(const Step& s, Context& ctx, const std::function<bool(std::string&)>& act,
                               const std::string& what, std::string& error) {
    IController& controller = nav_.Controller();
    const bool check = options_.reaction.enabled && !s.no_verify;
    std::optional<TemplateRef> expect;
    if (s.expect) expect = Substitute(*s.expect, ctx.each_value);
    const int extra = check || expect ? s.retries : 0;
    for (int attempt = 0; attempt <= extra; ++attempt) {
        if (attempt > 0) Log(log_, LogLevel::kInfo, "no reaction to " + what + ", trying again");
        Frame before;
        bool expected_already = false;
        if (check) {
            before = controller.Screenshot();
            // Visible before the click proves nothing: then the screen has to change as well.
            if (expect && !before.empty()) expected_already = nav_.Matcher().Find(before, *expect).found;
        }
        if (!act(error)) return false;
        if (!check && !expect) return true;
        bool reacted = true;
        if (expect) reacted = nav_.WaitForTemplate(*expect, StepWait(s.timeout_ms, options_.cancel)).has_value();
        if (reacted && check && (!expect || expected_already)) reacted = WaitReaction(before, s);
        if (reacted) {
            Settle();
            return true;
        }
        if (Cancelled()) return false;
    }
    error = expect ? expect->file + " did not appear after " + what : "no reaction to " + what;
    return false;
}

bool TaskRunner::ClickTemplate(const Step& s, Context& ctx, std::string& error) {
    IController& controller = nav_.Controller();
    const TemplateRef ref = Substitute(*s.templ, ctx.each_value);
    const auto m = nav_.WaitForTemplate(ref, StepWait(s.timeout_ms, options_.cancel));
    if (!m) {
        if (s.action == StepAction::kClickIfPresent) {
            Log(log_, LogLevel::kDebug, ref.file + " not present, skipped");
            return true;
        }
        error = ref.file + " not found";
        return false;
    }
    Point target = m->center;
    if (s.pick != Pick::kBest) {
        const auto all = nav_.Matcher().FindAll(nav_.LastFrame(), ref, 20);
        const MatchResult* chosen = nullptr;
        for (const auto& r : all) {
            if (!r.found) continue;
            const bool better =
                !chosen || (s.pick == Pick::kTop && r.box.y < chosen->box.y) ||
                (s.pick == Pick::kBottom && r.box.y > chosen->box.y) ||
                (s.pick == Pick::kLeft && r.box.x < chosen->box.x) ||
                (s.pick == Pick::kRight && r.box.x > chosen->box.x);
            if (better) chosen = &r;
        }
        if (chosen) target = chosen->center;
    }
    target = Offset(target, s.offset);
    return CheckedAction(
        s, ctx,
        [&](std::string& why) {
            if (controller.Click(target)) return true;
            why = Failed("click", controller);
            return false;
        },
        "clicking " + ref.file, error);
}

bool TaskRunner::RunStep(const Step& s, Context& ctx, std::string& error) {
    IController& controller = nav_.Controller();
    TaskRunResult& result = ctx.result;
    switch (s.action) {
        case StepAction::kGoto: {
            const NavResult r = nav_.GoTo(s.page);
            if (!r.ok) error = r.error;
            return r.ok;
        }
        case StepAction::kReturnHome: {
            const NavResult r = nav_.ReturnHome();
            if (!r.ok) error = r.error;
            return r.ok;
        }
        case StepAction::kWaitPage: {
            bool reached = false;
            nav_.ObserveUntil([&](const Observation& o) { return o.page == s.page; },
                              StepWait(s.timeout_ms, options_.cancel), reached);
            if (!reached) error = "page '" + s.page + "' did not appear";
            return reached;
        }
        case StepAction::kClick:
        case StepAction::kClickIfPresent: {
            if (!CheckPrecondition(s, ctx, error)) return false;
            if (s.templ) return ClickTemplate(s, ctx, error);
            Point target = s.point ? *s.point
                                   : s.points[static_cast<std::size_t>(ctx.loop_index.empty() ? 0 : ctx.loop_index.back()) %
                                              s.points.size()];
            target = Offset(target, s.offset);
            return CheckedAction(
                s, ctx,
                [&](std::string& why) {
                    if (controller.Click(target)) return true;
                    why = Failed("click", controller);
                    return false;
                },
                "the click at " + std::to_string(target.x) + "," + std::to_string(target.y), error);
        }
        case StepAction::kWaitTemplate: {
            const TemplateRef ref = Substitute(*s.templ, ctx.each_value);
            const auto m = nav_.WaitForTemplate(ref, StepWait(s.timeout_ms, options_.cancel));
            if (!m) error = ref.file + " did not appear";
            return m.has_value();
        }
        case StepAction::kBack:
            return CheckedAction(
                s, ctx,
                [&](std::string& why) {
                    if (controller.Back()) return true;
                    why = Failed("back", controller);
                    return false;
                },
                "the back key", error);
        case StepAction::kSleep:
            nav_.Clock().Sleep(milliseconds{s.sleep_ms});
            return true;
        case StepAction::kWaitStable:
            if (!nav_.WaitSettled(milliseconds{s.timeout_ms}))
                Log(log_, LogLevel::kDebug, "the screen kept moving, going on");
            return true;
        case StepAction::kReadNumber: {
            if (!ocr_) {
                error = "OCR is not available (start the OCR sidecar)";
                return false;
            }
            // Settle popups first: a result screen over the number would hide it.
            if (const Observation obs = nav_.Observe(); !obs.ok) {
                error = obs.error.empty() ? "screen capture failed" : obs.error;
                return false;
            }
            const Frame crop = controller.CaptureRegion(*s.roi);
            if (crop.empty()) {
                error = "region capture failed";
                return false;
            }
            // Numbers only: measured on 1920x1080 crops, the margin fixes them but disturbs names.
            const auto pieces = ocr_->Recognize(AddMargin(crop, kNumberCropMargin), Rect{}, options_.language);
            const std::string text = s.lenient ? TrailingNumber(JoinLeftToRight(pieces))
                                               : JoinLeftToRight(pieces);
            long long value = 0;
            if (const auto frac = ParseFraction(text)) {
                value = frac->value;
                result.values[s.store + "_max"] = frac->max;
            } else if (const auto n = ParseInteger(text)) {
                value = *n;
            } else {
                error = "could not read a number from '" + text + "'";
                if (!ocr_->LastError().empty()) error += ": " + ocr_->LastError();
                return false;
            }
            result.values[s.store] = value;
            Log(log_, LogLevel::kInfo, s.store + " = " + std::to_string(value));
            return true;
        }
        case StepAction::kClickColor: {
            // A blob that is still there after a click is tried once more at `retry_offset`
            // (when given), then left alone: the click did not reach its target.
            struct Spot {
                Point at;
                int tries;
            };
            std::vector<Spot> spots;
            const int max_tries = s.retry_offset ? 2 : 1;
            int clicks = 0;
            for (; clicks < s.max && !Cancelled(); ++clicks) {
                const Observation obs = nav_.Observe();
                if (!obs.ok) {
                    error = obs.error.empty() ? "screen capture failed" : obs.error;
                    return false;
                }
                Spot* target = nullptr;
                Point center;
                for (const auto& blob : FindColorBlobs(obs.frame, *s.color, 32)) {
                    center = Center(blob);
                    auto it = std::find_if(spots.begin(), spots.end(), [&](const Spot& sp) {
                        return std::abs(sp.at.x - center.x) <= 25 && std::abs(sp.at.y - center.y) <= 25;
                    });
                    if (it == spots.end()) {
                        spots.push_back({center, 0});
                        target = &spots.back();
                        break;
                    }
                    if (it->tries < max_tries) {
                        target = &*it;
                        break;
                    }
                }
                if (!target) break;
                const Point offset = target->tries == 0 ? s.offset : *s.retry_offset;
                ++target->tries;
                if (!controller.Click(Offset(center, offset))) {
                    error = Failed("click", controller);
                    return false;
                }
                if (s.sleep_ms > 0) nav_.Clock().Sleep(milliseconds{s.sleep_ms});
            }
            const auto missed = std::count_if(spots.begin(), spots.end(), [&](const Spot& sp) { return sp.tries >= max_tries && max_tries > 1; });
            Log(log_, LogLevel::kInfo, "clicked " + std::to_string(clicks) + " colour marker(s)" +
                                           (missed ? ", " + std::to_string(missed) + " needed a second try" : ""));
            return true;
        }
        case StepAction::kSave: {
            if (options_.data_dir.empty()) {
                error = "no data directory to save to";
                return false;
            }
            std::string name = s.file;
            for (const auto& [key, value] : result.values) {
                const std::string tag = "{" + key + "}";
                for (auto pos = name.find(tag); pos != std::string::npos; pos = name.find(tag))
                    name.replace(pos, tag.size(), std::to_string(value));
            }
            if (name.find('{') != std::string::npos) {
                error = "record name '" + name + "' needs a value that was not read";
                return false;
            }
            std::string why;
            if (!SaveRecord(options_.data_dir / PathFromUtf8(name), s, result, why)) {
                error = why;
                return false;
            }
            Log(log_, LogLevel::kInfo, "saved " + name);
            return true;
        }
        case StepAction::kClickText: {
            if (!ocr_) {
                error = "OCR is not available (start the OCR sidecar)";
                return false;
            }
            std::vector<std::string> wanted = s.texts;
            if (!s.text_setting.empty()) {
                auto it = ctx.settings.find(s.text_setting);
                if (it != ctx.settings.end())
                    wanted = it->second.list.empty() && !it->second.scalar.empty()
                                 ? std::vector<std::string>{it->second.scalar}  // a choice setting
                                 : it->second.list;
            }
            if (!s.text_key.empty()) {
                std::string key = s.text_key;
                if (const auto pos = key.find("{value}"); pos != std::string::npos) key.replace(pos, 7, ctx.each_value);
                auto it = options_.strings.find(key);
                if (it == options_.strings.end()) {
                    error = "no text '" + key + "' in strings/" + options_.language + ".json";
                    return false;
                }
                wanted = {it->second};
            }
            if (wanted.empty()) {
                error = "no text to look for";
                return false;
            }
            const auto normalize = [](std::string t) {
                t.erase(std::remove(t.begin(), t.end(), ' '), t.end());
                return t;
            };
            for (int page = 0; page <= s.max_scrolls && !Cancelled(); ++page) {
                if (page > 0) {
                    if (!controller.Swipe(*s.point, s.to, s.duration_ms)) {
                        error = Failed("swipe", controller);
                        return false;
                    }
                    nav_.Clock().Sleep(milliseconds{800});
                }
                if (const Observation obs = nav_.Observe(); !obs.ok) {
                    error = obs.error.empty() ? "screen capture failed" : obs.error;
                    return false;
                }
                const Frame crop = controller.CaptureRegion(*s.roi);
                if (crop.empty()) {
                    error = "region capture failed";
                    return false;
                }
                const auto pieces = ocr_->Recognize(crop, Rect{}, options_.language);
                for (const auto& want : wanted) {
                    for (const auto& p : pieces) {
                        if (normalize(p.text) != normalize(want)) continue;
                        const double sx = static_cast<double>(s.roi->width) / crop.width;
                        const double sy = static_cast<double>(s.roi->height) / crop.height;
                        Point at{s.roi->x + static_cast<int>((p.box.x + p.box.width / 2.0) * sx),
                                 s.roi->y + static_cast<int>((p.box.y + p.box.height / 2.0) * sy)};
                        if (s.click_x) at.x = *s.click_x;
                        Log(log_, LogLevel::kInfo, "found '" + want + "' on page " + std::to_string(page + 1));
                        if (!controller.Click(Offset(at, s.offset))) {
                            error = Failed("click", controller);
                            return false;
                        }
                        Settle();
                        return true;
                    }
                }
            }
            error = "none of the texts was found";
            return false;
        }
        case StepAction::kWheel:
            if (!controller.Scroll(*s.point, s.notches)) {
                error = Failed("wheel", controller);
                return false;
            }
            Settle();
            return true;
        case StepAction::kSwipe:
            if (!controller.Swipe(*s.point, s.to, s.duration_ms)) {
                error = Failed("swipe", controller);
                return false;
            }
            Settle();
            return true;
        default:
            break;
    }
    error = "unknown action";
    return false;
}

TaskRunner::Flow TaskRunner::RunAction(const Step& s, Context& ctx, std::string& error) {
    switch (s.action) {
        case StepAction::kIf:
            return RunSteps(Holds(*s.when, ctx) ? s.steps : s.else_steps, ctx);
        case StepAction::kLoop: {
            int count = s.max;
            if (!s.max_setting.empty()) {
                auto it = ctx.settings.find(s.max_setting);
                count = it == ctx.settings.end() ? 0 : std::atoi(it->second.scalar.c_str());
            } else if (!s.max_value.empty()) {
                auto it = ctx.result.values.find(s.max_value);
                count = it == ctx.result.values.end() ? 0 : static_cast<int>(it->second);
            }
            count = std::clamp(count + s.max_adjust, 0, kMaxLoop);
            ctx.loop_index.push_back(0);
            Flow flow = Flow::kNext;
            for (int i = 0; i < count; ++i) {
                ctx.loop_index.back() = i;
                if (Cancelled()) {
                    flow = Flow::kCancel;
                    break;
                }
                if (s.until && Holds(*s.until, ctx)) break;
                flow = RunSteps(s.steps, ctx);
                if (flow == Flow::kBreak) {
                    flow = Flow::kNext;
                    break;
                }
                if (flow != Flow::kNext) break;
            }
            ctx.loop_index.pop_back();
            return flow;
        }
        case StepAction::kEach: {
            auto it = ctx.settings.find(s.setting);
            const std::vector<std::string> values =
                it == ctx.settings.end() ? std::vector<std::string>{} : it->second.list;
            const std::string saved = ctx.each_value;
            Flow flow = Flow::kNext;
            for (const auto& value : values) {
                ctx.each_value = value;
                for (const auto& [key, body] : s.cases) {
                    if (key == value) {
                        flow = RunSteps(body, ctx);
                        break;
                    }
                }
                if (flow == Flow::kNext && !s.steps.empty()) flow = RunSteps(s.steps, ctx);
                if (flow == Flow::kBreak) flow = Flow::kNext;  // break leaves this value only
                if (flow != Flow::kNext) break;
            }
            ctx.each_value = saved;
            return flow;
        }
        case StepAction::kEnd:
            ctx.result.status = s.end_skipped ? TaskStatus::kSkipped : TaskStatus::kSucceeded;
            ctx.result.message = s.message;
            return Flow::kEnd;
        case StepAction::kFail:
            error = s.message;
            return Flow::kFail;
        case StepAction::kBreak:
            return Flow::kBreak;
        default:
            return RunStep(s, ctx, error) ? Flow::kNext : Flow::kFail;
    }
}

TaskRunner::Flow TaskRunner::RunSteps(const std::vector<Step>& steps, Context& ctx) {
    for (std::size_t i = 0; i < steps.size(); ++i) {
        const Step& s = steps[i];
        if (Cancelled()) return Flow::kCancel;
        if (s.action != StepAction::kIf && s.when && !Holds(*s.when, ctx)) continue;
        Log(log_, LogLevel::kDebug,
            std::string(ToString(s.action)) + (s.templ ? " " + Substitute(*s.templ, ctx.each_value).file : "") +
                (s.page.empty() ? "" : " " + s.page));
        for (int rep = 0; rep < s.repeat; ++rep) {
            if (Cancelled()) return Flow::kCancel;
            std::string error;
            const Flow flow = RunAction(s, ctx, error);
            if (Cancelled()) return Flow::kCancel;
            if (flow == Flow::kFail) {
                // A nested step already described the failure; keep its description.
                const std::string where =
                    !ctx.error.empty()
                        ? ctx.error
                        : "step " + std::to_string(i + 1) + " (" + ToString(s.action) + "): " + error;
                if (s.optional) {
                    Log(log_, s.quiet ? LogLevel::kInfo : LogLevel::kWarn, ctx.task.id + ": optional " + where);
                    if (!s.quiet) ctx.skipped.push_back(where);
                    ctx.error.clear();
                    break;  // stop repeating this step, continue with the next one
                }
                ctx.error = where;
                return flow;
            }
            if (flow != Flow::kNext) return flow;
            if (s.action != StepAction::kSleep && s.sleep_ms > 0 &&
                s.action != StepAction::kClickColor)
                nav_.Clock().Sleep(milliseconds{s.sleep_ms});
        }
    }
    return Flow::kNext;
}

TaskRunResult TaskRunner::RunTask(const TaskSpec& task) {
    TaskRunResult result;
    result.id = task.id;
    if (task.steps.empty()) {
        result.status = TaskStatus::kSkipped;
        result.message = "not implemented yet";
        return result;
    }
    const auto found = options_.settings.find(task.id);
    const TaskSettings settings = found != options_.settings.end() ? found->second : DefaultSettings(task);
    Context ctx{task, settings, result};
    switch (RunSteps(task.steps, ctx)) {
        case Flow::kNext:
        case Flow::kBreak:
            result.status = TaskStatus::kSucceeded;
            break;
        case Flow::kEnd:
            break;  // status and message set by the end step
        case Flow::kFail:
            result.status = TaskStatus::kFailed;
            result.message = ctx.error;
            // The screen at the failure, like a navigation failure (saved under captures/).
            if (nav_.Options().on_failure) nav_.Options().on_failure(nav_.LastFrame(), task.id + ": " + ctx.error);
            break;
        case Flow::kCancel:
            result.status = TaskStatus::kCancelled;
            result.message = "cancelled";
            break;
    }
    // "Succeeded" must not hide a step that did nothing, like a sweep that never started.
    if (result.status == TaskStatus::kSucceeded && !ctx.skipped.empty()) {
        const std::string note = std::to_string(ctx.skipped.size()) + " step(s) did not work: " + ctx.skipped.front();
        result.message = result.message.empty() ? note : result.message + "; " + note;
    }
    return result;
}

RunReport TaskRunner::Run(const std::vector<TaskSpec>& tasks,
                          const std::vector<std::string>& selected) {
    RunReport report;
    std::vector<TaskDef> defs;
    std::map<std::string, const TaskSpec*> by_id;
    for (const auto& t : tasks) {
        defs.push_back(t.Order());
        by_id[t.id] = &t;
    }
    const OrderResult order = ResolveOrder(defs);
    if (!order.ok) {
        report.error = order.error;
        return report;
    }
    const std::set<std::string> wanted(selected.begin(), selected.end());
    for (const auto& id : wanted) {
        if (!by_id.count(id)) {
            report.error = "unknown task: " + id;
            return report;
        }
    }

    // Start-up screens first (title, event splash): tap through to a known page once per run.
    // A failure is only logged; the first task's return home then reports it.
    const bool any_steps = std::any_of(wanted.begin(), wanted.end(),
                                       [&](const std::string& id) { return !by_id.at(id)->steps.empty(); });
    if (options_.return_home_between_tasks && any_steps && !Cancelled()) {
        const NavResult entered = nav_.EnterGame();
        if (!entered.ok && !entered.cancelled)
            Log(log_, LogLevel::kWarn, "could not enter the game: " + entered.error);
    }

    // Selected tasks that ran and did not pass. "Not implemented" skips do not block dependents:
    // they did nothing, so the game state is as if they were not selected.
    std::set<std::string> blocked;
    report.ok = true;
    for (const auto& id : order.order) {
        if (!wanted.count(id)) continue;
        const TaskSpec& task = *by_id.at(id);
        TaskRunResult r;
        r.id = id;

        if (report.cancelled || Cancelled()) {
            report.cancelled = true;
            r.status = TaskStatus::kCancelled;
            r.message = "cancelled";
        } else {
            // A selected dependency that did not succeed blocks this task.
            for (const auto& dep : task.after) {
                if (blocked.count(dep)) {
                    r.status = TaskStatus::kSkipped;
                    r.message = "dependency '" + dep + "' did not succeed";
                    break;
                }
            }
        }

        if (r.status == TaskStatus::kPending && task.steps.empty()) {
            r = RunTask(task);  // skipped: not implemented
        } else if (r.status == TaskStatus::kPending) {
            r.status = TaskStatus::kRunning;
            Notify(r);
            Log(log_, LogLevel::kInfo, "task '" + id + "' started");
            if (options_.return_home_between_tasks && !task.steps.empty()) {
                const NavResult home = nav_.ReturnHome();
                if (!home.ok) {
                    r.status = home.cancelled ? TaskStatus::kCancelled : TaskStatus::kFailed;
                    r.message = "could not return home: " + home.error;
                }
            }
            if (r.status == TaskStatus::kRunning) r = RunTask(task);
            if (r.status == TaskStatus::kFailed && options_.return_home_between_tasks &&
                !Cancelled()) {
                const NavResult home = nav_.ReturnHome();  // recovery for the next task
                if (!home.ok) Log(log_, LogLevel::kWarn, "recovery failed: " + home.error);
            }
        }

        if (r.status == TaskStatus::kCancelled) report.cancelled = true;
        // "not implemented" skips do not fail the run; dependency skips and failures do.
        const bool passed = r.status == TaskStatus::kSucceeded ||
                            (r.status == TaskStatus::kSkipped && task.steps.empty());
        if (!passed) {
            report.ok = false;
            blocked.insert(id);
        }
        Log(log_, r.status == TaskStatus::kFailed ? LogLevel::kWarn : LogLevel::kInfo,
            "task '" + id + "' " + ToString(r.status) + (r.message.empty() ? "" : ": " + r.message));
        Notify(r);
        report.tasks.push_back(std::move(r));
    }
    return report;
}

}  // namespace maba
