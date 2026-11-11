# SPDX-License-Identifier: AGPL-3.0-only
# SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
cmake_minimum_required(VERSION 3.21)
project(MabaAssistantBlueArchive VERSION 0.1.4 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MABA_BUILD_TESTS "Build unit tests" ON)
option(MABA_WITH_OPENCV "Enable the OpenCV-based Vision module (needs vcpkg on Windows)" OFF)

add_subdirectory(core)

if(MABA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(core/tests)
endif()
