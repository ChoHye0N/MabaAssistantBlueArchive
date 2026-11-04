// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#include "maba/screen_change.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>

namespace maba {

namespace {

// Mean difference over the three channels above which a pixel counts as changed. Measured on the
// Steam client: resting screens stay below 11 % with it, page changes reach 60 % and more.
constexpr int kPixelThreshold = 24;
constexpr int kStep = 2;  // every second pixel in both directions

}  // namespace

double ChangedShare(const Frame& a, const Frame& b, std::optional<Rect> roi) {
    if (a.width != b.width || a.height != b.height || a.bgr.size() != b.bgr.size()) return 100.0;
    if (a.empty()) return 0.0;
    const Rect r = roi.value_or(Rect{0, 0, a.width, a.height});
    const int x0 = std::clamp(r.x, 0, a.width), x1 = std::clamp(r.x + r.width, 0, a.width);
    const int y0 = std::clamp(r.y, 0, a.height), y1 = std::clamp(r.y + r.height, 0, a.height);
    long long changed = 0, total = 0;
    for (int y = y0; y < y1; y += kStep) {
        for (int x = x0; x < x1; x += kStep) {
            const std::size_t at = (static_cast<std::size_t>(y) * a.width + x) * 3;
            const int sum = std::abs(a.bgr[at] - b.bgr[at]) + std::abs(a.bgr[at + 1] - b.bgr[at + 1]) +
                            std::abs(a.bgr[at + 2] - b.bgr[at + 2]);
            if (sum > kPixelThreshold * 3) ++changed;
            ++total;
        }
    }
    return total == 0 ? 0.0 : 100.0 * static_cast<double>(changed) / static_cast<double>(total);
}

Frame AddMargin(const Frame& frame, int margin) {
    if (frame.empty() || margin <= 0) return frame;
    Frame out;
    out.width = frame.width + margin * 2;
    out.height = frame.height + margin * 2;
    out.bgr.resize(static_cast<std::size_t>(out.width) * out.height * 3);
    for (int y = 0; y < out.height; ++y) {
        const int sy = std::clamp(y - margin, 0, frame.height - 1);
        for (int x = 0; x < out.width; ++x) {
            const int sx = std::clamp(x - margin, 0, frame.width - 1);
            const std::size_t from = (static_cast<std::size_t>(sy) * frame.width + sx) * 3;
            const std::size_t to = (static_cast<std::size_t>(y) * out.width + x) * 3;
            for (int c = 0; c < 3; ++c) out.bgr[to + c] = frame.bgr[from + c];
        }
    }
    return out;
}

bool IsLoadingScreen(const Frame& frame) {
    if (frame.empty()) return false;
    long long dark = 0, total = 0;
    for (int y = 0; y < frame.height; y += kStep * 4) {
        for (int x = 0; x < frame.width; x += kStep * 4) {
            const std::size_t at = (static_cast<std::size_t>(y) * frame.width + x) * 3;
            if (frame.bgr[at] < 16 && frame.bgr[at + 1] < 16 && frame.bgr[at + 2] < 16) ++dark;
            ++total;
        }
    }
    return dark * 100 >= total * 85;
}

}  // namespace maba
