// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#pragma once

#include "maba/types.h"

namespace maba {

// Screen capture + input for one target (Steam PC window, or one emulator type).
// Contract: everything crossing this interface is in the normalized 1280x720 space.
// Implementations own all scaling (see coords.h) and any DPI/window-scale handling.
class IController {
public:
    virtual ~IController() = default;

    // Native size of the target surface (informational; callers never need it for logic).
    virtual Size NativeSize() const = 0;

    // Capture the current screen, scaled to kNormWidth x kNormHeight. Empty frame on failure.
    virtual Frame Screenshot() = 0;

    // Crop of the given normalized region, taken at the target's native resolution (no downscale).
    // Intended for OCR, where the 1280x720 frame may lose text detail. Empty frame on failure.
    virtual Frame CaptureRegion(Rect normalized) = 0;

    virtual bool Click(Point normalized) = 0;
    virtual bool Swipe(Point from, Point to, int duration_ms) = 0;
    virtual bool Back() = 0;
};

}  // namespace maba
