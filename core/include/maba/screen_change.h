// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
//
// How much of the screen changed between two frames, for "did the click do anything" and
// "has the screen stopped moving". Portable (no OpenCV).
#pragma once

#include <optional>

#include "maba/types.h"

namespace maba {

// Percentage (0..100) of the pixels of `roi` (whole frame when unset) that differ strongly.
// Small differences are ignored: idle animation and lighting move many pixels by a little.
// Frames of different sizes count as completely changed.
double ChangedShare(const Frame& a, const Frame& b, std::optional<Rect> roi = std::nullopt);

// A loading screen: nearly black, and it does not move, so it must not count as a resting page.
bool IsLoadingScreen(const Frame& frame);

// The frame with `margin` pixels added on every side, repeating the edge pixels. Text detection
// misses or splits numbers in crops that end right at the text.
Frame AddMargin(const Frame& frame, int margin);

}  // namespace maba
