// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Win32 child processes with redirected pipes. No console window is created.
#ifdef _WIN32

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

#include "maba/process.h"

namespace maba {

namespace {

constexpr std::size_t kMaxErrBytes = 8 * 1024;

std::wstring Widen(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

std::string LastErrorText(const char* what) {
    return std::string(what) + " failed (Win32 error " + std::to_string(GetLastError()) + ")";
}

std::wstring CommandLine(const std::vector<std::string>& argv) {
    std::string cmd;
    for (std::size_t i = 0; i < argv.size(); ++i) {
        if (i) cmd += ' ';
        cmd += QuoteWindowsArg(argv[i]);
    }
    return Widen(cmd);
}

struct Handle {
    HANDLE h = nullptr;
    Handle() = default;
    explicit Handle(HANDLE v) : h(v) {}
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    ~Handle() { Close(); }
    void Close() {
        if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h);
        h = nullptr;
    }
};

// Pipe whose `parent` end is not inherited.
bool MakePipe(Handle& read, Handle& write, bool parent_reads) {
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    if (!CreatePipe(&read.h, &write.h, &sa, 0)) return false;
    return SetHandleInformation(parent_reads ? read.h : write.h, HANDLE_FLAG_INHERIT, 0) != 0;
}

void Drain(HANDLE pipe, std::string& out, std::size_t cap) {
    char buf[64 * 1024];
    DWORD n = 0;
    while (ReadFile(pipe, buf, sizeof(buf), &n, nullptr) && n > 0) {
        if (out.size() < cap) out.append(buf, std::min<std::size_t>(n, cap - out.size()));
    }
}

// Starts the process with the given child-side handles (all of them inheritable). Only these
// three handles are inherited (PROC_THREAD_ATTRIBUTE_HANDLE_LIST): otherwise the child, and any
// process it leaves behind, would also hold this process's own inheritable handles (e.g. the
// console pipes of a parent test runner) open.
// The process is placed in `job` (if any) before it runs, so a timeout can stop its whole tree.
bool Spawn(const std::vector<std::string>& argv, HANDLE in, HANDLE out, HANDLE err, HANDLE job,
           PROCESS_INFORMATION& pi, std::string& error) {
    if (argv.empty()) {
        error = "empty command";
        return false;
    }
    HANDLE inherit[3];
    DWORD count = 0;
    for (HANDLE h : {in, out, err}) {
        if (std::find(inherit, inherit + count, h) == inherit + count) inherit[count++] = h;
    }
    SIZE_T attr_size = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attr_size);
    std::vector<unsigned char> attr_buf(attr_size);
    auto* attrs = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attr_buf.data());
    if (!InitializeProcThreadAttributeList(attrs, 1, 0, &attr_size)) {
        error = LastErrorText("InitializeProcThreadAttributeList");
        return false;
    }
    struct AttrGuard {
        LPPROC_THREAD_ATTRIBUTE_LIST a;
        ~AttrGuard() { DeleteProcThreadAttributeList(a); }
    } guard{attrs};
    if (!UpdateProcThreadAttribute(attrs, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherit,
                                   count * sizeof(HANDLE), nullptr, nullptr)) {
        error = LastErrorText("UpdateProcThreadAttribute");
        return false;
    }

    STARTUPINFOEXW si{};
    si.StartupInfo.cb = sizeof(si);
    si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    si.StartupInfo.hStdInput = in;
    si.StartupInfo.hStdOutput = out;
    si.StartupInfo.hStdError = err;
    si.lpAttributeList = attrs;
    std::wstring cmd = CommandLine(argv);
    const DWORD flags = CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT | CREATE_SUSPENDED;
    if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, flags, nullptr, nullptr,
                        &si.StartupInfo, &pi)) {
        error = LastErrorText("CreateProcess") + " for '" + argv[0] + "'";
        return false;
    }
    if (job) AssignProcessToJobObject(job, pi.hProcess);  // best effort (nested jobs may refuse)
    ResumeThread(pi.hThread);
    return true;
}

class Win32Interactive : public IInteractiveProcess {
public:
    Win32Interactive(PROCESS_INFORMATION pi, HANDLE stdin_w, HANDLE stdout_r, HANDLE stderr_r)
        : process_(pi.hProcess), thread_(pi.hThread), stdin_(stdin_w) {
        out_reader_ = std::thread([this, stdout_r] { ReadLines(stdout_r); });
        err_reader_ = std::thread([this, stderr_r] {
            std::string sink;
            Drain(stderr_r, sink, 0);  // discard: the peer reports errors on stdout
            CloseHandle(stderr_r);
            ++readers_done_;
        });
    }

    ~Win32Interactive() override {
        Terminate();
        // Grandchildren may hold the pipes open; cancel blocked reads after a short grace.
        for (int i = 0; i < 50 && readers_done_.load() < 2; ++i) Sleep(10);
        for (int i = 0; i < 200 && readers_done_.load() < 2; ++i) {
            if (out_reader_.joinable()) CancelSynchronousIo(out_reader_.native_handle());
            if (err_reader_.joinable()) CancelSynchronousIo(err_reader_.native_handle());
            Sleep(10);
        }
        if (out_reader_.joinable()) out_reader_.join();
        if (err_reader_.joinable()) err_reader_.join();
    }

    bool WriteLine(const std::string& line) override {
        std::lock_guard<std::mutex> lock(write_mutex_);
        if (!stdin_.h) return false;
        const std::string data = line + "\n";
        DWORD written = 0;
        return WriteFile(stdin_.h, data.data(), static_cast<DWORD>(data.size()), &written,
                         nullptr) &&
               written == data.size();
    }

    std::optional<std::string> ReadLine(std::chrono::milliseconds timeout) override {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock, timeout, [this] { return !lines_.empty() || eof_; });
        if (lines_.empty()) return std::nullopt;
        std::string line = std::move(lines_.front());
        lines_.pop_front();
        return line;
    }

    bool Running() override {
        return process_.h && WaitForSingleObject(process_.h, 0) == WAIT_TIMEOUT;
    }

    void Terminate() override {
        {
            std::lock_guard<std::mutex> lock(write_mutex_);
            stdin_.Close();  // polite: a line-based peer exits on EOF
        }
        if (process_.h && WaitForSingleObject(process_.h, 1500) == WAIT_TIMEOUT)
            TerminateProcess(process_.h, 1);
    }

private:
    void ReadLines(HANDLE pipe) {
        std::string partial;
        char buf[4096];
        DWORD n = 0;
        while (ReadFile(pipe, buf, sizeof(buf), &n, nullptr) && n > 0) {
            partial.append(buf, n);
            std::size_t pos;
            while ((pos = partial.find('\n')) != std::string::npos) {
                std::string line = partial.substr(0, pos);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                partial.erase(0, pos + 1);
                std::lock_guard<std::mutex> lock(mutex_);
                lines_.push_back(std::move(line));
                cv_.notify_all();
            }
        }
        CloseHandle(pipe);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            eof_ = true;
            cv_.notify_all();
        }
        ++readers_done_;
    }

    Handle process_;
    Handle thread_;
    Handle stdin_;
    std::mutex write_mutex_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<std::string> lines_;
    bool eof_ = false;
    std::atomic<int> readers_done_{0};
    std::thread out_reader_;
    std::thread err_reader_;
};

}  // namespace

ProcessResult SystemProcessRunner::Run(const std::vector<std::string>& argv,
                                       std::chrono::milliseconds timeout) {
    ProcessResult result;
    Handle out_r, out_w, err_r, err_w;
    if (!MakePipe(out_r, out_w, true) || !MakePipe(err_r, err_w, true)) {
        result.error = LastErrorText("CreatePipe");
        return result;
    }
    Handle nul(CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, 0, nullptr));
    SetHandleInformation(nul.h, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);

    // Job without kill-on-close: a normal exit leaves e.g. a freshly started adb server alive,
    // a timeout terminates the whole tree.
    Handle job(CreateJobObjectW(nullptr, nullptr));
    PROCESS_INFORMATION pi{};
    if (!Spawn(argv, nul.h, out_w.h, err_w.h, job.h, pi, result.error)) return result;
    Handle process(pi.hProcess), thread(pi.hThread);
    result.started = true;
    out_w.Close();  // the child holds its own copies; EOF arrives when it exits
    err_w.Close();

    std::atomic<int> readers_done{0};
    std::thread out_reader([&] {
        Drain(out_r.h, result.out, static_cast<std::size_t>(-1));
        ++readers_done;
    });
    std::thread err_reader([&] {
        Drain(err_r.h, result.err, kMaxErrBytes);
        ++readers_done;
    });

    const DWORD wait_ms = static_cast<DWORD>(std::max<long long>(timeout.count(), 0));
    if (WaitForSingleObject(process.h, wait_ms) == WAIT_TIMEOUT) {
        result.timed_out = true;
        if (!job.h || !TerminateJobObject(job.h, 1)) TerminateProcess(process.h, 1);
        WaitForSingleObject(process.h, 5000);
    }
    // A grandchild that inherited the pipes (e.g. an adb server started by this command) keeps
    // them open after the child exits. Give the readers a moment, then cancel their reads.
    for (int i = 0; i < 50 && readers_done.load() < 2; ++i) Sleep(10);
    for (int i = 0; i < 200 && readers_done.load() < 2; ++i) {
        CancelSynchronousIo(out_reader.native_handle());
        CancelSynchronousIo(err_reader.native_handle());
        Sleep(10);
    }
    out_reader.join();
    err_reader.join();
    DWORD code = 0;
    if (GetExitCodeProcess(process.h, &code)) result.exit_code = static_cast<int>(code);
    return result;
}

std::unique_ptr<IInteractiveProcess> SystemProcessLauncher::Start(
    const std::vector<std::string>& argv, std::string& error) {
    Handle in_r, in_w, out_r, out_w, err_r, err_w;
    if (!MakePipe(in_r, in_w, false) || !MakePipe(out_r, out_w, true) ||
        !MakePipe(err_r, err_w, true)) {
        error = LastErrorText("CreatePipe");
        return nullptr;
    }
    PROCESS_INFORMATION pi{};
    if (!Spawn(argv, in_r.h, out_w.h, err_w.h, nullptr, pi, error)) return nullptr;
    in_r.Close();
    out_w.Close();
    err_w.Close();
    auto p = std::make_unique<Win32Interactive>(pi, in_w.h, out_r.h, err_r.h);
    in_w.h = nullptr;  // ownership moved
    out_r.h = nullptr;
    err_r.h = nullptr;
    return p;
}

long SystemDetachedProcesses::Start(const std::vector<std::string>& argv,
                                    const std::string& working_dir, std::string& error) {
    if (argv.empty()) {
        error = "empty command";
        return 0;
    }
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::wstring cmd = CommandLine(argv);
    const std::wstring dir = Widen(working_dir);
    // Leave this process's job (if allowed) so that closing the GUI does not end the emulator.
    DWORD flags = CREATE_NEW_PROCESS_GROUP | CREATE_BREAKAWAY_FROM_JOB;
    BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, flags, nullptr,
                             dir.empty() ? nullptr : dir.c_str(), &si, &pi);
    if (!ok && GetLastError() == ERROR_ACCESS_DENIED) {
        flags &= ~static_cast<DWORD>(CREATE_BREAKAWAY_FROM_JOB);
        ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, flags, nullptr,
                            dir.empty() ? nullptr : dir.c_str(), &si, &pi);
    }
    if (!ok) {
        error = LastErrorText("CreateProcess") + " for '" + argv[0] + "'";
        return 0;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return static_cast<long>(pi.dwProcessId);
}

bool SystemDetachedProcesses::Alive(long pid) {
    if (pid <= 0) return false;
    Handle process(OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pid)));
    return process.h && WaitForSingleObject(process.h, 0) == WAIT_TIMEOUT;
}

bool SystemDetachedProcesses::KillTree(long pid, std::string& error) {
    if (pid <= 0) {
        error = "no process";
        return false;
    }
    SystemProcessRunner runner;
    const ProcessResult r =
        runner.Run({"taskkill", "/T", "/F", "/PID", std::to_string(pid)}, std::chrono::milliseconds{15000});
    if (r.ok()) return true;
    error = "taskkill failed: " + (r.error.empty() ? r.err + r.out : r.error);
    return false;
}

}  // namespace maba

#endif  // _WIN32
