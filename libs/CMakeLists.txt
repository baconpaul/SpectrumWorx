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

################################################################################
# sw-juce -- the one target that compiles JUCE.
#
#   A JUCE module target is an INTERFACE library carrying its own .cpp files as
# INTERFACE_SOURCES, so linking one does not link a library: it hands those
# sources to the *consumer*, which compiles them itself. Every target that named
# a module compiled JUCE, and a PUBLIC link passed the sources on to compile
# again -- 72 JUCE translation units in a build that needs ten of them.
#
#   This target compiles them once and gives everyone else the same modules'
# usage requirements with the sources left behind. The link below is PRIVATE,
# which CMake records as $<LINK_ONLY:juce::...>: it stays on a consumer's link
# line -- so the macOS frameworks, Linux's -latomic and the pkg-config packages
# still arrive -- while the sources, include paths and defines that would come
# with it do not. The three a consumer does need are copied across explicitly.
#
#   Per module rather than just the two at the root of the dependency graph:
# $<TARGET_PROPERTY:tgt,INTERFACE_X> reads that one target's property and does
# not walk its dependencies, so juce_gui_extra alone would forward none of
# juce_core's -- which is where JUCE_USE_CURL, JUCE_MODULE_AVAILABLE_* and the
# DEBUG/NDEBUG pair live. Copying them keeps one source of truth for the
# settings above: a consumer's headers and this target's objects cannot disagree
# about them, which is the ODR bug in the note above waiting to happen again.
#
# \note One target keeps compiling its own copy, and it is not an oversight:
# clap_juce_shim, which is upstream in sst-clap-helpers, and which compiles its
# nine modules with four settings of its own -- JUCE_MODAL_LOOPS_PERMITTED,
# JUCE_COREGRAPHICS_DRAW_ASYNC, JUCE_REPORT_APP_USAGE and JUCE_USE_CAMERA. Those
# it has, and this target does not, are what remains of the divergence the note
# above describes. It changes nothing about what ships: the bundle resolves all
# nine from libclap_juce_shim.a, exactly as it did when the alternative was
# sw-impl's copy (which took the four from the shim's PUBLIC link), and takes
# only juce_audio_basics and juce_audio_formats from here -- two modules the
# shim does not build and that read none of the four. `nm -pa <bundle> | grep
# OSO` is what answers this, per build.
#
# \note sw-show-ui was the second such target, and the reason it was one did not
# survive being looked at. It defined _CONSOLE so that its own copy would
# compile JUCEApplicationBase::main( argc, argv ) rather than the WinMain arm --
# but it also links sw-gui, which brings this target along PUBLIC, so both
# copies were on its link line and every symbol in the shared modules was
# defined twice. Three linkers never said so; lld-link did, on the first
# clang-cl CI run. The tool takes JUCE from here now and calls the no-argument
# overload, which is compiled outside that guard. See tools/show-ui.
#
#   JUCE_STANDALONE_APPLICATION is *not* what separated those two from this
# target, whatever the settings on sw-impl and sw-show-ui suggest: no module in
# JUCE 8.0.12 reads it. It is left where it is rather than deleted here.
#
# \note What this widens: a consumer sees all eight modules' headers rather than
# the closure of the one it used to name. The boundary that matters is sw-dsp,
# which links no JUCE at all and does not link this either --
# tests/checkNoJuceInDSP.cmake fails the build if that changes.
################################################################################

set(swJuceModules
        juce_core juce_events juce_data_structures juce_graphics
        juce_gui_basics juce_gui_extra juce_audio_basics juce_audio_formats)

# One translation unit of its own, so the target has a source of a language
# CMake can name. JUCE's arrive through the link below. file(CONFIGURE) rather
# than file(WRITE) because it leaves the file alone when the content has not
# changed, and rewriting it would recompile JUCE on every configure.
set(swJuceUnit "${CMAKE_CURRENT_BINARY_DIR}/swJuceUnit.cpp")
file(CONFIGURE OUTPUT "${swJuceUnit}"
     CONTENT "// JUCE is compiled here. See sw-juce in libs/CMakeLists.txt.\n")

add_library(sw-juce STATIC "${swJuceUnit}")

foreach (swJuceModule IN LISTS swJuceModules)
    target_link_libraries(sw-juce PRIVATE juce::${swJuceModule})

    target_include_directories(sw-juce INTERFACE
            $<TARGET_PROPERTY:juce::${swJuceModule},INTERFACE_INCLUDE_DIRECTORIES>)
    target_compile_definitions(sw-juce INTERFACE
            $<TARGET_PROPERTY:juce::${swJuceModule},INTERFACE_COMPILE_DEFINITIONS>)
    target_compile_options(sw-juce INTERFACE
            $<TARGET_PROPERTY:juce::${swJuceModule},INTERFACE_COMPILE_OPTIONS>)

    # A module's Linux pkg-config packages are a target of their own, hung off
    # the module's link interface -- and its include paths and cflags are on the
    # compile side of the line the PRIVATE link draws, so they need the same
    # treatment one hop further out. Only juce_graphics has any (freetype2 and
    # fontconfig), and only on Linux, which is why the existence test: the
    # target is created by juce_add_module() or not at all. The libraries
    # themselves come through the link interface and need nothing here.
    #
    # Untested from this machine -- macOS creates none of these -- so if a Linux
    # build fails on a missing freetype header, this is the line to read first.
    if (TARGET juce::pkgconfig_${swJuceModule}_LINUX_DEPS)
        target_include_directories(sw-juce INTERFACE
                $<TARGET_PROPERTY:juce::pkgconfig_${swJuceModule}_LINUX_DEPS,INTERFACE_INCLUDE_DIRECTORIES>)
        target_compile_options(sw-juce INTERFACE
                $<TARGET_PROPERTY:juce::pkgconfig_${swJuceModule}_LINUX_DEPS,INTERFACE_COMPILE_OPTIONS>)
    endif ()
endforeach ()

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
