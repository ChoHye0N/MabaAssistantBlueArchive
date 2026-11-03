// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Child processes: one-shot commands (adb) and long-running line-based peers (OCR sidecar,
// touch helper). Interfaces, so controllers and the OCR bridge are tested with fakes.
// Arguments are passed as a vector and never through a shell.
#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace maba {

struct ProcessResult {
    bool started = false;
    bool timed_out = false;  // killed after the timeout
    int exit_code = -1;
    std::string out;         // stdout, raw bytes
    std::string err;         // stderr, raw bytes (truncated to a few KiB)
    std::string error;       // why it could not start

    bool ok() const { return started && !timed_out && exit_code == 0; }
};

class IProcessRunner {
public:
    virtual ~IProcessRunner() = default;
    // Runs argv[0] with the arguments, waits at most `timeout`, kills the process on timeout.
    virtual ProcessResult Run(const std::vector<std::string>& argv,
                              std::chrono::milliseconds timeout) = 0;
};

// A running child with line-based stdin/stdout.
class IInteractiveProcess {
public:
    virtual ~IInteractiveProcess() = default;
    // Writes `line` plus '\n'. False if the process has exited or the pipe broke.
    virtual bool WriteLine(const std::string& line) = 0;
    // Next stdout line without the line ending, or nullopt on timeout / end of stream.
    virtual std::optional<std::string> ReadLine(std::chrono::milliseconds timeout) = 0;
    virtual bool Running() = 0;
    virtual void Terminate() = 0;
};

class IProcessLauncher {
public:
    virtual ~IProcessLauncher() = default;
    // nullptr on failure, with the reason in `error`.
    virtual std::unique_ptr<IInteractiveProcess> Start(const std::vector<std::string>& argv,
                                                       std::string& error) = 0;
};

// Programs that keep running on their own (an emulator): started without pipes, outside any
// job, so they survive this process. Ids are OS process ids.
class IDetachedProcesses {
public:
    virtual ~IDetachedProcesses() = default;
    // Starts argv[0] in `working_dir` (empty: the current one). Returns the process id, or 0
    // with the reason in `error`.
    virtual long Start(const std::vector<std::string>& argv, const std::string& working_dir,
                       std::string& error) = 0;
    virtual bool Alive(long pid) = 0;
    // Ends the process and the processes it started.
    virtual bool KillTree(long pid, std::string& error) = 0;
};

// OS implementations (Win32 or POSIX).
class SystemDetachedProcesses : public IDetachedProcesses {
public:
    long Start(const std::vector<std::string>& argv, const std::string& working_dir,
               std::string& error) override;
    bool Alive(long pid) override;
    bool KillTree(long pid, std::string& error) override;
};

class SystemProcessRunner : public IProcessRunner {
public:
    ProcessResult Run(const std::vector<std::string>& argv,
                      std::chrono::milliseconds timeout) override;
};

class SystemProcessLauncher : public IProcessLauncher {
public:
    std::unique_ptr<IInteractiveProcess> Start(const std::vector<std::string>& argv,
                                               std::string& error) override;
};

// Windows command-line quoting for one argument (CommandLineToArgvW rules). Portable for tests.
std::string QuoteWindowsArg(const std::string& arg);

}  // namespace maba
