// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Colour blobs: connected regions whose pixels fall inside an HSV range, filtered by size.
// Used for markers whose shape animates (cafe students), where template matching is unstable.
// Portable (no OpenCV), so it is unit-tested everywhere.
#pragma once

#include <optional>
#include <vector>

#include "maba/types.h"

namespace maba {

// HSV in OpenCV's scale: hue 0..180, saturation and value 0..255. Bounds are inclusive.
struct ColorSpec {
    int h_min = 0, s_min = 0, v_min = 0;
    int h_max = 180, s_max = 255, v_max = 255;
    // Blob size limits in normalized pixels (bounding box) and pixel count.
    int min_width = 1, max_width = kNormWidth;
    int min_height = 1, max_height = kNormHeight;
    int min_area = 1, max_area = kNormWidth * kNormHeight;
    std::optional<Rect> roi;  // search area; absent = whole frame
};

// Converts one BGR pixel to OpenCV-scale HSV.
void BgrToHsv(int b, int g, int r, int& h, int& s, int& v);

// Bounding boxes of the blobs that pass the size filter (8-connected), top to bottom, then left
// to right. At most `max_count` boxes.
std::vector<Rect> FindColorBlobs(const Frame& frame, const ColorSpec& spec, int max_count = 64);

}  // namespace maba
