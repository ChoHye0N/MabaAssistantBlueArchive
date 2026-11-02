// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Vision tests on synthetic fixtures (patterns drawn in code; no game images needed).
#include <filesystem>
#include <fstream>
#include <random>
#include <string>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "maba/geometry.h"
#include "maba/vision.h"
#include "test_framework.h"

namespace {

namespace fs = std::filesystem;

cv::Mat Noise(int w, int h, std::uint64_t seed) {
    cv::Mat m(h, w, CV_8UC3);
    cv::RNG rng(seed);
    rng.fill(m, cv::RNG::UNIFORM, cv::Scalar::all(0), cv::Scalar::all(256));
    return m;
}

// A distinctive patch: noise plus a few shapes so it does not correlate with background noise.
cv::Mat Patch(int w, int h, std::uint64_t seed) {
    cv::Mat p = Noise(w, h, seed);
    cv::rectangle(p, {2, 2}, {w / 2, h / 2}, cv::Scalar(255, 255, 255), cv::FILLED);
    cv::circle(p, {w * 3 / 4, h * 3 / 4}, std::min(w, h) / 5, cv::Scalar(0, 0, 255), cv::FILLED);
    return p;
}

maba::Frame FrameWith(const cv::Mat& patch, std::initializer_list<cv::Point> at,
                      std::uint64_t seed = 1) {
    cv::Mat bg = Noise(maba::kNormWidth, maba::kNormHeight, seed);
    for (const auto& p : at) patch.copyTo(bg(cv::Rect(p.x, p.y, patch.cols, patch.rows)));
    return maba::FrameFromMat(bg);
}

maba::Template Opaque(const cv::Mat& patch) { return {patch.clone(), {}}; }

struct TempDir {
    TempDir() {
        std::random_device rd;
        path = fs::temp_directory_path() / ("maba_vision_" + std::to_string(rd()));
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
    fs::path path;
};

MABA_TEST(VisionFindsCenter) {
    const cv::Mat patch = Patch(60, 40, 7);
    const auto frame = FrameWith(patch, {{900, 300}});
    const auto r = maba::FindTemplate(frame, Opaque(patch), std::nullopt, 0.9);
    CHECK(r.found);
    CHECK(r.score > 0.99);
    CHECK(r.box.x == 900 && r.box.y == 300 && r.box.width == 60 && r.box.height == 40);
    CHECK(r.center.x == 930 && r.center.y == 320);
}

MABA_TEST(VisionNotFoundReportsScore) {
    const cv::Mat patch = Patch(60, 40, 7);
    const auto frame = FrameWith(Patch(60, 40, 99), {{900, 300}});
    const auto r = maba::FindTemplate(frame, Opaque(patch), std::nullopt, 0.9);
    CHECK(!r.found && r.score < 0.9);
}

MABA_TEST(VisionRoiRestrictsSearch) {
    const cv::Mat patch = Patch(60, 40, 7);
    const auto frame = FrameWith(patch, {{900, 300}});
    const auto t = Opaque(patch);
    CHECK(!maba::FindTemplate(frame, t, maba::Rect{0, 0, 600, 720}, 0.9).found);
    const auto in = maba::FindTemplate(frame, t, maba::Rect{850, 250, 200, 200}, 0.9);
    CHECK(in.found && in.box.x == 900 && in.box.y == 300);  // offsets restored to frame space
}

MABA_TEST(VisionRoiClampedToFrame) {
    const cv::Mat patch = Patch(60, 40, 7);
    const auto frame = FrameWith(patch, {{1200, 660}});
    const auto r = maba::FindTemplate(frame, Opaque(patch), maba::Rect{1100, 600, 500, 500}, 0.9);
    CHECK(r.found && r.box.x == 1200 && r.box.y == 660);
    CHECK(!maba::FindTemplate(frame, Opaque(patch), maba::Rect{1300, 0, 50, 50}, 0.9).found);
}

MABA_TEST(VisionTemplateLargerThanRoi) {
    const cv::Mat patch = Patch(60, 40, 7);
    const auto frame = FrameWith(patch, {{100, 100}});
    const auto r = maba::FindTemplate(frame, Opaque(patch), maba::Rect{100, 100, 30, 30}, 0.5);
    CHECK(!r.found && r.score == 0);
}

MABA_TEST(VisionRejectsNonNormalizedFrame) {
    const cv::Mat patch = Patch(20, 20, 7);
    const auto frame = maba::FrameFromMat(Noise(1920, 1080, 3));
    CHECK(!maba::FindTemplate(frame, Opaque(patch), std::nullopt, 0.1).found);
}

MABA_TEST(VisionMaskIgnoresCoveredPixels) {
    // The masked band (e.g. a nickname) differs between template and screen.
    cv::Mat patch = Patch(80, 40, 11);
    cv::Mat on_screen = patch.clone();
    Noise(80, 12, 12345).copyTo(on_screen(cv::Rect(0, 28, 80, 12)));
    const auto frame = FrameWith(on_screen, {{400, 500}});

    maba::Template masked{patch, cv::Mat(40, 80, CV_8UC1, cv::Scalar(255))};
    masked.mask(cv::Rect(0, 28, 80, 12)).setTo(0);
    const auto r = maba::FindTemplate(frame, masked, std::nullopt, 0.97);
    CHECK(r.found && r.box.x == 400 && r.box.y == 500);
}

MABA_TEST(VisionFindAllSeparatesAndCaps) {
    const cv::Mat patch = Patch(40, 40, 21);
    const auto frame = FrameWith(patch, {{100, 100}, {300, 100}, {500, 100}, {700, 100}});
    const auto t = Opaque(patch);
    const auto all = maba::FindAllTemplates(frame, t, std::nullopt, 0.9, 10);
    CHECK(all.size() == 4);
    for (std::size_t i = 0; i < all.size(); ++i)
        for (std::size_t j = i + 1; j < all.size(); ++j)
            CHECK(maba::Intersect(all[i].box, all[j].box).width == 0);
    CHECK(maba::FindAllTemplates(frame, t, std::nullopt, 0.9, 2).size() == 2);
    CHECK(maba::FindAllTemplates(frame, t, std::nullopt, 0.9, 0).empty());
    CHECK(maba::FindAllTemplates(frame, t, maba::Rect{250, 50, 300, 200}, 0.9, 10).size() == 2);
}

MABA_TEST(VisionLoadTemplateAlphaAndIo) {
    TempDir dir;
    cv::Mat bgra(20, 30, CV_8UC4, cv::Scalar(10, 20, 30, 255));
    bgra(cv::Rect(0, 0, 10, 20)).setTo(cv::Scalar(0, 0, 0, 0));
    // Non-ASCII directory name: file I/O must not depend on the ANSI code page.
    const fs::path sub = dir.path / fs::u8path(u8"템플릿");
    fs::create_directories(sub);
    std::vector<unsigned char> png;
    CHECK(cv::imencode(".png", bgra, png));
    {
        std::ofstream(sub / "masked.png", std::ios::binary)
            .write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
    }
    const auto t = maba::LoadTemplate(sub / "masked.png");
    CHECK(t && t->bgr.cols == 30 && t->bgr.channels() == 3);
    CHECK(t && !t->mask.empty() && cv::countNonZero(t->mask) == 20 * 20);

    const auto frame = maba::FrameFromMat(Noise(64, 32, 5));
    CHECK(maba::SavePng(frame, sub / "out" / "frame.png"));
    const auto back = maba::LoadPng(sub / "out" / "frame.png");
    CHECK(back && back->width == 64 && back->bgr == frame.bgr);
    CHECK(!maba::LoadTemplate(sub / "missing.png"));
}

MABA_TEST(VisionNormalizeFrame) {
    const auto down = maba::NormalizeFrame(Noise(1920, 1080, 1));
    CHECK(down.width == maba::kNormWidth && down.height == maba::kNormHeight);
    const auto up = maba::NormalizeFrame(Noise(960, 540, 1));
    CHECK(up.width == maba::kNormWidth && up.height == maba::kNormHeight);
    CHECK(maba::NormalizeFrame(cv::Mat()).empty());
}

MABA_TEST(VisionMatcherResolvesAndReportsErrors) {
    TempDir root;
    const cv::Mat patch = Patch(50, 30, 31);
    fs::create_directories(root.path / "assets" / "common");
    CHECK(maba::SavePng(maba::FrameFromMat(patch), root.path / "assets" / "common" / "t_icon.png"));
    maba::OpenCvMatcher matcher(root.path, "ko");
    const auto frame = FrameWith(patch, {{10, 20}});

    const auto r = matcher.Find(frame, {"t_icon.png", std::nullopt, 0.9});
    CHECK(r.found && r.center.x == 35 && r.center.y == 35);
    CHECK(!matcher.Find(frame, {"missing.png", std::nullopt, 0.9}).found);
    CHECK(matcher.LastError().find("not found") != std::string::npos);
    CHECK(!matcher.Find(maba::Frame{}, {"t_icon.png", std::nullopt, 0.9}).found);
    CHECK(matcher.LastError().find("1280x720") != std::string::npos);
}

}  // namespace
