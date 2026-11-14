// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// What Windows says about the display a window is on.
#pragma once

#ifdef _WIN32

#include <windows.h>

namespace maba::win {

// True when the display showing most of `hwnd` has HDR (advanced colour) turned on. With HDR on,
// a copy of the screen can come out with other colours than the game drew, and the colour
// conditions of the tasks are measured without it. False when Windows cannot tell.
bool WindowOnHdrDisplay(HWND hwnd);

}  // namespace maba::win

#endif  // _WIN32
