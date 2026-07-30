# Third-party dependencies. Every add_subdirectory of a submodule lives here.
#
# Order is load-bearing:
#   clap -> clap-helpers        so clap-wrapper's guarantee_clap() short-circuits
#                               instead of CPM-fetching a second copy of clap
#   sst-clap-helpers            defines add_clap_juce_shim()
#   sst-cpputils                sst-basic-blocks probes for the target by name
#   fmt                         clap-wrapper reuses fmt-header-only if it exists
#   clap-wrapper last           it is the heaviest and depends on the rest
#
# SPDX-License-Identifier: GPL-3.0-or-later

add_subdirectory(sst/sst-cmake)
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" PARENT_SCOPE)

# simde ships no target of its own; sst-basic-blocks looks for one by name.
add_library(simde INTERFACE)
target_include_directories(simde INTERFACE simde)

# TEMPORARY — the preset parser until stage 8 swaps it for tinyxml2. Header
# only, vendored; see rapidxml/README.md.
add_library(rapidxml INTERFACE)
target_include_directories(rapidxml INTERFACE rapidxml)

add_subdirectory(fmt EXCLUDE_FROM_ALL)

# The FFT off Apple, replacing nt2/signal/static_fft.hpp. Apple keeps
# Accelerate/vDSP, so the target is only needed on the other platforms -- but it
# is configured everywhere so that a macOS build still notices if it breaks.
add_subdirectory(pffft EXCLUDE_FROM_ALL)

if (SW_BUILD_TESTS)
    add_subdirectory(catch2 EXCLUDE_FROM_ALL)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/catch2/extras")
    set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" PARENT_SCOPE)
endif()

add_subdirectory(clap/clap EXCLUDE_FROM_ALL)
add_subdirectory(clap/clap-helpers EXCLUDE_FROM_ALL)

add_subdirectory(sst/sst-clap-helpers)
add_clap_juce_shim(JUCE_PATH "${JUCE_PATH}")

add_subdirectory(sst/sst-cpputils)
add_subdirectory(sst/sst-basic-blocks)
add_subdirectory(sst/sst-plugininfra)

set(CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES TRUE CACHE BOOL "Fetch the VST3/AU SDKs")
set(CLAP_WRAPPER_BUILD_AUV2 TRUE CACHE BOOL "Build the AUv2 wrapper")
add_compile_definitions(CLAP_WRAPPER_LOGLEVEL=0)
if (APPLE)
    set(RTAUDIO_API_JACK FALSE CACHE BOOL "No jack on macOS")
endif()
if (WIN32)
    set(RTAUDIO_API_ASIO ON CACHE BOOL "ASIO for the Windows standalone" FORCE)
endif()
add_subdirectory(clap/clap-wrapper EXCLUDE_FROM_ALL)
