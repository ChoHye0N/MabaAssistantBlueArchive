// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Per-monitor DPI awareness for the calling thread, restored on scope exit.
// With it, window rects, client sizes, cursor and screen metrics are all physical pixels,
// so capture and input agree regardless of the display scale setting.
// Derived from the project owner's earlier project; see docs/SOURCES.md.
#pragma once

#ifdef _WIN32

#include <windows.h>

namespace maba::win {

class DpiAwareScope {
public:
    DpiAwareScope()
        : previous_(SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {}
    ~DpiAwareScope() {
        if (previous_) SetThreadDpiAwarenessContext(previous_);
    }
    DpiAwareScope(const DpiAwareScope&) = delete;
    DpiAwareScope& operator=(const DpiAwareScope&) = delete;

private:
    DPI_AWARENESS_CONTEXT previous_;
};

}  // namespace maba::win

#endif  // _WIN32
