# sw-assets — everything the plugin ships, compiled into the binary.
#
# In 2016 these were installed to disk and found through a `SpectrumWorx.paths`
# file that the installer wrote and the plugin mmapped at startup: no installer,
# no GUI and no factory presets. Embedding them removes that dependency
# entirely, and with it GUI::initializePaths(), mapPathsFile(), rootPath() and
# resourcesPath().
#
# CMakeRC rather than juce_add_binary_data, because add_clap_juce_shim() sets
# JUCE_MODULES_ONLY and none of the juce_add_* helpers exist under it.
#
# \note One library rooted at assets/, not one per kind: what the consumers want
# is a path ("skin/01.png", "presets/Echoes/LE Boiler.swp"), and the skin and the
# presets are wanted by the same binary in every configuration that has either.
#
# SPDX-License-Identifier: GPL-3.0-or-later

include(CmakeRC)

# CONFIGURE_DEPENDS so that adding a skin bitmap or a factory preset does not
# also mean editing this file. The sets are small and change rarely; the
# reconfigure cost is noise.
file(GLOB SW_SKIN_FILES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/skin/*.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/skin/*.ttf"
)

file(GLOB_RECURSE SW_PRESET_FILES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/presets/*.swp"
)

list(LENGTH SW_SKIN_FILES SW_SKIN_FILE_COUNT)
if (SW_SKIN_FILE_COUNT EQUAL 0)
    message(FATAL_ERROR "assets/skin is empty -- did the submodule/checkout complete?")
endif()

list(LENGTH SW_PRESET_FILES SW_PRESET_FILE_COUNT)
if (SW_PRESET_FILE_COUNT EQUAL 0)
    message(FATAL_ERROR "assets/presets is empty -- did the submodule/checkout complete?")
endif()

message(STATUS "SpectrumWorx assets: ${SW_SKIN_FILE_COUNT} skin files, "
        "${SW_PRESET_FILE_COUNT} factory presets")

cmrc_add_resource_library(sw-assets
        ALIAS sw::assets
        NAMESPACE swAssets
        WHENCE "${CMAKE_CURRENT_SOURCE_DIR}"
        ${SW_SKIN_FILES}
        ${SW_PRESET_FILES}
)

# cmrc_add_resource_library makes a static library of generated .cpp files; they
# are not ours and have nothing to say about our warnings.
set_target_properties(sw-assets PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        UNITY_BUILD FALSE
        FOLDER assets
)
