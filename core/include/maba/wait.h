// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Bounded waiting. Every wait in Core goes through WaitUntil: it always has a timeout and an attempt cap.
#pragma once

#include <atomic>
#include <chrono>
#include <functional>

namespace maba {

// Time source, injectable so tests run instantly.
class IClock {
public:
    virtual ~IClock() = default;
    virtual std::chrono::milliseconds Now() = 0;
    virtual void Sleep(std::chrono::milliseconds duration) = 0;
};

// Real clock (steady_clock + this_thread::sleep_for).
class SteadyClock : public IClock {
public:
    std::chrono::milliseconds Now() override;
    void Sleep(std::chrono::milliseconds duration) override;
};

struct WaitOptions {
    std::chrono::milliseconds timeout{5000};
    std::chrono::milliseconds interval{200};
    int max_attempts = 100;  // hard cap even if the clock misbehaves
    // Optional stop request (e.g. the GUI's Stop button); checked before every attempt.
    const std::atomic<bool>* cancel = nullptr;
};

struct WaitResult {
    bool ok = false;
    bool cancelled = false;
    int attempts = 0;
    std::chrono::milliseconds elapsed{0};
};

// Calls `condition` until it returns true, the timeout passes or max_attempts is reached.
// The condition is evaluated at least once unless `options.cancel` is already set.
WaitResult WaitUntil(const std::function<bool()>& condition, const WaitOptions& options,
                     IClock& clock);

}  // namespace maba
