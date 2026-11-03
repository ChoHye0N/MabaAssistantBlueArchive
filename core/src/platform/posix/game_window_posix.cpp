// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// The Steam PC client runs on Windows only; outside Windows the game window never exists.
#ifndef _WIN32

#include <utility>

#include "maba/steam_launch.h"

namespace maba {

SystemGameWindow::SystemGameWindow(std::string window_class, std::string window_title)
    : class_(std::move(window_class)), title_(std::move(window_title)) {}

bool SystemGameWindow::OpenUrl(const std::string&, std::string& error) {
    error = "starting the Steam client is only available on Windows";
    return false;
}

long SystemGameWindow::FindGame() { return 0; }

bool SystemGameWindow::RequestClose(std::string&) { return true; }

}  // namespace maba

#endif  // _WIN32
