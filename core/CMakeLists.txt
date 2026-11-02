# SPDX-License-Identifier: AGPL-3.0-only
# SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors

# Portable logic (no OS or OpenCV dependency by default), unit-testable on any platform.
add_library(maba_core STATIC
    src/coords.cpp
    src/geometry.cpp
    src/page.cpp
    src/task_order.cpp
    src/template_resolver.cpp
    src/wait.cpp)

target_include_directories(maba_core PUBLIC include)
target_compile_features(maba_core PUBLIC cxx_std_17)
set_target_properties(maba_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    target_compile_options(maba_core PRIVATE /utf-8 /W4 /permissive-)
else()
    target_compile_options(maba_core PRIVATE -Wall -Wextra)
endif()

# JSON for page/task data. Header-only; vcpkg on Windows, nlohmann-json3-dev on Linux.
find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(maba_core PUBLIC nlohmann_json::nlohmann_json)

if(MABA_WITH_OPENCV)
    find_package(OpenCV CONFIG REQUIRED)
    target_sources(maba_core PRIVATE src/vision.cpp)
    target_link_libraries(maba_core PUBLIC ${OpenCV_LIBS})
    target_include_directories(maba_core PUBLIC ${OpenCV_INCLUDE_DIRS})
    target_compile_definitions(maba_core PUBLIC MABA_WITH_OPENCV=1)
endif()

# C ABI shared library consumed by the C# GUI via P/Invoke.
add_library(MabaCore SHARED src/maba_api.cpp)
target_link_libraries(MabaCore PRIVATE maba_core)
target_compile_definitions(MabaCore PRIVATE
    MABA_BUILDING_DLL
    MABA_VERSION_STRING="${PROJECT_VERSION}")
set_target_properties(MabaCore PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)

# Windows-only platform code: Steam PC controller (GDI capture, SendInput) and the CLI driver.
# Needs OpenCV for frame scaling.
if(WIN32 AND MABA_WITH_OPENCV)
    add_library(maba_platform_windows STATIC
        src/platform/windows/gdi_capture.cpp
        src/platform/windows/send_input.cpp
        src/platform/windows/steam_controller.cpp)
    target_include_directories(maba_platform_windows PUBLIC src/platform/windows)
    target_link_libraries(maba_platform_windows PUBLIC maba_core)
    target_compile_definitions(maba_platform_windows PUBLIC
        UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(maba_platform_windows PRIVATE /utf-8 /W4 /permissive-)

    add_executable(maba_cli apps/maba_cli.cpp)
    target_link_libraries(maba_cli PRIVATE maba_platform_windows)
    target_compile_options(maba_cli PRIVATE /utf-8 /W4 /permissive-)
endif()
