# SpectrumWorx — top level build
#
# Copyright (c) 2010 - 2016. Little Endian Ltd.
# Copyright (c) 2026 the SpectrumWorx contributors.
# SPDX-License-Identifier: GPL-3.0-or-later

# 3.28 because clap-wrapper sets policy CMP0149, which is unknown before 3.27.
cmake_minimum_required(VERSION 3.28)

cmake_policy(SET CMP0091 NEW) # MSVC runtime via CMAKE_MSVC_RUNTIME_LIBRARY
cmake_policy(SET CMP0063 NEW) # visibility presets honoured for all target types

# Must precede project() — project() runs the compiler checks that consume them.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.15 CACHE STRING "Minimum macOS version")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(SpectrumWorx VERSION 3.0.0 LANGUAGES C CXX)

set(PRODUCT_NAME "SpectrumWorx")
set(SW_CLAP_ID "com.littleendian.spectrumworx")

# The JUCE shim compiles .mm. Enable ObjC here, not in a subdirectory — CMake
# does not populate CMAKE_OBJCXX_COMPILE_OBJECT for subdirectory-scoped
# enable_language() and generation fails.
if (APPLE)
    enable_language(OBJC)
    enable_language(OBJCXX)
    set(CMAKE_OBJC_VISIBILITY_PRESET hidden)
    set(CMAKE_OBJCXX_VISIBILITY_PRESET hidden)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Never shared unless asked")

# CI always passes one; a bare `cmake -B build` otherwise gets no optimisation
# flags at all on single-config generators.
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

option(SW_COPY_AFTER_BUILD "Install the built plugins into the user plugin folders" OFF)
option(SW_BUILD_TESTS "Build sw-tests" ON)

if (SW_BUILD_TESTS)
    enable_testing()
endif()

# Before add_subdirectory(libs): sst-plugininfra configure_file()s its version
# source from these variables, silently emitting empty strings if they are unset.
include(${CMAKE_SOURCE_DIR}/libs/sst/sst-plugininfra/cmake/git-version-functions.cmake)
version_from_versionfile_or_git()
message(STATUS "SpectrumWorx version: ${GIT_IMPLIED_DISPLAY_VERSION} (${GIT_BRANCH}@${GIT_COMMIT_HASH})")

set(JUCE_PATH "${CMAKE_SOURCE_DIR}/libs/JUCE")

add_subdirectory(libs)
include(temporary-boost) # delete with stage 7 — see the file
add_subdirectory(assets)
add_subdirectory(src)
add_subdirectory(src/clap-first)

if (SW_BUILD_TESTS)
    include(Catch)
    add_subdirectory(tests)
endif()
