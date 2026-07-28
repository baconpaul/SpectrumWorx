# sw-impl: the plugin, as a static library exporting clap_init / clap_deinit /
# clap_get_factory. clap-wrapper links it behind a per-format entry stub, so
# this is where essentially all development happens.
#
# The 2016 target definitions are in legacy-build.cmake, which nothing includes.
#
# SPDX-License-Identifier: GPL-3.0-or-later

include(${CMAKE_CURRENT_SOURCE_DIR}/dsp.cmake)

add_library(sw-impl STATIC
        spectrumWorxCLAP.cpp
        stubParameters.cpp
        stubEditor.cpp
        swClapEntryImpl.cpp
        clapHelpersInstantiation.cpp
)

target_include_directories(sw-impl PUBLIC .)

# PUBLIC: the per-format entry stubs compile against sw-impl's interface only,
# and they include <clap/clap.h>.
target_link_libraries(sw-impl PUBLIC clap)

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
