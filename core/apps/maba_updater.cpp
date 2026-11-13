// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
//
// maba_updater PLAN.json
//
// Replaces the program's files with an unpacked update and starts the program again
// (docs/notes/updates.md). The program starts a copy of this updater and exits; a running
// program cannot replace its own files. It never downloads anything: the program has checked
// and unpacked the update before.
#include <windows.h>
#include <shellapi.h>

#include <fstream>
#include <sstream>
#include <string>

#include "maba/update_apply.h"

namespace {

std::string ReadFile(const std::wstring& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

void WriteStatus(const maba::UpdatePlan& plan, bool ok, const std::string& error) {
    if (plan.status.empty()) return;
    std::ofstream out(plan.status, std::ios::binary | std::ios::trunc);
    out << maba::UpdateStatusJson(ok, plan.version, error);
}

// The program exits by itself after starting the updater; its files stay locked until then.
void WaitForProgram(unsigned long pid) {
    if (pid == 0) return;
    const HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (!process) return;  // already gone
    WaitForSingleObject(process, 60000);
    CloseHandle(process);
}

void Relaunch(const maba::UpdatePlan& plan) {
    if (plan.relaunch.empty()) return;
    const std::wstring exe = (plan.root / std::filesystem::path(std::u8string(plan.relaunch.begin(), plan.relaunch.end()))).wstring();
    std::wstring command = L"\"" + exe + L"\"";
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr, plan.root.wstring().c_str(), &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}

}  // namespace

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv || argc < 2) return 2;
    const std::wstring plan_path = argv[1];
    LocalFree(argv);

    maba::UpdatePlan plan;
    std::string error;
    if (!maba::ParseUpdatePlan(ReadFile(plan_path), plan, error)) return 2;

    WaitForProgram(plan.pid);
    const bool ok = maba::ApplyUpdate(plan, error);
    WriteStatus(plan, ok, error);
    DeleteFileW(plan_path.c_str());
    Relaunch(plan);  // the new program after a success, the old one after a failure
    return ok ? 0 : 1;
}
