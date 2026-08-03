# sw-impl: the plugin, as a static library exporting clap_init / clap_deinit /
# clap_get_factory. clap-wrapper links it behind a per-format entry stub, so
# this is where essentially all development happens.
#
# The 2016 target definitions are in legacy-build.cmake, which nothing includes.
#
# SPDX-License-Identifier: GPL-3.0-or-later

# gui.cmake first: sw-gui-widgets links sw-io, which dsp.cmake defines. CMake
# resolves target names at generate time either way, but reading it in dependency
# order is kinder.
#
# \note This used to read "sw-dsp links sw-gui-resources for the skin its module
# class now reaches". It does not: a module's strip belongs to the editor now and
# sw-dsp links no JUCE at all -- see the note over sw::assets in dsp.cmake, and
# checkNoJuceInDSP.cmake, which fails the build if it comes back.
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

sw_force_include_odr_header(sw-impl)

target_include_directories(sw-impl PUBLIC .)

# PUBLIC: the per-format entry stubs compile against sw-impl's interface only,
# and they include <clap/clap.h>.
target_link_libraries(sw-impl PUBLIC clap)

# The engine, by way of the module layer that its factory's vtables point at.
# PUBLIC because spectrumWorxCLAP.hpp derives from SpectrumWorxCore and holds a
# Program by value.
#
# \note It used to carry the force-include across this link line as well, which
# is how clap-wrapper's sources came to be compiled with a header of ours. They
# are not, now: sw_force_include_odr_header() above names sw-impl's own sources.
# The entry stubs in clap-first/ include swClapEntryImpl.hpp, which is written to
# need nothing from it.
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
        SW_VENDOR="${SW_VENDOR}"
        SW_VENDOR_URL="${SW_VENDOR_URL}"
        JUCE_STANDALONE_APPLICATION=0
)
