// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

#include "maba/update_apply.h"
#include "maba/utf8.h"
#include "test_framework.h"

namespace {

namespace fs = std::filesystem;

struct Install {
    Install() {
        std::random_device rd;
        base = fs::temp_directory_path() / ("maba_update_" + std::to_string(rd()));
        plan.root = base / "app";
        plan.staging = base / "app" / "update" / "program-new";
        plan.backup = base / "app" / "update" / "program-old";
        fs::create_directories(plan.root);
    }
    ~Install() {
        std::error_code ec;
        fs::remove_all(base, ec);
    }
    static void Write(const fs::path& p, const std::string& text) {
        fs::create_directories(p.parent_path());
        std::ofstream(p, std::ios::binary) << text;
    }
    static std::string Read(const fs::path& p) {
        std::ifstream in(p, std::ios::binary);
        std::ostringstream out;
        out << in.rdbuf();
        return out.str();
    }
    fs::path base;
    maba::UpdatePlan plan;
};

}  // namespace

MABA_TEST(UpdateReplacesFilesAndFoldersAndKeepsTheOldOnes) {
    Install i;
    Install::Write(i.plan.root / "app.exe", "old exe");
    Install::Write(i.plan.root / "bin" / "core.dll", "old core");
    Install::Write(i.plan.root / "bin" / "gone.dll", "only in the old version");
    Install::Write(i.plan.root / "data" / "tasks" / "t.json", "resources stay");
    Install::Write(i.plan.staging / "app.exe", "new exe");
    Install::Write(i.plan.staging / "bin" / "core.dll", "new core");
    Install::Write(i.plan.staging / "data" / "tools" / "tool.py", "new tool");
    i.plan.entries = {"app.exe", "bin", "data/tools"};

    std::string error;
    CHECK(maba::ApplyUpdate(i.plan, error, 0, 1));
    CHECK(error.empty());
    CHECK(Install::Read(i.plan.root / "app.exe") == "new exe");
    CHECK(Install::Read(i.plan.root / "bin" / "core.dll") == "new core");
    CHECK(!fs::exists(i.plan.root / "bin" / "gone.dll"));  // the folder is replaced as a whole
    CHECK(Install::Read(i.plan.root / "data" / "tools" / "tool.py") == "new tool");
    CHECK(Install::Read(i.plan.root / "data" / "tasks" / "t.json") == "resources stay");
    CHECK(Install::Read(i.plan.backup / "app.exe") == "old exe");
    CHECK(Install::Read(i.plan.backup / "bin" / "gone.dll") == "only in the old version");
    CHECK(!fs::exists(i.plan.staging));
}

MABA_TEST(UpdateIsUndoneWhenAnEntryFails) {
    Install i;
    Install::Write(i.plan.root / "app.exe", "old exe");
    Install::Write(i.plan.root / "bin" / "core.dll", "old core");
    Install::Write(i.plan.staging / "app.exe", "new exe");
    Install::Write(i.plan.staging / "bin" / "core.dll", "new core");
    i.plan.entries = {"app.exe", "bin", "missing.txt"};
    std::string error;
    CHECK(!maba::ApplyUpdate(i.plan, error, 0, 1));  // refused before anything moves
    CHECK(error.find("missing.txt") != std::string::npos);
    CHECK(Install::Read(i.plan.root / "app.exe") == "old exe");

    // A target that cannot be replaced: a folder where the new entry is a file whose parent is a file.
    Install::Write(i.plan.staging / "docs" / "a.txt", "new doc");
    Install::Write(i.plan.root / "docs", "a file where a folder has to go");
    i.plan.entries = {"app.exe", "docs/a.txt"};
    error.clear();
    CHECK(!maba::ApplyUpdate(i.plan, error, 0, 1));
    CHECK(!error.empty());
    CHECK(Install::Read(i.plan.root / "app.exe") == "old exe");  // put back
    CHECK(Install::Read(i.plan.staging / "app.exe") == "new exe");
}

MABA_TEST(UpdatePlanIsCheckedWhenRead) {
    Install i;
    const auto text = [&](const nlohmann::json& entries, const std::string& relaunch = "app.exe") {
        return nlohmann::json{{"root", maba::PathToUtf8(i.plan.root)}, {"staging", maba::PathToUtf8(i.plan.staging)},
                              {"backup", maba::PathToUtf8(i.plan.backup)}, {"entries", entries}, {"relaunch", relaunch},
                              {"pid", 4321}, {"status", maba::PathToUtf8(i.base / "status.json")}, {"version", "0.1.5"}}
            .dump();
    };
    maba::UpdatePlan plan;
    std::string error;
    CHECK(maba::ParseUpdatePlan(text({"app.exe", "bin", "data/tools"}), plan, error));
    CHECK(plan.entries.size() == 3 && plan.pid == 4321 && plan.version == "0.1.5" && plan.relaunch == "app.exe");
    for (const char* bad : {"../evil", "bin/../../evil", "/abs", "C:/abs", "bin\\x", "", "."}) {
        maba::UpdatePlan p;
        CHECK(!maba::ParseUpdatePlan(text({bad}), p, error));
    }
    CHECK(!maba::ParseUpdatePlan(text({"app.exe"}, "../other.exe"), plan, error));
    CHECK(!maba::ParseUpdatePlan(text(nlohmann::json::array()), plan, error));
    CHECK(!maba::ParseUpdatePlan("not json", plan, error));
    CHECK(!maba::ParseUpdatePlan(R"({"root": "rel", "staging": "rel", "backup": "rel", "entries": ["a"]})", plan, error));
    const auto status = nlohmann::json::parse(maba::UpdateStatusJson(false, "0.1.5", "why"));
    CHECK(status["ok"] == false && status["version"] == "0.1.5" && status["error"] == "why");
}
