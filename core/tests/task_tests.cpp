// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Task definitions and the task runner.
#include <atomic>
#include <string>
#include <vector>

#include "fakes.h"
#include "maba/task.h"
#include "maba/task_runner.h"
#include "nav_rig.h"
#include "test_framework.h"

namespace {

using maba_test::NavRig;

maba::TaskSpec Task(const char* json) {
    const auto t = maba::ParseTask(json);
    if (!t.ok) std::printf("task parse error: %s\n", t.error.c_str());
    CHECK(t.ok);
    return t.task;
}

MABA_TEST(TaskParsesSteps) {
    const auto t = maba::ParseTask(R"({
      "id": "mail_claim_all", "after": ["spend_ap"], "needs_ocr": false,
      "steps": [
        {"action": "goto", "page": "mail"},
        {"action": "click", "template": "mail_claim.png", "timeout_ms": 3000},
        {"action": "click_if_present", "template": "ok.png", "optional": true, "repeat": 3},
        {"action": "click", "x": 10, "y": 20},
        {"action": "sleep", "sleep_ms": 500},
        {"action": "read_number", "roi": {"x": 1, "y": 2, "w": 30, "h": 10}, "store": "ap"},
        {"action": "back"},
        {"action": "return_home"}
      ]})");
    CHECK(t.ok);
    CHECK(t.task.steps.size() == 8);
    CHECK(t.task.name == "mail_claim_all");
    CHECK(t.task.steps[1].timeout_ms == 3000);
    CHECK(t.task.steps[2].repeat == 3 && t.task.steps[2].optional);
    CHECK(t.task.steps[3].point.has_value());
    CHECK(t.task.steps[5].store == "ap");
}

MABA_TEST(TaskRejectsBadSteps) {
    CHECK(!maba::ParseTask(R"({"id": "a", "steps": [{"action": "fly"}]})").ok);
    CHECK(!maba::ParseTask(R"({"id": "a", "steps": [{"action": "goto"}]})").ok);
    CHECK(!maba::ParseTask(R"({"id": "a", "steps": [{"action": "sleep", "sleep_ms": 999999}]})").ok);
    CHECK(!maba::ParseTask(R"({"id": "a", "steps": [{"action": "click", "template": "x.png", "repeat": 0}]})").ok);
    CHECK(!maba::ParseTask(R"({"id": "a", "steps": [{"action": "read_number", "store": "ap"}]})").ok);
    CHECK(!maba::ParseTask(R"({"id": "a", "after": "b"})").ok);
}

MABA_TEST(ValidateTasksFindsUnknownPagesAndDeps) {
    NavRig rig;
    std::vector<maba::TaskSpec> tasks = {
        Task(R"({"id": "a", "after": ["zzz"], "steps": [{"action": "goto", "page": "nowhere"}]})")};
    CHECK(maba::ValidateTasks(tasks, rig.pages).size() == 2);
}

MABA_TEST(RunTaskClaimsMailAndReadsNumber) {
    NavRig rig;
    maba_test::FakeOcr ocr;
    ocr.results = {{"0/240", {20, 0, 30, 10}, 0.9f}, {"12", {0, 0, 20, 10}, 0.9f}};
    maba::TaskRunner runner(rig.Nav(), &ocr);
    const auto task = Task(R"({"id": "mail_claim_all", "steps": [
        {"action": "goto", "page": "mail"},
        {"action": "click", "template": "mail_claim.png"},
        {"action": "read_number", "roi": {"x": 1, "y": 2, "w": 30, "h": 10}, "store": "ap"}]})");
    const maba::TaskRunResult r = runner.RunTask(task);
    CHECK(r.status == maba::TaskStatus::kSucceeded);
    CHECK(rig.controller.clicks.size() == 2);
    CHECK(rig.controller.clicks.size() == 2 && rig.controller.clicks[1].x == 1050);
    CHECK(r.values.count("ap") && r.values.at("ap") == 120);
    CHECK(r.values.count("ap_max") && r.values.at("ap_max") == 240);
    CHECK(ocr.last_language == "ko");
}

MABA_TEST(RunTaskFailsWithStepInfo) {
    NavRig rig;
    maba::TaskRunner runner(rig.Nav(), nullptr);
    const auto task = Task(R"({"id": "t", "steps": [
        {"action": "goto", "page": "mail"},
        {"action": "click", "template": "shop_title.png", "timeout_ms": 1000}]})");
    const maba::TaskRunResult r = runner.RunTask(task);
    CHECK(r.status == maba::TaskStatus::kFailed);
    CHECK(r.message.find("step 2 (click)") != std::string::npos);
}

MABA_TEST(OptionalAndIfPresentStepsDoNotFail) {
    NavRig rig;
    maba::TaskRunner runner(rig.Nav(), nullptr);
    const auto task = Task(R"({"id": "t", "steps": [
        {"action": "click_if_present", "template": "shop_title.png", "timeout_ms": 500},
        {"action": "read_number", "roi": {"x": 1, "y": 2, "w": 30, "h": 10}, "store": "ap", "optional": true},
        {"action": "wait_page", "page": "main_menu"}]})");
    const maba::TaskRunResult r = runner.RunTask(task);
    CHECK(r.status == maba::TaskStatus::kSucceeded);
    CHECK(rig.controller.clicks.empty());
}

MABA_TEST(RunOrdersSkipsAndBlocksDependents) {
    NavRig rig;
    std::vector<maba::TaskRunResult> events;
    maba::RunnerOptions options;
    options.on_task = [&](const maba::TaskRunResult& r) { events.push_back(r); };
    maba::TaskRunner runner(rig.Nav(), nullptr, options);
    const std::vector<maba::TaskSpec> tasks = {
        Task(R"({"id": "mission", "after": ["mail"], "steps": [{"action": "goto", "page": "shop"}]})"),
        Task(R"({"id": "mail", "after": ["spend_ap"], "steps": [{"action": "goto", "page": "mail"}]})"),
        Task(R"({"id": "spend_ap", "steps": [{"action": "click", "template": "missing.png", "timeout_ms": 500}]})"),
        Task(R"({"id": "cafe"})"),
    };
    const maba::RunReport report = runner.Run(tasks, {"mission", "mail", "spend_ap", "cafe"});
    CHECK(report.error.empty());
    CHECK(!report.ok);
    CHECK(report.tasks.size() == 4);
    if (report.tasks.size() == 4) {
        CHECK(report.tasks[0].id == "spend_ap" && report.tasks[0].status == maba::TaskStatus::kFailed);
        CHECK(report.tasks[1].id == "mail" && report.tasks[1].status == maba::TaskStatus::kSkipped);
        CHECK(report.tasks[2].id == "mission" && report.tasks[2].status == maba::TaskStatus::kSkipped);
        CHECK(report.tasks[3].id == "cafe" && report.tasks[3].message == "not implemented yet");
    }
    CHECK(events.size() == 5);  // spend_ap running + 4 results
}

MABA_TEST(NotImplementedDependencyDoesNotBlock) {
    NavRig rig;
    maba::TaskRunner runner(rig.Nav(), nullptr);
    const std::vector<maba::TaskSpec> tasks = {
        Task(R"({"id": "spend_ap"})"),
        Task(R"({"id": "mail", "after": ["spend_ap"], "steps": [{"action": "goto", "page": "mail"}]})"),
    };
    const maba::RunReport report = runner.Run(tasks, {"mail", "spend_ap"});
    CHECK(report.ok);
    CHECK(report.tasks.size() == 2 && report.tasks[1].status == maba::TaskStatus::kSucceeded);
}

MABA_TEST(RunRejectsUnknownTaskAndCycles) {
    NavRig rig;
    maba::TaskRunner runner(rig.Nav(), nullptr);
    CHECK(!runner.Run({Task(R"({"id": "a"})")}, {"b"}).error.empty());
    const std::vector<maba::TaskSpec> cyclic = {Task(R"({"id": "a", "after": ["b"]})"),
                                                Task(R"({"id": "b", "after": ["a"]})")};
    CHECK(!runner.Run(cyclic, {"a"}).error.empty());
}

MABA_TEST(RunCancelledMarksRemainingTasks) {
    NavRig rig;
    std::atomic<bool> cancel{true};
    rig.nav_options.cancel = &cancel;
    maba::RunnerOptions options;
    options.cancel = &cancel;
    maba::TaskRunner runner(rig.Nav(), nullptr, options);
    const std::vector<maba::TaskSpec> tasks = {
        Task(R"({"id": "a", "steps": [{"action": "goto", "page": "mail"}]})"),
        Task(R"({"id": "b", "steps": [{"action": "goto", "page": "shop"}]})")};
    const maba::RunReport report = runner.Run(tasks, {"a", "b"});
    CHECK(report.cancelled);
    CHECK(!report.ok);
    CHECK(report.tasks.size() == 2 && report.tasks[1].status == maba::TaskStatus::kCancelled);
    CHECK(rig.controller.clicks.empty());
}

MABA_TEST(RepoTaskFilesLoad) {
    const auto set = maba::LoadTaskDir(std::string(MABA_SOURCE_DIR) + "/tasks");
    if (!set.ok) std::printf("%s\n", set.error.c_str());
    CHECK(set.ok);
    CHECK(set.tasks.size() >= 6);
    std::vector<maba::TaskDef> defs;
    for (const auto& t : set.tasks) defs.push_back(t.Order());
    CHECK(maba::ResolveOrder(defs).ok);
    const auto pages = maba::LoadPageDir(std::string(MABA_SOURCE_DIR) + "/pages");
    CHECK(pages.ok);
    const auto problems = maba::ValidateTasks(set.tasks, pages.pages);
    for (const auto& p : problems) std::printf("%s\n", p.c_str());
    CHECK(problems.empty());
    const auto popups = maba::LoadPopupDir(std::string(MABA_SOURCE_DIR) + "/popups");
    if (!popups.ok) std::printf("%s\n", popups.error.c_str());
    CHECK(popups.ok);
}

}  // namespace
