// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// POSIX child processes (fork/exec, pipes, poll). Used by the Linux test build; the product
// itself is Windows-only.
#ifndef _WIN32

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>

#include "maba/process.h"

namespace maba {

namespace {

using Clock = std::chrono::steady_clock;
constexpr std::size_t kMaxErrBytes = 8 * 1024;

void CloseFd(int& fd) {
    if (fd >= 0) close(fd);
    fd = -1;
}

// Forks and execs argv. Child stdio: in/out/err (-1 = /dev/null). Returns the pid or -1.
pid_t Spawn(const std::vector<std::string>& argv, int in, int out, int err, std::string& error) {
    if (argv.empty()) {
        error = "empty command";
        return -1;
    }
    int status_pipe[2];  // reports exec failure to the parent
    if (pipe(status_pipe) != 0) {
        error = std::string("pipe: ") + std::strerror(errno);
        return -1;
    }
    fcntl(status_pipe[1], F_SETFD, FD_CLOEXEC);
    const pid_t pid = fork();
    if (pid < 0) {
        error = std::string("fork: ") + std::strerror(errno);
        close(status_pipe[0]);
        close(status_pipe[1]);
        return -1;
    }
    if (pid == 0) {
        const int devnull = open("/dev/null", O_RDWR);
        dup2(in >= 0 ? in : devnull, 0);
        dup2(out >= 0 ? out : devnull, 1);
        dup2(err >= 0 ? err : devnull, 2);
        std::vector<char*> args;
        for (const auto& a : argv) args.push_back(const_cast<char*>(a.c_str()));
        args.push_back(nullptr);
        close(status_pipe[0]);
        execvp(args[0], args.data());
        const int e = errno;
        (void)!write(status_pipe[1], &e, sizeof(e));
        _exit(127);
    }
    close(status_pipe[1]);
    int child_errno = 0;
    const ssize_t n = read(status_pipe[0], &child_errno, sizeof(child_errno));
    close(status_pipe[0]);
    if (n == sizeof(child_errno)) {
        waitpid(pid, nullptr, 0);
        error = "cannot run '" + argv[0] + "': " + std::strerror(child_errno);
        return -1;
    }
    return pid;
}

int ExitCode(int status) {
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return -1;
}

class PosixInteractive : public IInteractiveProcess {
public:
    PosixInteractive(pid_t pid, int in_w, int out_r) : pid_(pid), in_(in_w) {
        reader_ = std::thread([this, out_r] { ReadLines(out_r); });
    }
    ~PosixInteractive() override {
        Terminate();
        if (reader_.joinable()) reader_.join();
    }

    bool WriteLine(const std::string& line) override {
        std::lock_guard<std::mutex> lock(write_mutex_);
        if (in_ < 0) return false;
        const std::string data = line + "\n";
        std::size_t off = 0;
        while (off < data.size()) {
            const ssize_t n = write(in_, data.data() + off, data.size() - off);
            if (n <= 0) return false;
            off += static_cast<std::size_t>(n);
        }
        return true;
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
        if (pid_ <= 0) return false;
        int status = 0;
        if (waitpid(pid_, &status, WNOHANG) == pid_) {
            pid_ = -1;
            return false;
        }
        return true;
    }

    void Terminate() override {
        {
            std::lock_guard<std::mutex> lock(write_mutex_);
            CloseFd(in_);
        }
        if (pid_ <= 0) return;
        for (int i = 0; i < 150; ++i) {
            if (waitpid(pid_, nullptr, WNOHANG) == pid_) {
                pid_ = -1;
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
        }
        kill(pid_, SIGKILL);
        waitpid(pid_, nullptr, 0);
        pid_ = -1;
    }

private:
    void ReadLines(int fd) {
        std::string partial;
        char buf[4096];
        while (true) {
            const ssize_t n = read(fd, buf, sizeof(buf));
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) break;
            partial.append(buf, static_cast<std::size_t>(n));
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
        close(fd);
        std::lock_guard<std::mutex> lock(mutex_);
        eof_ = true;
        cv_.notify_all();
    }

    pid_t pid_;
    int in_;
    std::mutex write_mutex_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<std::string> lines_;
    bool eof_ = false;
    std::thread reader_;
};

}  // namespace

ProcessResult SystemProcessRunner::Run(const std::vector<std::string>& argv,
                                       std::chrono::milliseconds timeout) {
    ProcessResult result;
    int out[2], err[2];
    if (pipe(out) != 0) {
        result.error = std::string("pipe: ") + std::strerror(errno);
        return result;
    }
    if (pipe(err) != 0) {
        result.error = std::string("pipe: ") + std::strerror(errno);
        close(out[0]);
        close(out[1]);
        return result;
    }
    fcntl(out[0], F_SETFD, FD_CLOEXEC);
    fcntl(err[0], F_SETFD, FD_CLOEXEC);
    const pid_t pid = Spawn(argv, -1, out[1], err[1], result.error);
    close(out[1]);
    close(err[1]);
    if (pid < 0) {
        close(out[0]);
        close(err[0]);
        return result;
    }
    result.started = true;

    const auto deadline = Clock::now() + timeout;
    int fds[2] = {out[0], err[0]};
    int status = 0;
    bool exited = false;
    while (fds[0] >= 0 || fds[1] >= 0) {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now());
        if (left.count() <= 0) {
            result.timed_out = true;
            break;
        }
        if (!exited && waitpid(pid, &status, WNOHANG) == pid) {
            exited = true;
            // Grandchildren may keep the pipes open; read what is buffered, then stop.
        }
        pollfd p[2];
        int n = 0;
        for (int fd : fds)
            if (fd >= 0) p[n++] = {fd, POLLIN, 0};
        const int ready = poll(p, n, exited ? 50 : static_cast<int>(std::min<long long>(left.count(), 100)));
        if (ready < 0 && errno != EINTR) break;
        if (ready == 0 && exited) break;
        for (int i = 0; i < n; ++i) {
            if (!(p[i].revents & (POLLIN | POLLHUP | POLLERR))) continue;
            char buf[64 * 1024];
            const ssize_t got = read(p[i].fd, buf, sizeof(buf));
            const bool is_out = p[i].fd == fds[0];
            if (got <= 0) {
                close(p[i].fd);
                (is_out ? fds[0] : fds[1]) = -1;
                continue;
            }
            if (is_out) {
                result.out.append(buf, static_cast<std::size_t>(got));
            } else if (result.err.size() < kMaxErrBytes) {
                result.err.append(buf, std::min<std::size_t>(static_cast<std::size_t>(got),
                                                             kMaxErrBytes - result.err.size()));
            }
        }
    }
    for (int& fd : fds) CloseFd(fd);
    if (result.timed_out && !exited) kill(pid, SIGKILL);
    if (!exited) waitpid(pid, &status, 0);
    result.exit_code = ExitCode(status);
    return result;
}

std::unique_ptr<IInteractiveProcess> SystemProcessLauncher::Start(
    const std::vector<std::string>& argv, std::string& error) {
    int in[2], out[2];
    if (pipe(in) != 0) {
        error = std::string("pipe: ") + std::strerror(errno);
        return nullptr;
    }
    if (pipe(out) != 0) {
        error = std::string("pipe: ") + std::strerror(errno);
        close(in[0]);
        close(in[1]);
        return nullptr;
    }
    fcntl(in[1], F_SETFD, FD_CLOEXEC);
    fcntl(out[0], F_SETFD, FD_CLOEXEC);
    signal(SIGPIPE, SIG_IGN);  // a dead peer must not kill this process on write
    const pid_t pid = Spawn(argv, in[0], out[1], -1, error);
    close(in[0]);
    close(out[1]);
    if (pid < 0) {
        close(in[1]);
        close(out[0]);
        return nullptr;
    }
    return std::make_unique<PosixInteractive>(pid, in[1], out[0]);
}

long SystemDetachedProcesses::Start(const std::vector<std::string>& argv,
                                    const std::string& working_dir, std::string& error) {
    if (argv.empty()) {
        error = "empty command";
        return 0;
    }
    int status_pipe[2];
    if (pipe(status_pipe) != 0) {
        error = std::string("pipe: ") + std::strerror(errno);
        return 0;
    }
    fcntl(status_pipe[1], F_SETFD, FD_CLOEXEC);
    const pid_t pid = fork();
    if (pid < 0) {
        error = std::string("fork: ") + std::strerror(errno);
        close(status_pipe[0]);
        close(status_pipe[1]);
        return 0;
    }
    if (pid == 0) {
        setsid();  // own process group: KillTree signals the whole group
        const int devnull = open("/dev/null", O_RDWR);
        dup2(devnull, 0);
        dup2(devnull, 1);
        dup2(devnull, 2);
        close(status_pipe[0]);
        int e = 0;
        if (!working_dir.empty() && chdir(working_dir.c_str()) != 0) {
            e = errno;
        } else {
            std::vector<char*> args;
            for (const auto& a : argv) args.push_back(const_cast<char*>(a.c_str()));
            args.push_back(nullptr);
            execvp(args[0], args.data());
            e = errno;
        }
        (void)!write(status_pipe[1], &e, sizeof(e));
        _exit(127);
    }
    close(status_pipe[1]);
    int child_errno = 0;
    const ssize_t n = read(status_pipe[0], &child_errno, sizeof(child_errno));
    close(status_pipe[0]);
    if (n == sizeof(child_errno)) {
        waitpid(pid, nullptr, 0);
        error = "cannot run '" + argv[0] + "': " + std::strerror(child_errno);
        return 0;
    }
    return static_cast<long>(pid);
}

bool SystemDetachedProcesses::Alive(long pid) {
    if (pid <= 0) return false;
    int status = 0;
    if (waitpid(static_cast<pid_t>(pid), &status, WNOHANG) == pid) return false;  // reaped
    return kill(static_cast<pid_t>(pid), 0) == 0;
}

bool SystemDetachedProcesses::KillTree(long pid, std::string& error) {
    if (pid <= 0) {
        error = "no process";
        return false;
    }
    if (kill(-static_cast<pid_t>(pid), SIGKILL) != 0 && kill(static_cast<pid_t>(pid), SIGKILL) != 0) {
        error = std::string("kill: ") + std::strerror(errno);
        return false;
    }
    waitpid(static_cast<pid_t>(pid), nullptr, WNOHANG);
    return true;
}

}  // namespace maba

#endif  // !_WIN32
