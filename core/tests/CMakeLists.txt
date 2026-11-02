# SPDX-License-Identifier: AGPL-3.0-only
# SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
add_executable(maba_core_tests core_tests.cpp)
target_link_libraries(maba_core_tests PRIVATE maba_core)
add_test(NAME maba_core_tests COMMAND maba_core_tests)
