// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Derived from the project owner's earlier project (template loading with an alpha mask,
// single and multiple template matching with ROI); see docs/SOURCES.md.
// Changes: normalized 1280x720 space, no global state, Unicode-safe file I/O, NaN-safe scores.
#include "maba/vision.h"

#include <algorithm>
#include <fstream>
#include <iterator>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "maba/geometry.h"
#include "maba/template_resolver.h"

namespace maba {

namespace fs = std::filesystem;

namespace {

std::vector<unsigned char> ReadFileBytes(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

cv::Mat DecodeFile(const fs::path& path, int flags) {
    const auto bytes = ReadFileBytes(path);
    if (bytes.empty()) return {};
    return cv::imdecode(bytes, flags);
}

Rect FrameRect() { return {0, 0, kNormWidth, kNormHeight}; }

bool IsNormalizedFrame(const Frame& f) {
    return !f.empty() && f.width == kNormWidth && f.height == kNormHeight &&
           f.bgr.size() == static_cast<std::size_t>(f.width) * f.height * 3;
}

// Search region inside the frame, or empty if unusable for this template.
Rect SearchRegion(const Template& templ, std::optional<Rect> roi) {
    const Rect region = roi ? Intersect(*roi, FrameRect()) : FrameRect();
    if (!IsValid(region) || region.width < templ.bgr.cols || region.height < templ.bgr.rows) return {};
    return region;
}

cv::Mat MatchScores(const Frame& frame, const Template& templ, const Rect& region) {
    const cv::Mat search = FrameView(frame)(cv::Rect(region.x, region.y, region.width, region.height));
    cv::Mat scores;
    if (templ.mask.empty()) {
        cv::matchTemplate(search, templ.bgr, scores, cv::TM_CCOEFF_NORMED);
    } else {
        cv::matchTemplate(search, templ.bgr, scores, cv::TM_CCOEFF_NORMED, templ.mask);
    }
    // Flat regions can yield NaN/inf with normalized methods; treat them as no match. Masked
    // matching divides by the variance under the mask, so far-out values are not perfect matches.
    cv::patchNaNs(scores, 0.0);
    scores.setTo(0.0, scores > 1.0 + 1e-5);
    scores.setTo(0.0, scores < -1.0 - 1e-5);
    cv::threshold(scores, scores, 1.0, 1.0, cv::THRESH_TRUNC);
    return scores;
}

MatchResult MakeResult(const Template& templ, const Rect& region, cv::Point loc, double score,
                       double threshold) {
    MatchResult r;
    r.score = score;
    r.box = {region.x + loc.x, region.y + loc.y, templ.bgr.cols, templ.bgr.rows};
    r.center = {r.box.x + r.box.width / 2, r.box.y + r.box.height / 2};
    r.found = score >= threshold;
    return r;
}

}  // namespace

cv::Mat FrameView(const Frame& frame) {
    if (frame.empty()) return {};
    return cv::Mat(frame.height, frame.width, CV_8UC3,
                   const_cast<std::uint8_t*>(frame.bgr.data()));
}

Frame FrameFromMat(const cv::Mat& bgr) {
    Frame f;
    if (bgr.empty() || bgr.type() != CV_8UC3) return f;
    const cv::Mat contiguous = bgr.isContinuous() ? bgr : bgr.clone();
    f.width = contiguous.cols;
    f.height = contiguous.rows;
    f.bgr.assign(contiguous.data, contiguous.data + contiguous.total() * 3);
    return f;
}

std::optional<Frame> LoadPng(const fs::path& path) {
    const cv::Mat img = DecodeFile(path, cv::IMREAD_COLOR);
    if (img.empty()) return std::nullopt;
    return FrameFromMat(img);
}

bool SavePng(const Frame& frame, const fs::path& path) {
    if (frame.empty()) return false;
    std::vector<unsigned char> bytes;
    if (!cv::imencode(".png", FrameView(frame), bytes)) return false;
    std::error_code ec;
    if (path.has_parent_path()) fs::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(out);
}

std::optional<Template> LoadTemplate(const fs::path& path) {
    const cv::Mat raw = DecodeFile(path, cv::IMREAD_UNCHANGED);
    if (raw.empty() || raw.depth() != CV_8U) return std::nullopt;

    Template t;
    switch (raw.channels()) {
        case 1: cv::cvtColor(raw, t.bgr, cv::COLOR_GRAY2BGR); break;
        case 3: t.bgr = raw; break;
        case 4: {
            cv::cvtColor(raw, t.bgr, cv::COLOR_BGRA2BGR);
            cv::Mat alpha;
            cv::extractChannel(raw, alpha, 3);
            cv::threshold(alpha, t.mask, 0, 255, cv::THRESH_BINARY);
            const int opaque = cv::countNonZero(t.mask);
            if (opaque == 0) return std::nullopt;  // nothing left to match
            if (opaque == static_cast<int>(t.mask.total())) t.mask.release();
            break;
        }
        default: return std::nullopt;
    }
    return t;
}

Frame NormalizeFrame(const cv::Mat& native_bgr) {
    if (native_bgr.empty() || native_bgr.type() != CV_8UC3) return {};
    if (native_bgr.cols == kNormWidth && native_bgr.rows == kNormHeight) return FrameFromMat(native_bgr);
    const bool shrinking = native_bgr.cols > kNormWidth || native_bgr.rows > kNormHeight;
    cv::Mat scaled;
    cv::resize(native_bgr, scaled, cv::Size(kNormWidth, kNormHeight), 0, 0,
               shrinking ? cv::INTER_AREA : cv::INTER_LINEAR);
    return FrameFromMat(scaled);
}

MatchResult FindTemplate(const Frame& frame, const Template& templ, std::optional<Rect> roi,
                         double threshold) {
    if (!IsNormalizedFrame(frame) || templ.bgr.empty()) return {};
    const Rect region = SearchRegion(templ, roi);
    if (!IsValid(region)) return {};

    const cv::Mat scores = MatchScores(frame, templ, region);
    double best = 0;
    cv::Point loc;
    cv::minMaxLoc(scores, nullptr, &best, nullptr, &loc);
    return MakeResult(templ, region, loc, best, threshold);
}

std::vector<MatchResult> FindAllTemplates(const Frame& frame, const Template& templ,
                                          std::optional<Rect> roi, double threshold,
                                          int max_count) {
    std::vector<MatchResult> results;
    if (max_count <= 0 || !IsNormalizedFrame(frame) || templ.bgr.empty()) return results;
    const Rect region = SearchRegion(templ, roi);
    if (!IsValid(region)) return results;

    cv::Mat scores = MatchScores(frame, templ, region);
    const cv::Rect all(0, 0, scores.cols, scores.rows);
    while (static_cast<int>(results.size()) < max_count) {
        double best = 0;
        cv::Point loc;
        cv::minMaxLoc(scores, nullptr, &best, nullptr, &loc);
        if (best < threshold) break;
        results.push_back(MakeResult(templ, region, loc, best, threshold));
        // Suppress every position whose box would overlap this match.
        const cv::Rect suppress(loc.x - templ.bgr.cols + 1, loc.y - templ.bgr.rows + 1,
                                templ.bgr.cols * 2 - 1, templ.bgr.rows * 2 - 1);
        scores(suppress & all).setTo(-1.0);
    }
    return results;
}

OpenCvMatcher::OpenCvMatcher(fs::path repo_root, std::string lang)
    : repo_root_(std::move(repo_root)), lang_(std::move(lang)) {}

const Template* OpenCvMatcher::Get(const std::string& name) {
    if (auto it = cache_.find(name); it != cache_.end() && it->second) return &*it->second;
    const fs::path path = ResolveTemplate(repo_root_, lang_, name);
    if (path.empty()) {
        last_error_ = "template not found: " + name + " (lang " + lang_ + ")";
        return nullptr;
    }
    auto templ = LoadTemplate(path);
    if (!templ) {
        last_error_ = "cannot decode template: " + path.generic_string();
        return nullptr;
    }
    auto& slot = cache_[name];
    slot = std::move(templ);  // only successes are cached, so a file added later is picked up
    return &*slot;
}

bool OpenCvMatcher::CheckFrame(const Frame& frame) {
    if (IsNormalizedFrame(frame)) return true;
    last_error_ = "frame is not 1280x720 (" + std::to_string(frame.width) + "x" +
                  std::to_string(frame.height) + ")";
    return false;
}

MatchResult OpenCvMatcher::Find(const Frame& frame, const TemplateRef& ref) {
    if (!CheckFrame(frame)) return {};
    const Template* templ = Get(ref.file);
    if (!templ) return {};
    return FindTemplate(frame, *templ, ref.roi, ref.threshold);
}

std::vector<MatchResult> OpenCvMatcher::FindAll(const Frame& frame, const TemplateRef& ref,
                                                int max_count) {
    if (!CheckFrame(frame)) return {};
    const Template* templ = Get(ref.file);
    if (!templ) return {};
    return FindAllTemplates(frame, *templ, ref.roi, ref.threshold, max_count);
}

}  // namespace maba
