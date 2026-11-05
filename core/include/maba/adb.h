// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Portable pieces of the emulator (ADB) controller: command lines, output parsing, raw
// screencap decoding and the minitouch text protocol (spoken by MaaTouch). No OpenCV, no OS
// calls, so all of it is unit-tested. Behavior notes: docs/notes/controllers.md.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "maba/types.h"

namespace maba::adb {

struct Device {
    std::string serial;
    std::string state;  // "device", "offline", "unauthorized", ...
};

// `adb devices` output -> devices (header and empty lines skipped).
std::vector<Device> ParseDevices(const std::string& text);

// `adb shell wm size` output. An "Override size" wins over the "Physical size".
std::optional<Size> ParseWmSize(const std::string& text);

// Old adb versions on Windows turn "\n" into "\r\n" in shell output, which corrupts binary
// data. Replaces every "\r\n" with "\n". Returns false if there was none.
bool ConvertCrlf(std::string& data);

// `screencap` raw output: header (width, height, format[, colorspace], little endian, 12 or 16
// bytes) followed by width*height RGBA pixels. Returns a BGR frame at native size, or nullopt
// if the size does not add up or the alpha check fails (a hint for CRLF damage).
std::optional<Frame> DecodeRawScreencap(const std::string& data);

// The landscape orientation of a size: width >= height.
Size Landscape(Size s);

// Command lines (argv, no shell). `adb` is the adb executable, `serial` the device.
std::vector<std::string> ConnectCommand(const std::string& adb, const std::string& serial);
std::vector<std::string> DevicesCommand(const std::string& adb);
std::vector<std::string> ShellCommand(const std::string& adb, const std::string& serial,
                                      const std::string& remote_command);
std::vector<std::string> ExecOutCommand(const std::string& adb, const std::string& serial,
                                        const std::string& remote_command);
std::vector<std::string> PushCommand(const std::string& adb, const std::string& serial,
                                     const std::string& local, const std::string& remote);
// `input tap/swipe/keyevent` remote commands (native pixel coordinates).
std::string TapRemote(Point p);
std::string SwipeRemote(Point from, Point to, int duration_ms);
std::string KeyEventRemote(int keycode);

// minitouch protocol (MaaTouch implements it). Header lines sent by the helper on start:
//   "v <version>", "^ <max-contacts> <max-x> <max-y> <max-pressure>", "$ <pid>"
struct TouchLimits {
    int max_contacts = 0;
    int max_x = 0;
    int max_y = 0;
    int max_pressure = 0;

    bool valid() const { return max_contacts > 0 && max_x > 0 && max_y > 0; }
};

// Feeds one header line; returns true once "^" was seen with sane values.
bool ParseTouchHeaderLine(const std::string& line, TouchLimits& limits);

// Maps a native screen point (landscape, `screen` size) to touch-device coordinates.
// MaaTouch reports limits in the current orientation, so this is a plain scale.
Point ToTouch(Point native, Size screen, const TouchLimits& limits);

// Command batches, each ending with a commit. Pressure is clamped to the device maximum.
std::string TouchTap(Point touch, int hold_ms, const TouchLimits& limits);
std::string TouchSwipe(const std::vector<Point>& touch_path, int step_ms, const TouchLimits& limits);
// Key press through the helper ("k <keycode> d", "k <keycode> u"): a MaaTouch extension.
std::string TouchKey(int keycode);

}  // namespace maba::adb
