# The editor, in layers, so that a layer can be built and looked at before the
# one above it compiles. Stage 6 has ~10 k lines of JUCE 2.1.2 to port and the
# harness (tools/show-ui) shows whatever is ready.
#
#   sw-gui-resources    skin bitmaps and fonts        <- builds
#   sw-gui-widgets      gui.{hpp,cpp}, the widget set <- builds
#   sw-gui              editor, modules, preset browser
#
# SPDX-License-Identifier: GPL-3.0-or-later

add_library(sw-gui-resources STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/gui/resources.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/gui/theme.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/le/utility/assertionHandler.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/le/utility/trace.cpp # assertionHandler routes through it
)

sw_force_include_odr_header(sw-gui-resources)

target_include_directories(sw-gui-resources PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# juce_gui_basics rather than juce_graphics: Theme is a LookAndFeel_V2. The
# other four modules come transitively.
target_link_libraries(sw-gui-resources
        PUBLIC juce::juce_gui_basics
        # assertionHandler.cpp prints a stack trace with the message.
        PRIVATE sw::assets sst-plugininfra
)

target_compile_definitions(sw-gui-resources PRIVATE LE_ENABLE_ASSERT_HANDLER)

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

# Where the user's presets live -- ~/Documents/SpectrumWorx and its platform
# equivalents, XDG included. PRIVATE: gui.cpp answers with a juce::File, so
# nothing above it needs to know where the answer came from.
target_link_libraries(sw-gui-widgets PRIVATE sst-plugininfra)

if (APPLE)
    # gui.mm is Cocoa only now. "-framework Carbon" stood beside Cocoa here and
    # was linked into every macOS build for the owned windows' HIView path,
    # which stage 6.4 deleted.
    target_sources(sw-gui-widgets PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/gui/gui.mm)
    target_link_libraries(sw-gui-widgets PRIVATE "-framework Cocoa")
endif()

# After the .mm above, so that it gets the header too.
sw_force_include_odr_header(sw-gui-widgets)

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
        ${CMAKE_CURRENT_SOURCE_DIR}/gui/modules/moduleWidgets.cpp

        ${CMAKE_CURRENT_SOURCE_DIR}/gui/editor/auxiliaryComponents.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/gui/editor/moduleMenuHolder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/gui/editor/presetLoading.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/gui/editor/spectrumWorxEditor.cpp

        ${CMAKE_CURRENT_SOURCE_DIR}/gui/preset_browser/presetBrowser.cpp
)

sw_force_include_odr_header(sw-gui)

target_link_libraries(sw-gui PUBLIC sw-gui-widgets)
