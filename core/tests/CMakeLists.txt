# SPDX-License-Identifier: AGPL-3.0-only
# SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
add_executable(maba_core_tests
    test_main.cpp
    core_tests.cpp
    geometry_tests.cpp
    page_tests.cpp
    resolver_tests.cpp
    smoke_tests.cpp
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

# Real-screenshot page detection; skipped unless assets-local/fixtures/ holds screenshots.
if(MABA_WITH_OPENCV)
    add_executable(maba_fixture_tests fixture_tests.cpp)
    target_link_libraries(maba_fixture_tests PRIVATE maba_core)
    target_compile_definitions(maba_fixture_tests PRIVATE MABA_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME maba_fixture_tests COMMAND maba_fixture_tests)
    set_tests_properties(maba_fixture_tests PROPERTIES SKIP_RETURN_CODE 77)
endif()
