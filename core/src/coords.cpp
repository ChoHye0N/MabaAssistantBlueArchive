// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#include "maba/coords.h"

#include <algorithm>
#include <cmath>

namespace maba {

namespace {

int ScaleClamped(int value, int from_extent, int to_extent) {
    const double scaled = static_cast<double>(value) * to_extent / from_extent;
    const long rounded = std::lround(scaled);
    return static_cast<int>(std::clamp<long>(rounded, 0, to_extent - 1));
}

}  // namespace

Point ToNative(Point normalized, Size native) {
    if (native.width <= 0 || native.height <= 0) return {};
    return {ScaleClamped(normalized.x, kNormWidth, native.width),
            ScaleClamped(normalized.y, kNormHeight, native.height)};
}

Point ToNormalized(Point native_point, Size native) {
    if (native.width <= 0 || native.height <= 0) return {};
    return {ScaleClamped(native_point.x, native.width, kNormWidth),
            ScaleClamped(native_point.y, native.height, kNormHeight)};
}

bool IsSupportedAspect(Size native, double tolerance) {
    if (native.width <= 0 || native.height <= 0) return false;
    const double ratio = static_cast<double>(native.width) / native.height;
    return std::abs(ratio - 16.0 / 9.0) <= tolerance;
}

}  // namespace maba
