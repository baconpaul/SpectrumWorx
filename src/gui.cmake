# The editor, in layers, so that a layer can be built and looked at before the
# one above it compiles. Stage 6 has ~10 k lines of JUCE 2.1.2 to port and the
# harness (tools/show-ui) shows whatever is ready.
#
#   sw-gui-resources    skin bitmaps and fonts        <- builds
#   sw-gui-widgets      gui.{hpp,cpp}, the widget set <- builds
#   sw-gui              editor, modules, preset browser
#
# SPDX-License-Identifier: GPL-3.0-or-later

set(SW_GUI_RESOURCE_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/gui/resources.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/gui/theme.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/le/utility/assertionHandler.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/le/utility/trace.cpp # assertionHandler routes through it
)

add_library(sw-gui-resources STATIC ${SW_GUI_RESOURCE_SOURCES})

target_include_directories(sw-gui-resources PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# juce_gui_basics rather than juce_graphics: Theme is a LookAndFeel_V2. The
# other four modules come transitively.
target_link_libraries(sw-gui-resources
        PUBLIC juce::juce_gui_basics
        PRIVATE sw::skin
)

target_compile_definitions(sw-gui-resources PRIVATE LE_ENABLE_ASSERT_HANDLER)

# Per source file, not per target: linking a JUCE module target adds that
# module's own .c/.cpp/.mm to *this* target, and leConfigurationAndODRHeader.h
# is C++ (it includes <cstddef>), so a target-wide -include breaks JUCE's C.
set_source_files_properties(${SW_GUI_RESOURCE_SOURCES}
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        PROPERTIES COMPILE_OPTIONS
        "-include;${CMAKE_CURRENT_SOURCE_DIR}/le/build/leConfigurationAndODRHeader.h")

################################################################################
# sw-gui-widgets -- the widget set: knobs, buttons, combo boxes, menus.
#
# Built at LE_SW_GUI=1, which is the configuration the whole port is moving to.
# It does not link yet: gui.cpp still calls into SpectrumWorxEditor, whose
# translation unit is bound to the deleted 2016 VST2 plugin class. Compiling it
# is the milestone -- the two JUCE 8 rewrites (asynchronous menus and dialogs,
# and a PopupMenu that owns its items instead of reaching into JUCE's private
# state) both live here and both are done.
################################################################################

add_library(sw-gui-widgets STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/gui/gui.cpp
)

target_link_libraries(sw-gui-widgets PUBLIC sw-gui-resources sw-dsp)

# The configuration this target exists to prove.
target_compile_definitions(sw-gui-widgets PUBLIC LE_SW_GUI=1)

if (APPLE)
    target_sources(sw-gui-widgets PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/gui/gui.mm)
    target_link_libraries(sw-gui-widgets PRIVATE "-framework Carbon" "-framework Cocoa")
endif()
