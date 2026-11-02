// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Real mouse and keyboard input through SendInput (moves the actual cursor).
// Derived from the project owner's earlier project; see docs/SOURCES.md.
#pragma once

#ifdef _WIN32

#include <windows.h>

#include "maba/types.h"

namespace maba::win {

// Bounding rect of all monitors, in physical pixels when called inside a DpiAwareScope.
Rect VirtualDesktop();

// Move the cursor to a screen point (works across monitors, including negative coordinates).
bool MoveCursorTo(POINT screen);

bool LeftButton(bool down);

// Press and release a key by hardware scan code (e.g. 0x01 = Esc).
bool PressScanCode(WORD scan);

}  // namespace maba::win

#endif  // _WIN32
