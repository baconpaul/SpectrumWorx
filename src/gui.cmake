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
# is C++ (it includes <cstddef>), so a target-wide force-include breaks JUCE's C.
set_source_files_properties(${SW_GUI_RESOURCE_SOURCES}
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        PROPERTIES COMPILE_OPTIONS "${SW_FORCE_INCLUDE_ODR_HEADER}")

################################################################################
# sw-gui-widgets -- the widget set: knobs, buttons, combo boxes, menus.
#
# The two JUCE 8 rewrites live here and are done: asynchronous menus and
# dialogs, and a PopupMenu that owns its items instead of reaching into JUCE's
# private state.
################################################################################

add_library(sw-gui-widgets STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/gui/gui.cpp
)

target_link_libraries(sw-gui-widgets PUBLIC sw-gui-resources sw-dsp)

if (APPLE)
    target_sources(sw-gui-widgets PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/gui/gui.mm)
    target_link_libraries(sw-gui-widgets PRIVATE "-framework Carbon" "-framework Cocoa")
endif()

################################################################################
# sw-gui -- the module layer: a module's UI and its parameter controls.
#
# core/modules/moduleDSPAndGUI.cpp lives here rather than in sw-dsp: it is
# SW::Module's out-of-line half, and every one of its virtuals exists to push a
# value into a widget. sw-dsp's factory instantiates the class and emits the
# vtables; this target supplies what they point at.
################################################################################

add_library(sw-gui STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/core/modules/moduleDSPAndGUI.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/gui/modules/moduleControl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/gui/modules/moduleUI.cpp

        ${CMAKE_CURRENT_SOURCE_DIR}/gui/editor/auxiliaryComponents.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/gui/editor/moduleMenuHolder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/gui/editor/spectrumWorxEditor.cpp
)

target_link_libraries(sw-gui PUBLIC sw-gui-widgets)
