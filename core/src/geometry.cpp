// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#include "maba/geometry.h"

#include <algorithm>
#include <cmath>

namespace maba {

namespace {

int RoundScaled(int value, int from_extent, int to_extent) {
    return static_cast<int>(std::lround(static_cast<double>(value) * to_extent / from_extent));
}

Rect FromEdges(int left, int top, int right, int bottom) {
    if (right <= left || bottom <= top) return {};
    return {left, top, right - left, bottom - top};
}

}  // namespace

bool IsValid(const Rect& r) { return r.width > 0 && r.height > 0; }

Rect Intersect(const Rect& a, const Rect& b) {
    if (!IsValid(a) || !IsValid(b)) return {};
    return FromEdges(std::max(a.x, b.x), std::max(a.y, b.y),
                     std::min(a.x + a.width, b.x + b.width),
                     std::min(a.y + a.height, b.y + b.height));
}

Rect ClientCropRect(const Rect& window, Point client_origin, Size client) {
    if (!IsValid(window) || client.width <= 0 || client.height <= 0) return {};
    const Rect crop{client_origin.x - window.x, client_origin.y - window.y, client.width,
                    client.height};
    return Intersect(crop, {0, 0, window.width, window.height});
}

Rect NormalizedRectToNative(const Rect& normalized, Size native) {
    if (!IsValid(normalized) || native.width <= 0 || native.height <= 0) return {};
    const Rect scaled = FromEdges(
        RoundScaled(normalized.x, kNormWidth, native.width),
        RoundScaled(normalized.y, kNormHeight, native.height),
        RoundScaled(normalized.x + normalized.width, kNormWidth, native.width),
        RoundScaled(normalized.y + normalized.height, kNormHeight, native.height));
    return Intersect(scaled, {0, 0, native.width, native.height});
}

Rect NativeRectToNormalized(const Rect& native_rect, Size native) {
    if (!IsValid(native_rect) || native.width <= 0 || native.height <= 0) return {};
    const Rect scaled = FromEdges(
        RoundScaled(native_rect.x, native.width, kNormWidth),
        RoundScaled(native_rect.y, native.height, kNormHeight),
        RoundScaled(native_rect.x + native_rect.width, native.width, kNormWidth),
        RoundScaled(native_rect.y + native_rect.height, native.height, kNormHeight));
    return Intersect(scaled, {0, 0, kNormWidth, kNormHeight});
}

Point ToAbsoluteInput(Point screen, const Rect& virtual_desktop) {
    if (!IsValid(virtual_desktop)) return {};
    auto axis = [](int v, int origin, int extent) {
        if (extent <= 1) return 0;
        const int offset = std::clamp(v - origin, 0, extent - 1);
        return static_cast<int>(std::lround(offset * 65535.0 / (extent - 1)));
    };
    return {axis(screen.x, virtual_desktop.x, virtual_desktop.width),
            axis(screen.y, virtual_desktop.y, virtual_desktop.height)};
}

std::vector<Point> SwipePath(Point from, Point to, int duration_ms, int step_ms, int max_points) {
    max_points = std::max(max_points, 2);
    int segments = (step_ms > 0 && duration_ms > 0) ? duration_ms / step_ms : 1;
    segments = std::clamp(segments, 1, max_points - 1);

    std::vector<Point> path;
    path.reserve(static_cast<std::size_t>(segments) + 1);
    for (int i = 0; i <= segments; ++i) {
        const double t = static_cast<double>(i) / segments;
        path.push_back({static_cast<int>(std::lround(from.x + (to.x - from.x) * t)),
                        static_cast<int>(std::lround(from.y + (to.y - from.y) * t))});
    }
    return path;
}

}  // namespace maba
