// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// OCR sidecar protocol and client against a fake peer.
#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS  // std::getenv in a test
#endif
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include <nlohmann/json.hpp>

#include "fakes.h"
#include "maba/assistant.h"
#include "maba/ocr_sidecar.h"
#include "test_framework.h"

namespace {

maba::LanguageConfig Langs() {
    maba::LanguageConfig c;
    c.languages = {{"ko", "한국어", "confirmed", "korean"}, {"en", "English", "confirmed", "en"}};
    return c;
}

MABA_TEST(Base64MatchesRfc4648) {
    auto enc = [](const std::string& s) {
        return maba::Base64Encode(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
    };
    CHECK(enc("") == "");
    CHECK(enc("f") == "Zg==");
    CHECK(enc("fo") == "Zm8=");
    CHECK(enc("foo") == "Zm9v");
    CHECK(enc("foobar") == "Zm9vYmFy");
}

MABA_TEST(CropFrameClampsRoi) {
    maba::Frame f = maba_test::BlankFrame(10, 10);
    f.bgr[(5 * 10 + 5) * 3] = 7;
    const maba::Frame c = maba::CropFrame(f, {5, 5, 20, 20});
    CHECK(c.width == 5 && c.height == 5 && c.bgr[0] == 7);
    CHECK(maba::CropFrame(f, {}).width == 10);
    CHECK(maba::CropFrame(f, {20, 20, 5, 5}).empty());
}

MABA_TEST(RequestAndResponseRoundTrip) {
    const maba::Frame img = maba_test::BlankFrame(2, 1, 1);
    const auto req = nlohmann::json::parse(maba::EncodeRecognizeRequest(7, img, "korean"));
    CHECK(req["id"] == 7 && req["lang"] == "korean" && req["width"] == 2 && req["format"] == "bgr24");
    CHECK(req["data"] == "AQEBAQEB");

    std::vector<maba::OcrResult> out;
    std::string error;
    CHECK(maba::DecodeRecognizeResponse(
        R"({"id": 7, "ok": true, "results": [{"text": "120", "box": [1, 2, 3, 4], "score": 0.5}]})", 7,
        {100, 200}, out, error));
    CHECK(out.size() == 1 && out[0].text == "120" && out[0].box.x == 101 && out[0].box.y == 202);
    CHECK(!maba::DecodeRecognizeResponse(R"({"id": 6, "ok": true, "results": []})", 7, {}, out, error));
    CHECK(!maba::DecodeRecognizeResponse(R"({"id": 7, "ok": false, "error": "boom"})", 7, {}, out, error));
    CHECK(error == "boom");
    CHECK(!maba::DecodeRecognizeResponse("not json", 7, {}, out, error));
}

// A fake sidecar that answers every request with the text "42".
std::unique_ptr<maba_test::FakeInteractive> Peer(int* requests = nullptr) {
    auto p = std::make_unique<maba_test::FakeInteractive>();
    p->pending.push_back("loading models...");
    p->pending.push_back(R"({"ready": true, "engine": "fake", "version": "1"})");
    p->on_line = [requests](const std::string& line, maba_test::FakeInteractive& self) {
        const auto req = nlohmann::json::parse(line);
        if (requests) ++*requests;
        nlohmann::json res = {{"id", req["id"]},
                              {"ok", true},
                              {"results", {{{"text", "42"}, {"box", {0, 0, 5, 5}}, {"score", 0.9}}}}};
        self.pending.push_back(res.dump());
    };
    return p;
}

MABA_TEST(SidecarRecognizesThroughPeer) {
    maba_test::FakeLauncher launcher;
    int requests = 0;
    launcher.make = [&] { return Peer(&requests); };
    maba::SidecarOcr ocr(launcher, {{"python", "server.py"}}, Langs());
    const auto r = ocr.Recognize(maba_test::BlankFrame(100, 50), {10, 10, 20, 20}, "ko");
    CHECK(r.size() == 1 && r[0].text == "42" && r[0].box.x == 10);
    CHECK(ocr.EngineInfo() == "fake 1");
    CHECK(requests == 1);
    CHECK(ocr.LastError().empty());
    CHECK(launcher.last_argv.size() == 2 && launcher.last_argv[1] == "server.py");
}

MABA_TEST(SidecarRejectsUnknownLanguage) {
    maba_test::FakeLauncher launcher;
    launcher.make = [] { return Peer(); };
    maba::SidecarOcr ocr(launcher, {{"python"}}, Langs());
    CHECK(ocr.Recognize(maba_test::BlankFrame(10, 10), {}, "xx").empty());
    CHECK(ocr.LastError().find("unknown language") != std::string::npos);
    CHECK(launcher.starts == 0);
}

MABA_TEST(SidecarReportsStartupError) {
    maba_test::FakeLauncher launcher;
    launcher.make = [] {
        auto p = std::make_unique<maba_test::FakeInteractive>();
        p->pending.push_back(R"({"error": "No module named 'rapidocr'"})");
        return p;
    };
    maba::SidecarOcr ocr(launcher, {{"python"}}, Langs());
    CHECK(ocr.Recognize(maba_test::BlankFrame(10, 10), {}, "ko").empty());
    CHECK(ocr.LastError().find("rapidocr") != std::string::npos);
}

MABA_TEST(SidecarRestartsAfterCrashWithinLimit) {
    maba_test::FakeLauncher launcher;
    maba_test::FakeInteractive* current = nullptr;
    launcher.make = [&] {
        auto p = Peer();
        current = p.get();
        return p;
    };
    maba::SidecarOptions options{{"python"}};
    options.max_restarts = 1;
    maba::SidecarOcr ocr(launcher, options, Langs());
    CHECK(ocr.Recognize(maba_test::BlankFrame(10, 10), {}, "ko").size() == 1);
    current->running = false;  // crash
    CHECK(ocr.Recognize(maba_test::BlankFrame(10, 10), {}, "ko").size() == 1);
    CHECK(launcher.starts == 2);
    current->running = false;  // crash again: limit reached
    CHECK(ocr.Recognize(maba_test::BlankFrame(10, 10), {}, "ko").empty());
    CHECK(ocr.LastError().find("restart limit") != std::string::npos);
}

MABA_TEST(DefaultOcrCommandFindsBundledPython) {
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path root = fs::temp_directory_path() / "maba_ocr_cmd_test";
    fs::remove_all(root, ec);
    CHECK(maba::DefaultOcrCommand(root).front() == "python");  // nothing bundled: PATH
#ifdef _WIN32
    fs::create_directories(root / "ocr" / "python", ec);
    std::ofstream(root / "ocr" / "python" / "python.exe") << "";
    const auto bundled = maba::DefaultOcrCommand(root);
    CHECK(bundled.front().find("python") != std::string::npos && bundled.front() != "python");
    fs::create_directories(root / "ocr" / ".venv" / "Scripts", ec);
    std::ofstream(root / "ocr" / ".venv" / "Scripts" / "python.exe") << "";
    CHECK(maba::DefaultOcrCommand(root).front().find(".venv") != std::string::npos);  // venv wins
    // A release: the data folder has ocr/ beside it.
    fs::remove_all(root / "ocr" / ".venv", ec);
    fs::create_directories(root / "data", ec);
    const auto release = maba::DefaultOcrCommand(root / "data");
    CHECK(release.front() == bundled.front());
#endif
    fs::remove_all(root, ec);
}

// End to end with the real Python sidecar (fake engine, so no models are needed).
// Runs only when MABA_PYTHON_TESTS=1, because it needs Python on PATH or ocr/.venv.
MABA_TEST(SidecarTalksToPythonServer) {
    const char* enabled = std::getenv("MABA_PYTHON_TESTS");
    if (!enabled || std::string(enabled) != "1") return;
    maba::SystemProcessLauncher launcher;
    auto command = maba::DefaultOcrCommand(MABA_SOURCE_DIR);
    command.insert(command.end(), {"--engine", "fake", "--fake-text", "1,234"});
    maba::SidecarOcr ocr(launcher, {command}, Langs());
    const auto r = ocr.Recognize(maba_test::BlankFrame(64, 32), {8, 4, 40, 20}, "ko");
    if (!ocr.LastError().empty()) std::printf("sidecar: %s\n", ocr.LastError().c_str());
    CHECK(r.size() == 1 && r[0].text == "1,234");
    CHECK(r.size() == 1 && r[0].box.x == 8 && r[0].box.width == 40);
    CHECK(ocr.EngineInfo() == "fake 1");
}

}  // namespace
