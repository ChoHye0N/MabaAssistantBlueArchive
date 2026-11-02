// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Derived from the project owner's earlier project (SendInput mouse click and scan-code key press);
// see docs/SOURCES.md.
// Changes: virtual-desktop absolute coordinates (multi-monitor), move/down/up split so callers can
// pace them, return values checked.
#ifdef _WIN32

#include "send_input.h"

#include "maba/geometry.h"

#pragma comment(lib, "user32.lib")

namespace maba::win {

Rect VirtualDesktop() {
    return {GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN),
            GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN)};
}

bool MoveCursorTo(POINT screen) {
    const Point abs = ToAbsoluteInput({screen.x, screen.y}, VirtualDesktop());
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dx = abs.x;
    in.mi.dy = abs.y;
    in.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
    return SendInput(1, &in, sizeof(INPUT)) == 1;
}

bool LeftButton(bool down) {
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
    return SendInput(1, &in, sizeof(INPUT)) == 1;
}

bool PressScanCode(WORD scan) {
    INPUT in[2]{};
    in[0].type = in[1].type = INPUT_KEYBOARD;
    in[0].ki.wScan = in[1].ki.wScan = scan;
    in[0].ki.dwFlags = KEYEVENTF_SCANCODE;
    in[1].ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
    return SendInput(2, in, sizeof(INPUT)) == 2;
}

}  // namespace maba::win

#endif  // _WIN32
