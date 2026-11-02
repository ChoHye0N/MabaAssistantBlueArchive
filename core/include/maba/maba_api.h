// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// C ABI exposed to the C# GUI (P/Invoke, cdecl). Keep it small: functions + JSON strings.
#pragma once

#if defined(_WIN32)
#if defined(MABA_BUILDING_DLL)
#define MABA_API __declspec(dllexport)
#else
#define MABA_API __declspec(dllimport)
#endif
#else
#define MABA_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Library version string, e.g. "0.1.0". The pointer is static and must not be freed.
MABA_API const char* MabaGetVersion(void);

// Bumped on every breaking change to this C ABI.
MABA_API int MabaApiVersion(void);

#ifdef __cplusplus
}
#endif
