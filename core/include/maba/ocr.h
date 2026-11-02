// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#pragma once

#include <string>
#include <vector>

#include "maba/types.h"

namespace maba {

struct OcrResult {
    std::string text;  // UTF-8
    Rect box;          // pixel coordinates of the frame passed to Recognize
    float confidence = 0.0f;
};

// Core depends only on this interface. First implementation: the Python sidecar (RapidOCR on
// ONNX Runtime with PaddleOCR models, see ocr_sidecar.h). Later the same ONNX models can run
// natively in C++.
class IOcr {
public:
    virtual ~IOcr() = default;

    // Recognize text inside `roi` (pixel coordinates of `frame`; an empty roi means the whole
    // frame). `language` is a code from config/languages.json ("ko", "en", ...).
    // Frames may be native-resolution crops from IController::CaptureRegion.
    virtual std::vector<OcrResult> Recognize(const Frame& frame, const Rect& roi,
                                             const std::string& language) = 0;

    // Reason for the last failure (empty when the last call succeeded).
    virtual std::string LastError() const { return {}; }
};

}  // namespace maba
