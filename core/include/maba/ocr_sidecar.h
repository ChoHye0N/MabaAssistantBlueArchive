// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// IOcr backed by the Python sidecar (ocr/server.py) over stdin/stdout, one JSON object per line.
// Protocol (docs/ARCHITECTURE.md, "OCR sidecar protocol"):
//   sidecar -> {"ready": true, "engine": "...", "version": "..."}            once, at start
//   core    -> {"id": 1, "op": "recognize", "lang": "korean", "width": W, "height": H,
//               "format": "bgr24", "data": "<base64>"}
//   sidecar -> {"id": 1, "ok": true, "results": [{"text": "...", "box": [x, y, w, h],
//               "score": 0.98}]}   or   {"id": 1, "ok": false, "error": "..."}
// Boxes are relative to the image sent; the client maps them back to the caller's frame.
#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "maba/languages.h"
#include "maba/log.h"
#include "maba/ocr.h"
#include "maba/process.h"

namespace maba {

std::string Base64Encode(const std::uint8_t* data, std::size_t size);

// Crops `roi` (clamped to the frame; empty = whole frame). Empty frame if nothing is left.
Frame CropFrame(const Frame& frame, const Rect& roi);

std::string EncodeRecognizeRequest(int id, const Frame& image, const std::string& ocr_lang);

// Parses a response line for request `id`. Boxes are shifted by `offset` (the crop origin).
bool DecodeRecognizeResponse(const std::string& line, int id, Point offset,
                             std::vector<OcrResult>& out, std::string& error);

struct SidecarOptions {
    std::vector<std::string> command;  // e.g. {"python", "ocr/server.py"}
    std::chrono::milliseconds start_timeout{120000};  // first start may download models
    std::chrono::milliseconds request_timeout{20000};
    int max_restarts = 2;  // per SidecarOcr lifetime, after a crash or a timeout
};

class SidecarOcr : public IOcr {
public:
    SidecarOcr(IProcessLauncher& launcher, SidecarOptions options, LanguageConfig languages,
               LogFn log = {});
    ~SidecarOcr() override;

    // Starts the process and waits for its ready line. Called lazily by Recognize.
    bool Start();
    void Stop();
    const std::string& EngineInfo() const { return engine_; }

    std::vector<OcrResult> Recognize(const Frame& frame, const Rect& roi,
                                     const std::string& language) override;
    std::string LastError() const override { return last_error_; }

private:
    bool Fail(std::string message);

    IProcessLauncher& launcher_;
    SidecarOptions options_;
    LanguageConfig languages_;
    LogFn log_;
    std::unique_ptr<IInteractiveProcess> process_;
    std::string engine_;
    std::string last_error_;
    int next_id_ = 1;
    int restarts_ = 0;
    bool started_once_ = false;
};

}  // namespace maba
