// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Command-line driver for the Steam PC controller (Windows only).
//   maba_cli info
//   maba_cli grab  [--capture bitblt|printwindow] [--out FILE] [--region x,y,w,h]
//   maba_cli smoke [--capture ...] [--lang ko] [--root DIR] [--dry-run]
#ifdef _WIN32

#include <cstdio>
#include <ctime>
#include <cwchar>
#include <filesystem>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <windows.h>

#include "maba/page.h"
#include "maba/smoke.h"
#include "maba/types.h"
#include "maba/vision.h"
#include "steam_controller.h"

namespace fs = std::filesystem;

namespace {

using Args = std::map<std::string, std::string>;

std::string Narrow(const wchar_t* w) {
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    std::string s(n > 0 ? n - 1 : 0, '\0');
    if (n > 1) WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
    return s;
}

// --key value pairs; flags without a value get "1".
bool ParseArgs(int argc, wchar_t** argv, int first, Args& out) {
    for (int i = first; i < argc; ++i) {
        const std::string key = Narrow(argv[i]);
        if (key.rfind("--", 0) != 0) {
            std::fprintf(stderr, "unexpected argument: %s\n", key.c_str());
            return false;
        }
        const bool has_value = i + 1 < argc && std::wcsncmp(argv[i + 1], L"--", 2) != 0;
        out[key.substr(2)] = has_value ? Narrow(argv[++i]) : "1";
    }
    return true;
}

std::string Get(const Args& a, const std::string& key, const std::string& fallback) {
    auto it = a.find(key);
    return it == a.end() ? fallback : it->second;
}

bool ParseCapture(const Args& a, maba::win::CaptureMethod& out) {
    const std::string v = Get(a, "capture", "bitblt");
    if (v == "bitblt") out = maba::win::CaptureMethod::BitBlt;
    else if (v == "printwindow") out = maba::win::CaptureMethod::PrintWindow;
    else {
        std::fprintf(stderr, "--capture must be bitblt or printwindow\n");
        return false;
    }
    return true;
}

bool ParseRect(const std::string& s, maba::Rect& r) {
    std::istringstream in(s);
    char c1 = 0, c2 = 0, c3 = 0;
    in >> r.x >> c1 >> r.y >> c2 >> r.width >> c3 >> r.height;
    return in && in.peek() == std::char_traits<char>::eof() && c1 == ',' && c2 == ',' &&
           c3 == ',' && r.width > 0 && r.height > 0;
}

std::string Timestamp() {
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &t);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tm);
    return buf;
}

fs::path Utf8Path(const std::string& s) { return fs::u8path(s); }

int CmdInfo() {
    maba::win::SteamController controller;
    if (!controller.Connect()) {
        std::fprintf(stderr, "%s\n", controller.LastError().c_str());
        return 1;
    }
    for (auto method : {maba::win::CaptureMethod::BitBlt, maba::win::CaptureMethod::PrintWindow}) {
        controller.SetCaptureMethod(method);
        const maba::Frame f = controller.Screenshot();
        const maba::Size n = controller.NativeSize();
        if (f.empty()) {
            std::printf("%-12s FAILED: %s\n", maba::win::ToString(method),
                        controller.LastError().c_str());
        } else {
            std::printf("%-12s ok: native client %dx%d -> %dx%d\n", maba::win::ToString(method),
                        n.width, n.height, f.width, f.height);
        }
    }
    return 0;
}

int CmdGrab(const Args& args) {
    maba::win::SteamControllerOptions options;
    if (!ParseCapture(args, options.capture)) return 2;
    maba::win::SteamController controller(options);

    maba::Frame frame;
    const std::string region = Get(args, "region", "");
    if (region.empty()) {
        frame = controller.Screenshot();
    } else {
        maba::Rect r;
        if (!ParseRect(region, r)) {
            std::fprintf(stderr, "--region must be x,y,w,h in 1280x720 coordinates\n");
            return 2;
        }
        frame = controller.CaptureRegion(r);
    }
    if (frame.empty()) {
        std::fprintf(stderr, "capture failed: %s\n", controller.LastError().c_str());
        return 1;
    }
    const fs::path out = Utf8Path(Get(args, "out", "captures/grab_" + Timestamp() + ".png"));
    if (!maba::SavePng(frame, out)) {
        std::fprintf(stderr, "cannot write %s\n", out.u8string().c_str());
        return 1;
    }
    std::printf("saved %dx%d frame (%s) to %s\n", frame.width, frame.height,
                region.empty() ? "normalized" : "native region", out.u8string().c_str());
    return 0;
}

int CmdSmoke(const Args& args) {
    maba::win::SteamControllerOptions controller_options;
    if (!ParseCapture(args, controller_options.capture)) return 2;
    const fs::path root = Utf8Path(Get(args, "root", "."));
    const std::string lang = Get(args, "lang", "ko");

    const maba::PageSetLoadResult pages = maba::LoadPageDir(root / "pages");
    if (!pages.ok) {
        std::fprintf(stderr, "cannot load pages: %s (run from the repository root or pass --root)\n",
                     pages.error.c_str());
        return 1;
    }

    maba::win::SteamController controller(controller_options);
    maba::OpenCvMatcher matcher(root, lang);
    maba::SteadyClock clock;
    maba::SmokeOptions options;
    options.dry_run = args.count("dry-run") > 0;
    options.log = [](const std::string& msg) { std::printf("%s\n", msg.c_str()); };
    options.on_failure = [&](const maba::Frame& frame, const std::string&) {
        if (!controller.LastError().empty())
            std::printf("controller: %s\n", controller.LastError().c_str());
        if (frame.empty()) return;
        // Failure frames stay under captures/ (gitignored): they may show personal data.
        const fs::path out = root / "captures" / ("smoke_fail_" + Timestamp() + ".png");
        if (maba::SavePng(frame, out)) std::printf("saved failure frame to %s\n", out.u8string().c_str());
    };

    std::printf("smoke: %s -> %s, capture %s, lang %s%s\n", options.from_page.c_str(),
                options.to_page.c_str(), maba::win::ToString(controller_options.capture),
                lang.c_str(), options.dry_run ? ", dry run" : "");
    const maba::SmokeResult r =
        maba::RunTransitionSmoke(controller, matcher, pages.pages, options, clock);
    std::printf("%s\n", r.ok ? "PASS" : "FAIL");
    return r.ok ? 0 : 1;
}

void Usage() {
    std::fprintf(stderr,
                 "usage:\n"
                 "  maba_cli info\n"
                 "  maba_cli grab [--capture bitblt|printwindow] [--out FILE] [--region x,y,w,h]\n"
                 "  maba_cli smoke [--capture bitblt|printwindow] [--lang ko] [--root DIR] [--dry-run]\n");
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    SetConsoleOutputCP(CP_UTF8);
    if (argc < 2) {
        Usage();
        return 2;
    }
    const std::string cmd = Narrow(argv[1]);
    Args args;
    if (!ParseArgs(argc, argv, 2, args)) return 2;
    if (cmd == "info") return CmdInfo();
    if (cmd == "grab") return CmdGrab(args);
    if (cmd == "smoke") return CmdSmoke(args);
    Usage();
    return 2;
}

#endif  // _WIN32
