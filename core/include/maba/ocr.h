// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#pragma once

#include <string>
#include <vector>

#include "maba/types.h"

namespace maba {

struct OcrResult {
    std::string text;  // UTF-8
    Rect box;          // in normalized coordinates
    float confidence = 0.0f;
};

// Core depends only on this interface. First implementation: Python (PaddleOCR) sidecar over IPC.
// Later: PaddleOCR models exported to ONNX and run natively in C++.
class IOcr {
public:
    virtual ~IOcr() = default;

    // Recognize text inside `roi` of the frame. `language` is a code from config/languages.json.
    virtual std::vector<OcrResult> Recognize(const Frame& frame, const Rect& roi,
                                             const std::string& language) = 0;
};

}  // namespace maba
