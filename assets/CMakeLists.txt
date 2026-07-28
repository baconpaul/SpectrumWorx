# sw-skin — the editor's bitmaps and fonts, compiled into the binary.
#
# In 2016 these were installed to disk and found through a `SpectrumWorx.paths`
# file that the installer wrote and the plugin mmapped at startup: no installer,
# no GUI. Embedding them removes that dependency entirely, and with it
# GUI::initializePaths(), mapPathsFile(), rootPath() and resourcesPath().
#
# CMakeRC rather than juce_add_binary_data, because add_clap_juce_shim() sets
# JUCE_MODULES_ONLY and none of the juce_add_* helpers exist under it.
#
# SPDX-License-Identifier: GPL-3.0-or-later

include(CmakeRC)

# CONFIGURE_DEPENDS so that adding a skin bitmap does not also mean editing this
# file. The set is small and changes rarely; the reconfigure cost is noise.
file(GLOB SW_SKIN_FILES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/skin/*.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/skin/*.ttf"
)

list(LENGTH SW_SKIN_FILES SW_SKIN_FILE_COUNT)
if (SW_SKIN_FILE_COUNT EQUAL 0)
    message(FATAL_ERROR "assets/skin is empty -- did the submodule/checkout complete?")
endif()
message(STATUS "SpectrumWorx skin: ${SW_SKIN_FILE_COUNT} embedded files")

cmrc_add_resource_library(sw-skin
        ALIAS sw::skin
        NAMESPACE swSkin
        WHENCE "${CMAKE_CURRENT_SOURCE_DIR}"
        ${SW_SKIN_FILES}
)

# cmrc_add_resource_library makes a static library of generated .cpp files; they
# are not ours and have nothing to say about our warnings.
set_target_properties(sw-skin PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        UNITY_BUILD FALSE
        FOLDER assets
)
