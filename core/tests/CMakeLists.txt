# SPDX-License-Identifier: AGPL-3.0-only
# SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
add_executable(maba_core_tests
    test_main.cpp
    adb_tests.cpp
    core_tests.cpp
    flow_tests.cpp
    geometry_tests.cpp
    navigator_tests.cpp
    ocr_sidecar_tests.cpp
    page_tests.cpp
    resolver_tests.cpp
    smoke_tests.cpp
    task_tests.cpp
    util_tests.cpp
    wait_tests.cpp)
target_link_libraries(maba_core_tests PRIVATE maba_core)
target_compile_definitions(maba_core_tests PRIVATE MABA_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
if(MABA_WITH_OPENCV)
    target_sources(maba_core_tests PRIVATE vision_tests.cpp)
endif()
if(MSVC)
    target_compile_options(maba_core_tests PRIVATE /utf-8 /W4 /permissive-)
else()
    target_compile_options(maba_core_tests PRIVATE -Wall -Wextra)
endif()
add_test(NAME maba_core_tests COMMAND maba_core_tests)

# The C ABI through the shared library, as the GUI loads it.
find_package(nlohmann_json CONFIG REQUIRED)
add_executable(maba_api_tests test_main.cpp api_tests.cpp)
target_link_libraries(maba_api_tests PRIVATE MabaCore nlohmann_json::nlohmann_json)
target_include_directories(maba_api_tests PRIVATE ${PROJECT_SOURCE_DIR}/core/include)
target_compile_definitions(maba_api_tests PRIVATE MABA_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
if(MSVC)
    target_compile_options(maba_api_tests PRIVATE /utf-8 /W4 /permissive-)
endif()
if(WIN32)
    # The test needs MabaCore.dll and its own dependencies (OpenCV) next to it.
    add_custom_command(TARGET maba_api_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:MabaCore> $<TARGET_RUNTIME_DLLS:MabaCore> $<TARGET_FILE_DIR:maba_api_tests>
        COMMAND_EXPAND_LISTS)
endif()
add_test(NAME maba_api_tests COMMAND maba_api_tests)

# Real-screenshot page detection; skipped unless assets-local/fixtures/ holds screenshots.
if(MABA_WITH_OPENCV)
    add_executable(maba_fixture_tests fixture_tests.cpp)
    target_link_libraries(maba_fixture_tests PRIVATE maba_core)
    target_compile_definitions(maba_fixture_tests PRIVATE MABA_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME maba_fixture_tests COMMAND maba_fixture_tests)
    set_tests_properties(maba_fixture_tests PROPERTIES SKIP_RETURN_CODE 77)
endif()
