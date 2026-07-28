# sw-impl: the plugin, as a static library exporting clap_init / clap_deinit /
# clap_get_factory. clap-wrapper links it behind a per-format entry stub, so
# this is where essentially all development happens.
#
# The 2016 target definitions are in legacy-build.cmake, which nothing includes.
#
# SPDX-License-Identifier: GPL-3.0-or-later

include(${CMAKE_CURRENT_SOURCE_DIR}/dsp.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/gui.cmake)

add_library(sw-impl STATIC
        spectrumWorxCLAP.cpp
        stubParameters.cpp
        stubEditor.cpp
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

# The engine. PUBLIC because spectrumWorxCLAP.hpp derives from SpectrumWorxCore
# and holds a Program by value, and because sw-dsp carries the force-include and
# LE_SW_GUI=0 that every one of its headers assumes.
#
# \note LE_SW_GUI=0 is what makes SpectrumWorxCore's Module typedef ModuleDSP
# rather than ModuleDSPAndGUI. Hosting the real editor means building the core
# with LE_SW_GUI=1, which is a second configuration of sw-dsp, not a flag on
# this target -- see the note in doc/tech/implementation_sequence.md.
target_link_libraries(sw-impl PUBLIC sw-dsp)

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
