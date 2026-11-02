// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// OpenCV-based Vision. Only available when built with MABA_WITH_OPENCV.
// Template loading and matching are derived from the project owner's earlier project; see docs/SOURCES.md.
#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <opencv2/core.hpp>

#include "maba/matcher.h"
#include "maba/types.h"

namespace maba {

// A template image. Transparent pixels (alpha 0) are excluded from matching through `mask`.
struct Template {
    cv::Mat bgr;   // CV_8UC3
    cv::Mat mask;  // CV_8UC1 (0 or 255), empty if fully opaque
};

// Image conversion and I/O. Paths may contain non-ASCII characters.
cv::Mat FrameView(const Frame& frame);  // no copy; valid while `frame` lives
Frame FrameFromMat(const cv::Mat& bgr);  // copies a CV_8UC3 image
std::optional<Frame> LoadPng(const std::filesystem::path& path);
bool SavePng(const Frame& frame, const std::filesystem::path& path);
std::optional<Template> LoadTemplate(const std::filesystem::path& path);

// Scale a native capture to the normalized 1280x720 frame
// (area averaging when shrinking, bilinear when enlarging).
Frame NormalizeFrame(const cv::Mat& native_bgr);

// Matching on a 1280x720 frame. `roi` (normalized) is clamped to the frame.
// Not found if the search region is empty or smaller than the template.
MatchResult FindTemplate(const Frame& frame, const Template& templ, std::optional<Rect> roi,
                         double threshold);
std::vector<MatchResult> FindAllTemplates(const Frame& frame, const Template& templ,
                                          std::optional<Rect> roi, double threshold,
                                          int max_count);

// IMatcher backed by OpenCV. Resolves template names with ResolveTemplate(repo_root, lang)
// and caches decoded templates per name.
class OpenCvMatcher : public IMatcher {
public:
    OpenCvMatcher(std::filesystem::path repo_root, std::string lang);

    MatchResult Find(const Frame& frame, const TemplateRef& ref) override;
    std::vector<MatchResult> FindAll(const Frame& frame, const TemplateRef& ref,
                                     int max_count) override;
    const std::string& LastError() const override { return last_error_; }

private:
    const Template* Get(const std::string& name);
    bool CheckFrame(const Frame& frame);

    std::filesystem::path repo_root_;
    std::string lang_;
    std::map<std::string, std::optional<Template>> cache_;
    std::string last_error_;
};

}  // namespace maba
