// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Log sink passed down to Core components. Core never writes to stdout itself; the CLI prints,
// the C ABI forwards to the GUI as JSON events.
#pragma once

#include <functional>
#include <string>

namespace maba {

enum class LogLevel { kDebug, kInfo, kWarn, kError };

using LogFn = std::function<void(LogLevel, const std::string&)>;

inline const char* ToString(LogLevel level) {
    switch (level) {
        case LogLevel::kDebug: return "debug";
        case LogLevel::kInfo: return "info";
        case LogLevel::kWarn: return "warn";
        case LogLevel::kError: return "error";
    }
    return "info";
}

// Calls `log` if it is set.
inline void Log(const LogFn& log, LogLevel level, const std::string& message) {
    if (log) log(level, message);
}

}  // namespace maba
