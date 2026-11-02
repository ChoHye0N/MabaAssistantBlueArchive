// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#include "maba/wait.h"

#include <algorithm>
#include <thread>

namespace maba {

std::chrono::milliseconds SteadyClock::Now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch());
}

void SteadyClock::Sleep(std::chrono::milliseconds duration) {
    std::this_thread::sleep_for(duration);
}

WaitResult WaitUntil(const std::function<bool()>& condition, const WaitOptions& options,
                     IClock& clock) {
    WaitResult result;
    const auto start = clock.Now();
    const int max_attempts = std::max(options.max_attempts, 1);
    const auto interval = std::max(options.interval, std::chrono::milliseconds{1});

    while (true) {
        ++result.attempts;
        if (condition()) {
            result.ok = true;
            break;
        }
        result.elapsed = clock.Now() - start;
        if (result.attempts >= max_attempts || result.elapsed + interval > options.timeout) break;
        clock.Sleep(interval);
    }
    result.elapsed = clock.Now() - start;
    return result;
}

}  // namespace maba
