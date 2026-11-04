// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Win32 access to the Steam game window: find it, open the steam:// URL, ask it to close.
#ifdef _WIN32

#include <windows.h>

#include <objbase.h>
#include <shellapi.h>

#include <utility>

#include "maba/steam_launch.h"

namespace maba {

namespace {

std::wstring Widen(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

HWND FindVisible(const std::string& cls, const std::string& title) {
    const std::wstring c = Widen(cls), t = Widen(title);
    HWND hwnd = FindWindowW(c.empty() ? nullptr : c.c_str(), t.empty() ? nullptr : t.c_str());
    return hwnd && IsWindowVisible(hwnd) ? hwnd : nullptr;
}

}  // namespace

SystemGameWindow::SystemGameWindow(std::string window_class, std::string window_title)
    : class_(std::move(window_class)), title_(std::move(window_title)) {}

bool SystemGameWindow::OpenUrl(const std::string& url, std::string& error) {
    // Shell handlers may use COM; the calling thread is a plain worker.
    const HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const std::wstring w = Widen(url);
    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
    info.lpVerb = L"open";
    info.lpFile = w.c_str();
    info.nShow = SW_SHOWNORMAL;
    const bool ok = ShellExecuteExW(&info) != FALSE;
    if (!ok) error = "ShellExecuteEx failed (Win32 error " + std::to_string(GetLastError()) + "; is Steam installed?)";
    if (SUCCEEDED(co)) CoUninitialize();
    return ok;
}

long SystemGameWindow::FindGame() {
    HWND hwnd = FindVisible(class_, title_);
    if (!hwnd) return 0;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    return static_cast<long>(pid);
}

bool SystemGameWindow::RestoreGame() {
    HWND hwnd = FindVisible(class_, title_);
    if (!hwnd || !IsIconic(hwnd)) return false;
    ShowWindow(hwnd, SW_RESTORE);
    return true;
}

bool SystemGameWindow::RequestClose(std::string& error) {
    HWND hwnd = FindVisible(class_, title_);
    if (!hwnd) return true;
    if (!PostMessageW(hwnd, WM_CLOSE, 0, 0)) {
        error = "PostMessage failed (Win32 error " + std::to_string(GetLastError()) + ")";
        return false;
    }
    return true;
}

}  // namespace maba

#endif  // _WIN32
