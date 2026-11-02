// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Pure geometry used by platform controllers. No OS dependency, so it is unit-tested on every platform.
#pragma once

#include <vector>

#include "maba/types.h"

namespace maba {

// True if the rectangle has a positive area.
bool IsValid(const Rect& r);

// Intersection of two rectangles; an empty rect ({0,0,0,0}) if they do not overlap.
Rect Intersect(const Rect& a, const Rect& b);

// Where the client area sits inside a captured window bitmap.
// `window` is the window rect in screen coordinates (origin of the bitmap),
// `client_origin` is the client (0,0) converted to screen coordinates,
// `client` is the client size. The result is clamped to the bitmap; empty if invalid.
Rect ClientCropRect(const Rect& window, Point client_origin, Size client);

// Map a rectangle in the normalized 1280x720 space to native pixels of a surface.
// Edges are rounded independently, so adjacent regions do not overlap or leave gaps.
// The result is clamped to the surface; empty if invalid or fully outside.
Rect NormalizedRectToNative(const Rect& normalized, Size native);

// Inverse of NormalizedRectToNative (native rect -> normalized 1280x720 space).
Rect NativeRectToNormalized(const Rect& native_rect, Size native);

// SendInput absolute coordinates (0..65535) of a screen point on the virtual desktop.
// `virtual_desktop` is the bounding rect of all monitors (its origin may be negative).
// Points outside are clamped to the desktop edge.
Point ToAbsoluteInput(Point screen, const Rect& virtual_desktop);

// Points of a straight swipe from `from` to `to`, one per `step_ms` over `duration_ms`.
// Always contains both endpoints. The number of points is capped at `max_points` (>= 2).
std::vector<Point> SwipePath(Point from, Point to, int duration_ms, int step_ms,
                             int max_points = 240);

}  // namespace maba
