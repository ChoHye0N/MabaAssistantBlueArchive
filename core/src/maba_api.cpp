// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#include "maba/maba_api.h"

#ifndef MABA_VERSION_STRING
#define MABA_VERSION_STRING "0.0.0"
#endif

extern "C" {

MABA_API const char* MabaGetVersion(void) { return MABA_VERSION_STRING; }

MABA_API int MabaApiVersion(void) { return 1; }

}  // extern "C"
