// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Written from docs/notes/controllers.md. The behaviors noted there (CRLF repair, 12/16-byte
// raw header with an alpha check, landscape touch limits) follow the emulator controller of an
// AGPL-3.0 project; see docs/SOURCES.md.
#include "maba/adb.h"

#include <algorithm>
#include <sstream>

namespace maba::adb {

namespace {

std::string Trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    const auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

std::uint32_t ReadLe32(const std::string& d, std::size_t at) {
    return static_cast<std::uint32_t>(static_cast<unsigned char>(d[at])) |
           static_cast<std::uint32_t>(static_cast<unsigned char>(d[at + 1])) << 8 |
           static_cast<std::uint32_t>(static_cast<unsigned char>(d[at + 2])) << 16 |
           static_cast<std::uint32_t>(static_cast<unsigned char>(d[at + 3])) << 24;
}

std::optional<Size> ParseSizeAfter(const std::string& text, const std::string& label) {
    const auto pos = text.find(label);
    if (pos == std::string::npos) return std::nullopt;
    std::istringstream in(text.substr(pos + label.size()));
    int w = 0, h = 0;
    char x = 0;
    in >> w >> x >> h;
    if (!in || x != 'x' || w <= 0 || h <= 0) return std::nullopt;
    return Size{w, h};
}

}  // namespace

std::vector<Device> ParseDevices(const std::string& text) {
    std::vector<Device> out;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        line = Trim(line);
        if (line.empty() || line.rfind("List of devices", 0) == 0 || line[0] == '*') continue;
        std::istringstream fields(line);
        Device d;
        fields >> d.serial >> d.state;
        if (!d.serial.empty() && !d.state.empty()) out.push_back(d);
    }
    return out;
}

std::optional<Size> ParseWmSize(const std::string& text) {
    if (auto s = ParseSizeAfter(text, "Override size:")) return s;
    if (auto s = ParseSizeAfter(text, "Physical size:")) return s;
    return std::nullopt;
}

bool ConvertCrlf(std::string& data) {
    if (data.find("\r\n") == std::string::npos) return false;
    std::string out;
    out.reserve(data.size());
    for (std::size_t i = 0; i < data.size(); ++i) {
        if (data[i] == '\r' && i + 1 < data.size() && data[i + 1] == '\n') continue;
        out += data[i];
    }
    data.swap(out);
    return true;
}

std::optional<Frame> DecodeRawScreencap(const std::string& data) {
    if (data.size() < 12) return std::nullopt;
    const std::uint32_t w = ReadLe32(data, 0);
    const std::uint32_t h = ReadLe32(data, 4);
    if (w == 0 || h == 0 || w > 8192 || h > 8192) return std::nullopt;
    const std::size_t pixels = static_cast<std::size_t>(w) * h * 4;
    if (data.size() < pixels + 12) return std::nullopt;
    const std::size_t header = data.size() - pixels;
    if (header != 12 && header != 16) return std::nullopt;

    const auto* src = reinterpret_cast<const unsigned char*>(data.data()) + header;
    if (src[pixels - 1] != 255) return std::nullopt;  // last pixel must be opaque
    Frame f;
    f.width = static_cast<int>(w);
    f.height = static_cast<int>(h);
    f.bgr.resize(static_cast<std::size_t>(w) * h * 3);
    for (std::size_t i = 0, j = 0; i < pixels; i += 4, j += 3) {
        f.bgr[j] = src[i + 2];
        f.bgr[j + 1] = src[i + 1];
        f.bgr[j + 2] = src[i];
    }
    return f;
}

Size Landscape(Size s) { return {std::max(s.width, s.height), std::min(s.width, s.height)}; }

std::vector<std::string> ConnectCommand(const std::string& adb, const std::string& serial) {
    return {adb, "connect", serial};
}

std::vector<std::string> DevicesCommand(const std::string& adb) { return {adb, "devices"}; }

std::vector<std::string> ShellCommand(const std::string& adb, const std::string& serial,
                                      const std::string& remote_command) {
    return {adb, "-s", serial, "shell", remote_command};
}

std::vector<std::string> ExecOutCommand(const std::string& adb, const std::string& serial,
                                        const std::string& remote_command) {
    return {adb, "-s", serial, "exec-out", remote_command};
}

std::vector<std::string> PushCommand(const std::string& adb, const std::string& serial,
                                     const std::string& local, const std::string& remote) {
    return {adb, "-s", serial, "push", local, remote};
}

std::string TapRemote(Point p) {
    return "input tap " + std::to_string(p.x) + " " + std::to_string(p.y);
}

std::string SwipeRemote(Point from, Point to, int duration_ms) {
    return "input swipe " + std::to_string(from.x) + " " + std::to_string(from.y) + " " +
           std::to_string(to.x) + " " + std::to_string(to.y) + " " +
           std::to_string(std::max(duration_ms, 1));
}

std::string KeyEventRemote(int keycode) { return "input keyevent " + std::to_string(keycode); }

bool ParseTouchHeaderLine(const std::string& raw, TouchLimits& limits) {
    const std::string line = Trim(raw);
    if (line.empty() || line[0] != '^') return false;
    std::istringstream in(line.substr(1));
    int contacts = 0, a = 0, b = 0, pressure = 0;
    in >> contacts >> a >> b >> pressure;
    if (!in || contacts <= 0 || a <= 0 || b <= 0 || pressure < 0) return false;
    // Some emulators report the sizes swapped; the game is landscape, so the larger is x.
    limits.max_contacts = contacts;
    limits.max_x = std::max(a, b);
    limits.max_y = std::min(a, b);
    limits.max_pressure = pressure;
    return true;
}

Point ToTouch(Point native, Size screen, const TouchLimits& limits) {
    if (screen.width <= 0 || screen.height <= 0 || !limits.valid()) return {};
    const auto scale = [](int v, int from, int to) {
        const long long r = (static_cast<long long>(v) * to + from / 2) / from;
        return static_cast<int>(std::clamp<long long>(r, 0, to));
    };
    return {scale(native.x, screen.width, limits.max_x), scale(native.y, screen.height, limits.max_y)};
}

namespace {

int Pressure(const TouchLimits& limits) { return limits.max_pressure > 0 ? limits.max_pressure : 1; }

std::string Down(Point p, const TouchLimits& l) {
    return "d 0 " + std::to_string(p.x) + " " + std::to_string(p.y) + " " +
           std::to_string(Pressure(l)) + "\nc\n";
}

std::string Move(Point p, const TouchLimits& l) {
    return "m 0 " + std::to_string(p.x) + " " + std::to_string(p.y) + " " +
           std::to_string(Pressure(l)) + "\nc\n";
}

std::string Wait(int ms) { return ms > 0 ? "w " + std::to_string(ms) + "\n" : std::string(); }

}  // namespace

std::string TouchTap(Point touch, int hold_ms, const TouchLimits& limits) {
    return Down(touch, limits) + Wait(hold_ms) + "u 0\nc\n";
}

std::string TouchSwipe(const std::vector<Point>& path, int step_ms, const TouchLimits& limits) {
    if (path.empty()) return {};
    std::string out = Down(path.front(), limits);
    for (std::size_t i = 1; i < path.size(); ++i) out += Wait(step_ms) + Move(path[i], limits);
    return out + Wait(step_ms) + "u 0\nc\n";
}

std::string TouchKey(int keycode) {
    const std::string k = std::to_string(keycode);
    return "k " + k + " d\nc\nk " + k + " u\nc\n";
}

}  // namespace maba::adb
