# sw-show-ui — a desktop window that hosts the SpectrumWorx GUI with no plugin
# under it. See main.cpp for why.
#
# Deliberately not a juce_add_gui_app(): add_clap_juce_shim() sets
# JUCE_MODULES_ONLY, under which JUCE's top-level CMakeLists returns before
# JUCEUtils.cmake is included, so none of the juce_add_* helpers exist on this
# route. A plain executable plus START_JUCE_APPLICATION is all a GUI app needs.
#
# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(sw-show-ui
        main.cpp
        page.cpp
        pages/editorPage.cpp
        pages/environmentPage.cpp
        pages/skinPage.cpp
        pages/themePage.cpp
)

target_link_libraries(sw-show-ui PRIVATE
        juce::juce_gui_basics
        juce::juce_gui_extra
        sw-gui-resources

        # The editor page.
        sw-gui
)

# Plugin2HostInteropControler's out-of-line half. It lives in sw-impl, and
# linking that would drag the CLAP entry point into a plain desktop app -- so
# compile the two sources here instead.
target_sources(sw-show-ui PRIVATE
        ${CMAKE_SOURCE_DIR}/src/core/host_interop/plugin2Host.cpp
        ${CMAKE_SOURCE_DIR}/src/core/host_interop/host2Plugin.cpp
)

# After the target_sources() above -- the two src/ files need the header rather
# more than this directory's own do.
sw_force_include_odr_header(sw-show-ui)

target_compile_definitions(sw-show-ui PRIVATE
        JUCE_STANDALONE_APPLICATION=1
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CAMERA=0
        JUCE_REPORT_APP_USAGE=0

        # The editor opens menus and colour pickers from mouse handlers, which
        # in 2016 ran a nested modal loop. Keep the harness under the same rule
        # the plugin will live by so that a synchronous call is a compile error
        # here and not a hang in a DAW.
        JUCE_MODAL_LOOPS_PERMITTED=0
)

# Render every page offscreen. Cheap, needs no display, and it is the only
# thing that notices a page which compiles but throws, asserts or draws
# nothing -- which is most of how GUI code fails.
if (SW_BUILD_TESTS)
    # \note editor-module is the one that exercises a filled slot: building a
    # module's widgets is what reaches SpectrumWorxEditor::fromChild and
    # EditorModuleInitialiser, neither of which the empty editor touches.
    # editor-module-cold does it against an engine with no sample rate, which is
    # the order a session restore arrives in and the one that used to assert.
    foreach (page editor editor-module editor-module-cold environment skin theme)
        add_test(NAME show-ui-renders-${page}
                COMMAND sw-show-ui --render ${page}
                "${CMAKE_CURRENT_BINARY_DIR}/rendered-${page}.png")
    endforeach()
endif()

if (APPLE)
    # A bare Mach-O gets no dock icon and cannot take keyboard focus; a bundle
    # can, and `open` / a double click then behave like any other app. The
    # binary is still directly runnable at
    # sw-show-ui.app/Contents/MacOS/sw-show-ui.
    set_target_properties(sw-show-ui PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_GUI_IDENTIFIER "${SW_CLAP_ID}.show-ui"
            MACOSX_BUNDLE_BUNDLE_NAME "sw-show-ui"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
endif()
