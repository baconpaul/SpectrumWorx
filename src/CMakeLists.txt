# sw-impl: the plugin, as a static library exporting clap_init / clap_deinit /
# clap_get_factory. clap-wrapper links it behind a per-format entry stub, so
# this is where essentially all development happens.
#
# The 2016 target definitions are in legacy-build.cmake, which nothing includes.
#
# SPDX-License-Identifier: GPL-3.0-or-later

# leConfigurationAndODRHeader.h is force-included into every C++ translation
# unit, as the 2016 build did: it defines LE_IMPL_NAMESPACE_BEGIN,
# LE_CHECKED_BUILD and the SSE feature macros, and the headers assume all of
# them. Missing it does not fail cleanly -- LE_IMPL_NAMESPACE_BEGIN( Math ) then
# parses as a variable declaration, and le/math alone produces thirty-odd errors
# that name everything except the cause.
#
# MSVC spells it /FI with the path attached; everything else takes -include and
# the path as two arguments. Given only the latter, MSVC took the header for a
# source file and reported that it could not open it.
#
# Absolute, because the two consumers below are a target-wide option and a
# per-source one, and a relative path is resolved against the include search
# path rather than against this directory.
if (MSVC)
    set(SW_FORCE_INCLUDE_ODR_HEADER
            "/FI${CMAKE_CURRENT_SOURCE_DIR}/le/build/leConfigurationAndODRHeader.h")
else ()
    set(SW_FORCE_INCLUDE_ODR_HEADER
            "-include" "${CMAKE_CURRENT_SOURCE_DIR}/le/build/leConfigurationAndODRHeader.h")
endif ()

# gui.cmake first: sw-dsp links sw-gui-resources for the skin its module class
# now reaches, and sw-gui-widgets links sw-dsp back. CMake resolves target names
# at generate time either way, but reading it in dependency order is kinder.
include(${CMAKE_CURRENT_SOURCE_DIR}/gui.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/dsp.cmake)

add_library(sw-impl STATIC
        spectrumWorxCLAP.cpp
        swClapEntryImpl.cpp
        clapHelpersInstantiation.cpp

        # The host-facing half of the core. Deliberately not in sw-dsp: these
        # are what a plugin format talks to, and the golden tests must not need
        # them. plugin2Host.cpp holds the real parameter enumeration -- the
        # statics over Program const* that clap_plugin_params is made of.
        core/host_interop/host2Plugin.cpp
        core/host_interop/plugin2Host.cpp
)

target_include_directories(sw-impl PUBLIC .)

# PUBLIC: the per-format entry stubs compile against sw-impl's interface only,
# and they include <clap/clap.h>.
target_link_libraries(sw-impl PUBLIC clap)

# The engine, by way of the module layer that its factory's vtables point at.
# PUBLIC because spectrumWorxCLAP.hpp derives from SpectrumWorxCore and holds a
# Program by value, and because sw-dsp carries the force-include every one of
# its headers assumes.
target_link_libraries(sw-impl PUBLIC sw-gui)

target_link_libraries(sw-impl PRIVATE
        clap-helpers
        clap-wrapper-extensions
        fmt-header-only
        sst-plugininfra
        sst-plugininfra::version_information
        sst::clap_juce_shim
        sst::clap_juce_shim_headers
        juce::juce_gui_basics
)

# clap_juce_shim.h is included from spectrumWorxCLAP.hpp, and the ADD_SHIM_*
# macros need SHIM_MAC/SHIM_LINUX/SHIM_WINDOWS from this interface target.
target_link_libraries(sw-impl PUBLIC sst::clap_juce_shim_headers)

target_compile_definitions(sw-impl PRIVATE
        PRODUCT_NAME="${PRODUCT_NAME}"
        SW_CLAP_ID="${SW_CLAP_ID}"
        JUCE_STANDALONE_APPLICATION=0
)
