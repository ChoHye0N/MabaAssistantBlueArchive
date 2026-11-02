# SPDX-License-Identifier: AGPL-3.0-only
# SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
add_executable(maba_core_tests
    test_main.cpp
    core_tests.cpp
    geometry_tests.cpp
    page_tests.cpp
    resolver_tests.cpp
    wait_tests.cpp)
target_link_libraries(maba_core_tests PRIVATE maba_core)
if(MABA_WITH_OPENCV)
    target_sources(maba_core_tests PRIVATE vision_tests.cpp)
endif()
if(MSVC)
    target_compile_options(maba_core_tests PRIVATE /utf-8 /W4 /permissive-)
else()
    target_compile_options(maba_core_tests PRIVATE -Wall -Wextra)
endif()
add_test(NAME maba_core_tests COMMAND maba_core_tests)
