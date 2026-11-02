// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#pragma once

#include "maba/types.h"

namespace maba {

// Map a point in the normalized 1280x720 space to a native surface of the given size.
// The result is clamped to the surface. Returns {0,0} if the native size is invalid.
Point ToNative(Point normalized, Size native);

// Inverse of ToNative.
Point ToNormalized(Point native_point, Size native);

// True if the surface is 16:9 within the tolerance.
bool IsSupportedAspect(Size native, double tolerance = 0.01);

}  // namespace maba
