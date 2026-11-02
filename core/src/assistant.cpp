// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#include "maba/assistant.h"

#include <system_error>

#if defined(MABA_WITH_OPENCV)
#include "maba/vision.h"
#endif

namespace maba {

namespace fs = std::filesystem;

std::vector<std::string> DefaultOcrCommand(const fs::path& root) {
    std::error_code ec;
#ifdef _WIN32
    const fs::path venv = root / "ocr" / ".venv" / "Scripts" / "python.exe";
#else
    const fs::path venv = root / "ocr" / ".venv" / "bin" / "python";
#endif
    const std::string python = fs::exists(venv, ec) ? venv.u8string() : std::string("python");
    return {python, (root / "ocr" / "server.py").u8string()};
}

Assistant::Assistant(AssistantConfig config, LogFn log)
    : config_(std::move(config)), log_(std::move(log)) {}

bool Assistant::Load(std::string& error) {
    const auto pages = LoadPageDir(config_.root / "pages");
    if (!pages.ok) {
        error = "pages: " + pages.error;
        return false;
    }
    const auto popups = LoadPopupDir(config_.root / "popups");
    if (!popups.ok) {
        error = "popups: " + popups.error;
        return false;
    }
    const auto tasks = LoadTaskDir(config_.root / "tasks");
    if (!tasks.ok) {
        error = "tasks: " + tasks.error;
        return false;
    }
    const auto languages = LoadLanguages(config_.root / "config" / "languages.json");
    if (!languages.ok) {
        error = "languages: " + languages.error;
        return false;
    }
    if (!languages.config.Find(config_.language)) {
        error = "language '" + config_.language + "' is not in config/languages.json";
        return false;
    }
    std::vector<TaskDef> defs;
    for (const auto& t : tasks.tasks) defs.push_back(t.Order());
    const OrderResult order = ResolveOrder(defs);
    if (!order.ok) {
        error = "tasks: " + order.error;
        return false;
    }

    pages_ = pages.pages;
    popups_ = popups.popups;
    tasks_ = tasks.tasks;
    languages_ = languages.config;
    for (const auto& p : ValidatePageGraph(pages_, config_.navigator.home))
        Log(log_, LogLevel::kWarn, "data: " + p);
    for (const auto& p : ValidateTasks(tasks_, pages_)) Log(log_, LogLevel::kWarn, "data: " + p);
    Log(log_, LogLevel::kInfo, "loaded " + std::to_string(pages_.size()) + " pages, " +
                                   std::to_string(popups_.size()) + " popups, " +
                                   std::to_string(tasks_.size()) + " tasks");
    return true;
}

RunReport Assistant::Run(const std::vector<std::string>& selected, const std::atomic<bool>* cancel,
                         std::function<void(const TaskRunResult&)> on_task,
                         std::function<void(const Frame&, const std::string&)> on_failure) {
    RunReport report;
    if (!controller_) {
        report.error = "not connected";
        return report;
    }
    if (!matcher_) {
#if defined(MABA_WITH_OPENCV)
        matcher_ = std::make_unique<OpenCvMatcher>(config_.root, config_.language);
#else
        report.error = "this build has no template matcher (built without OpenCV)";
        return report;
#endif
    }
    SteadyClock clock;
    PopupOptions popup_options = config_.popups;
    popup_options.cancel = cancel;
    PopupHandler popups(*matcher_, popups_, popup_options, log_);
    NavigatorOptions nav_options = config_.navigator;
    nav_options.cancel = cancel;
    nav_options.on_failure = on_failure;
    Navigator navigator(*controller_, *matcher_, pages_, popups, clock, nav_options, log_);
    RunnerOptions runner_options;
    runner_options.language = config_.language;
    runner_options.cancel = cancel;
    runner_options.on_task = std::move(on_task);
    TaskRunner runner(navigator, ocr_.get(), runner_options, log_);
    return runner.Run(tasks_, selected);
}

}  // namespace maba
