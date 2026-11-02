// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#include "maba/color.h"

#include <algorithm>
#include <cstdint>

namespace maba {

void BgrToHsv(int b, int g, int r, int& h, int& s, int& v) {
    const int mx = std::max({r, g, b});
    const int mn = std::min({r, g, b});
    const int delta = mx - mn;
    v = mx;
    s = mx == 0 ? 0 : (delta * 255 + mx / 2) / mx;
    if (delta == 0) {
        h = 0;
        return;
    }
    // Degrees (0..360) halved to OpenCV's 0..180 range.
    double deg = 0;
    if (mx == r) {
        deg = 60.0 * (g - b) / delta;
    } else if (mx == g) {
        deg = 120.0 + 60.0 * (b - r) / delta;
    } else {
        deg = 240.0 + 60.0 * (r - g) / delta;
    }
    if (deg < 0) deg += 360.0;
    h = static_cast<int>(deg / 2.0 + 0.5) % 180;
}

std::vector<Rect> FindColorBlobs(const Frame& frame, const ColorSpec& spec, int max_count) {
    std::vector<Rect> out;
    if (frame.empty() || max_count <= 0) return out;
    Rect area{0, 0, frame.width, frame.height};
    if (spec.roi) {
        const int x0 = std::clamp(spec.roi->x, 0, frame.width);
        const int y0 = std::clamp(spec.roi->y, 0, frame.height);
        const int x1 = std::clamp(spec.roi->x + spec.roi->width, 0, frame.width);
        const int y1 = std::clamp(spec.roi->y + spec.roi->height, 0, frame.height);
        area = {x0, y0, x1 - x0, y1 - y0};
    }
    if (area.width <= 0 || area.height <= 0) return out;

    const int w = area.width;
    const int h = area.height;
    std::vector<std::uint8_t> mask(static_cast<std::size_t>(w) * h, 0);
    for (int y = 0; y < h; ++y) {
        const std::uint8_t* row = frame.bgr.data() + (static_cast<std::size_t>(y + area.y) * frame.width + area.x) * 3;
        for (int x = 0; x < w; ++x) {
            int hh = 0, ss = 0, vv = 0;
            BgrToHsv(row[x * 3], row[x * 3 + 1], row[x * 3 + 2], hh, ss, vv);
            if (hh >= spec.h_min && hh <= spec.h_max && ss >= spec.s_min && ss <= spec.s_max &&
                vv >= spec.v_min && vv <= spec.v_max)
                mask[static_cast<std::size_t>(y) * w + x] = 1;
        }
    }

    std::vector<int> stack;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (mask[static_cast<std::size_t>(y) * w + x] != 1) continue;
            int min_x = x, max_x = x, min_y = y, max_y = y, count = 0;
            stack.assign(1, y * w + x);
            mask[static_cast<std::size_t>(y) * w + x] = 2;
            while (!stack.empty()) {
                const int p = stack.back();
                stack.pop_back();
                const int px = p % w, py = p / w;
                ++count;
                min_x = std::min(min_x, px);
                max_x = std::max(max_x, px);
                min_y = std::min(min_y, py);
                max_y = std::max(max_y, py);
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        const int nx = px + dx, ny = py + dy;
                        if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
                        std::uint8_t& m = mask[static_cast<std::size_t>(ny) * w + nx];
                        if (m != 1) continue;
                        m = 2;
                        stack.push_back(ny * w + nx);
                    }
                }
            }
            const int bw = max_x - min_x + 1, bh = max_y - min_y + 1;
            if (bw < spec.min_width || bw > spec.max_width || bh < spec.min_height ||
                bh > spec.max_height || count < spec.min_area || count > spec.max_area)
                continue;
            out.push_back({min_x + area.x, min_y + area.y, bw, bh});
        }
    }
    std::sort(out.begin(), out.end(), [](const Rect& a, const Rect& b) {
        return a.y != b.y ? a.y < b.y : a.x < b.x;
    });
    if (static_cast<int>(out.size()) > max_count) out.resize(static_cast<std::size_t>(max_count));
    return out;
}

}  // namespace maba
