// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Composition root used by the C ABI and the CLI: loads the data (pages, popups, tasks,
// languages), owns the controller / matcher / OCR and runs tasks.
#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "maba/controller.h"
#include "maba/languages.h"
#include "maba/log.h"
#include "maba/matcher.h"
#include "maba/navigator.h"
#include "maba/ocr.h"
#include "maba/popup.h"
#include "maba/task.h"
#include "maba/strings.h"
#include "maba/task_runner.h"

namespace maba {

struct AssistantConfig {
    std::filesystem::path root = ".";  // repository / install root holding pages/, tasks/, ...
    std::string language = "ko";
    // Where save steps write per-account records (outside the repository: personal data).
    std::filesystem::path data_dir;
    NavigatorOptions navigator;
    PopupOptions popups;
};

// OCR sidecar command: ocr/.venv's Python if present, otherwise "python", running ocr/server.py.
std::vector<std::string> DefaultOcrCommand(const std::filesystem::path& root);

class Assistant {
public:
    explicit Assistant(AssistantConfig config, LogFn log = {});

    // Loads and validates data. Data problems that do not block a run are logged as warnings.
    bool Load(std::string& error);

    const std::vector<TaskSpec>& Tasks() const { return tasks_; }
    const std::map<std::string, PageDef>& Pages() const { return pages_; }
    const LanguageConfig& Languages() const { return languages_; }
    const StringTable& Strings() const { return strings_; }
    const AssistantConfig& Config() const { return config_; }

    void SetController(std::unique_ptr<IController> controller) { controller_ = std::move(controller); }
    void SetMatcher(std::unique_ptr<IMatcher> matcher) { matcher_ = std::move(matcher); }
    void SetOcr(std::unique_ptr<IOcr> ocr) { ocr_ = std::move(ocr); }
    IController* Controller() { return controller_.get(); }

    // Runs the selected tasks. Needs a controller; creates the OpenCV matcher when none is set
    // (fails without OpenCV). The OCR is optional (read_number steps fail without it).
    // `settings_json` overrides task settings: {"<task id>": {"<key>": value}} (see ResolveSettings).
    RunReport Run(const std::vector<std::string>& selected, const std::atomic<bool>* cancel,
                  std::function<void(const TaskRunResult&)> on_task = {},
                  std::function<void(const Frame&, const std::string&)> on_failure = {},
                  const std::string& settings_json = {});

private:
    AssistantConfig config_;
    LogFn log_;
    std::map<std::string, PageDef> pages_;
    std::vector<PopupDef> popups_;
    std::vector<TaskSpec> tasks_;
    LanguageConfig languages_;
    StringTable strings_;
    std::unique_ptr<IController> controller_;
    std::unique_ptr<IMatcher> matcher_;
    std::unique_ptr<IOcr> ocr_;
};

}  // namespace maba
