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

# JUCE's module settings, on the module rather than on one of its consumers.
#
# **Linking a JUCE module compiles that module's own sources into the linking
# target.** juce_core.cpp is built once inside sw-io, again inside
# sw-gui-resources and again inside sw-show-ui, so a setting put on any one of
# them is a setting the other two do not have -- and one of those builds JUCE
# with different settings than the rest, which is an ODR violation waiting for
# something to notice it. These were on sw-io, PUBLIC, and sw-gui-resources does
# not link sw-io: it takes juce::juce_gui_basics directly, built juce_core.cpp
# with JUCE's defaults, and reached for <curl/curl.h>.
#
# Only on Linux, which is why it took until there was a Linux build. JUCE's URL
# backend is NSURLSession on macOS and WinINet on Windows; on Linux it is
# libcurl, wanted by a plugin that does no networking at all.
#
# juce_core because every other module depends on it, and INTERFACE because the
# consumer is what does the compiling.
#
# \note Only these. add_clap_juce_shim() also sets JUCE_MODAL_LOOPS_PERMITTED,
# JUCE_USE_CAMERA and JUCE_REPORT_APP_USAGE on its own targets; those change what
# compiles in gui/, where the preset browser's two async save-path callers are
# exactly the code that would notice.
target_compile_definitions(juce_core INTERFACE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)

# The factory samples are MP3, and off Apple and Windows nothing else decodes
# one: registerBasicFormats() gets CoreAudioFormat on macOS and
# WindowsMediaAudioFormat on Windows, and on Linux it would get neither. JUCE's
# own decoder is behind this flag because it carries a patent disclaimer -- the
# MP3 patents expired in 2017, and the licence JUCE's decoder ships under is the
# one this whole tree is already built with.
target_compile_definitions(juce_audio_formats INTERFACE JUCE_USE_MP3AUDIOFORMAT=1)

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
