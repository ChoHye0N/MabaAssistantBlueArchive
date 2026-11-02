// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#pragma once

#include <cstdint>
#include <vector>

namespace maba {

// All game logic works in this normalized coordinate space.
inline constexpr int kNormWidth = 1280;
inline constexpr int kNormHeight = 720;

struct Point {
    int x = 0;
    int y = 0;
};

struct Size {
    int width = 0;
    int height = 0;
};

struct Rect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

// BGR frame, tightly packed (width * height * 3 bytes).
// Controllers return frames already scaled to kNormWidth x kNormHeight.
struct Frame {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> bgr;

    bool empty() const { return width <= 0 || height <= 0 || bgr.empty(); }
};

}  // namespace maba
