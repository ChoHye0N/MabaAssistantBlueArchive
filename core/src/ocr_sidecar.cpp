// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#include "maba/ocr_sidecar.h"

#include <algorithm>

#include <nlohmann/json.hpp>

#include "maba/geometry.h"

namespace maba {

using nlohmann::json;

std::string Base64Encode(const std::uint8_t* data, std::size_t size) {
    static const char kTable[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((size + 2) / 3 * 4);
    std::size_t i = 0;
    for (; i + 2 < size; i += 3) {
        const std::uint32_t v = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
        out += kTable[(v >> 18) & 63];
        out += kTable[(v >> 12) & 63];
        out += kTable[(v >> 6) & 63];
        out += kTable[v & 63];
    }
    if (i + 1 == size) {
        const std::uint32_t v = data[i] << 16;
        out += kTable[(v >> 18) & 63];
        out += kTable[(v >> 12) & 63];
        out += "==";
    } else if (i + 2 == size) {
        const std::uint32_t v = (data[i] << 16) | (data[i + 1] << 8);
        out += kTable[(v >> 18) & 63];
        out += kTable[(v >> 12) & 63];
        out += kTable[(v >> 6) & 63];
        out += '=';
    }
    return out;
}

Frame CropFrame(const Frame& frame, const Rect& roi) {
    if (frame.empty()) return {};
    const Rect whole{0, 0, frame.width, frame.height};
    const Rect r = IsValid(roi) ? Intersect(roi, whole) : whole;
    if (!IsValid(r)) return {};
    Frame out;
    out.width = r.width;
    out.height = r.height;
    out.bgr.resize(static_cast<std::size_t>(r.width) * r.height * 3);
    for (int y = 0; y < r.height; ++y) {
        const auto* src = frame.bgr.data() + (static_cast<std::size_t>(r.y + y) * frame.width + r.x) * 3;
        std::copy(src, src + static_cast<std::size_t>(r.width) * 3,
                  out.bgr.begin() + static_cast<std::ptrdiff_t>(y) * r.width * 3);
    }
    return out;
}

std::string EncodeRecognizeRequest(int id, const Frame& image, const std::string& ocr_lang) {
    json req = {{"id", id},
                {"op", "recognize"},
                {"lang", ocr_lang},
                {"width", image.width},
                {"height", image.height},
                {"format", "bgr24"},
                {"data", Base64Encode(image.bgr.data(), image.bgr.size())}};
    return req.dump();
}

bool DecodeRecognizeResponse(const std::string& line, int id, Point offset,
                             std::vector<OcrResult>& out, std::string& error) {
    out.clear();
    try {
        const json res = json::parse(line);
        if (!res.is_object() || !res.contains("id") || res.at("id") != id) {
            error = "response for another request";
            return false;
        }
        if (!res.value("ok", false)) {
            error = res.value("error", std::string("sidecar reported an error"));
            return false;
        }
        for (const auto& item : res.at("results")) {
            OcrResult r;
            r.text = item.at("text").get<std::string>();
            r.confidence = item.value("score", 0.0f);
            const auto& box = item.at("box");
            if (!box.is_array() || box.size() != 4) throw std::runtime_error("box must have 4 numbers");
            r.box = {box[0].get<int>() + offset.x, box[1].get<int>() + offset.y, box[2].get<int>(),
                     box[3].get<int>()};
            out.push_back(std::move(r));
        }
        return true;
    } catch (const std::exception& e) {
        error = std::string("bad response: ") + e.what();
        out.clear();
        return false;
    }
}

SidecarOcr::SidecarOcr(IProcessLauncher& launcher, SidecarOptions options,
                       LanguageConfig languages, LogFn log)
    : launcher_(launcher),
      options_(std::move(options)),
      languages_(std::move(languages)),
      log_(std::move(log)) {}

SidecarOcr::~SidecarOcr() { Stop(); }

bool SidecarOcr::Fail(std::string message) {
    last_error_ = std::move(message);
    Log(log_, LogLevel::kWarn, "ocr: " + last_error_);
    return false;
}

void SidecarOcr::Stop() {
    if (process_) {
        process_->Terminate();
        process_.reset();
    }
}

bool SidecarOcr::Start() {
    if (process_ && process_->Running()) return true;
    Stop();
    if (options_.command.empty()) return Fail("no OCR sidecar command configured");
    std::string error;
    process_ = launcher_.Start(options_.command, error);
    if (!process_) return Fail("cannot start OCR sidecar: " + error);
    // Skip any noise before the ready line (bounded by the start timeout as a whole).
    for (int lines = 0; lines < 50; ++lines) {
        const auto line = process_->ReadLine(options_.start_timeout);
        if (!line) break;
        try {
            const json ready = json::parse(*line);
            if (ready.is_object() && ready.value("ready", false)) {
                engine_ = ready.value("engine", std::string("?")) + " " +
                          ready.value("version", std::string(""));
                Log(log_, LogLevel::kInfo, "ocr: sidecar ready (" + engine_ + ")");
                last_error_.clear();
                started_once_ = true;
                return true;
            }
            if (ready.is_object() && ready.contains("error")) {
                const std::string why = ready.value("error", std::string());
                Stop();
                return Fail("OCR sidecar failed to start: " + why);
            }
        } catch (const json::exception&) {
            // not JSON: ignore
        }
    }
    Stop();
    return Fail("OCR sidecar did not report ready");
}

std::vector<OcrResult> SidecarOcr::Recognize(const Frame& frame, const Rect& roi,
                                             const std::string& language) {
    std::vector<OcrResult> results;
    const Language* lang = languages_.Find(language);
    if (!lang) {
        Fail("unknown language: " + language);
        return results;
    }
    const Frame crop = CropFrame(frame, roi);
    if (crop.empty()) {
        Fail("empty OCR region");
        return results;
    }
    const Point offset = IsValid(roi) ? Point{std::max(roi.x, 0), std::max(roi.y, 0)} : Point{};

    for (int attempt = 0; attempt <= options_.max_restarts; ++attempt) {
        if (!process_ || !process_->Running()) {
            if (started_once_) {
                if (restarts_ >= options_.max_restarts) {
                    Fail("OCR sidecar stopped; restart limit reached");
                    return results;
                }
                ++restarts_;
                Log(log_, LogLevel::kWarn, "ocr: restarting sidecar");
            }
            if (!Start()) return results;
        }
        const int id = next_id_++;
        if (!process_->WriteLine(EncodeRecognizeRequest(id, crop, lang->ocr_lang))) {
            Stop();
            continue;
        }
        // Responses to earlier, timed-out requests may still arrive: skip them (bounded).
        for (int skipped = 0; skipped < 8; ++skipped) {
            const auto line = process_->ReadLine(options_.request_timeout);
            if (!line) {
                Fail("OCR request timed out");
                Stop();
                return results;
            }
            std::string error;
            if (DecodeRecognizeResponse(*line, id, offset, results, error)) {
                last_error_.clear();
                return results;
            }
            if (error != "response for another request") {
                Fail(error);
                return results;
            }
        }
        Fail("no response for the OCR request");
        return results;
    }
    Fail("OCR sidecar is not available");
    return results;
}

}  // namespace maba
