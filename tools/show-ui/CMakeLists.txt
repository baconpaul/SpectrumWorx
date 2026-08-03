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

# \note sw-gui-resources -- which the skin and theme pages are made of -- was
# named here too, and the linker answered "ignoring duplicate libraries". It
# comes with sw-gui below, PUBLIC by way of sw-gui-widgets, headers included.
target_link_libraries(sw-show-ui PRIVATE
        juce::juce_gui_basics
        juce::juce_gui_extra

        # The editor page, and the skin and theme pages under it.
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
#
# \note "draws nothing" became a real assertion on 03.08.2026; until then
# --render wrote a PNG and returned 0, so a page painting solid black passed.
# See renderPage() in main.cpp.
if (SW_BUILD_TESTS)
    # \note editor-module is the one that exercises a filled slot: building a
    # module's widgets is what reaches SpectrumWorxEditor::fromChild and
    # EditorModuleInitialiser, neither of which the empty editor touches.
    # editor-module-cold does it against an engine with no sample rate, which is
    # the order a session restore arrives in and the one that used to assert.
    # editor-presets opens the preset browser, which is what the presets button
    # does and which segfaulted the first time anyone pressed it: nothing
    # headless covered that button until this page.
    foreach (page editor editor-module editor-module-cold editor-presets editor-settings environment skin theme)
        add_test(NAME show-ui-renders-${page}
                COMMAND sw-show-ui --render ${page}
                "${CMAKE_CURRENT_BINARY_DIR}/rendered-${page}.png")
    endforeach()

    # \note And one bank listed out of the binary, which is the other half of
    # what that button does: the root shows "Factory" and "User", and this is
    # what is behind the first of them.
    add_test(NAME show-ui-renders-editor-presets-factory-bank
            COMMAND sw-show-ui --render editor-presets
            "${CMAKE_CURRENT_BINARY_DIR}/rendered-editor-presets-bank.png")
    set_tests_properties(show-ui-renders-editor-presets-factory-bank
            PROPERTIES ENVIRONMENT "SW_SHOW_UI_PRESET_BANK=Echoes")

    ############################################################################
    # One module UI per effect
    #
    #   editor-module renders effect 0 and nothing else did, which week_two.md
    # §2.3 calls "1 of 57". Building a module's widgets is per-effect work --
    # every widget type an effect uses is a template instantiation of its own and
    # the ranges are quantised against the engine setup per parameter -- so one
    # effect rendering says very little about the other fifty-six.
    #
    # \note The list is read out of effectsList.hpp rather than copied here.
    # That header calls itself "the single source of truth for which effects
    # exist, in which order", the order is ABI, and an effect appended to it
    # should grow a test without anybody remembering to. A `foreach(RANGE 0 56)`
    # would have been shorter and would have gone quietly stale.
    ############################################################################

    set(effectsListHeader
            "${CMAKE_SOURCE_DIR}/src/le/spectrumworx/effects/configuration/effectsList.hpp")

    # \note file(READ) and MATCHALL rather than file(STRINGS ... REGEX), which
    # looks like the right tool and is not: every line of that macro ends in a
    # backslash, and the backslash escapes the ';' CMake separates list elements
    # with -- so all 57 entries come back as one element with literal semicolons
    # in it. Anchoring the match to the newline also excludes the two lines of
    # the header's own doc comment that describe the macro's shape.
    file(READ "${effectsListHeader}" effectsListSource)
    string(REGEX MATCHALL "\n[ \t]*x\\([^\n]*\\)" effectListEntries "${effectsListSource}")

    set(effectIndex 0)
    foreach (entry IN LISTS effectListEntries)
        # x( folder, module, EffectName, GroupName )
        string(REGEX REPLACE "^[ \t\n]*x\\([^,]+,[^,]+,[ \t]*([A-Za-z0-9_]+)[ \t]*,.*$" "\\1"
                effect "${entry}")
        add_test(NAME show-ui-renders-module-${effect}
                COMMAND sw-show-ui --render editor-module
                "${CMAKE_CURRENT_BINARY_DIR}/rendered-module-${effect}.png")
        set_tests_properties(show-ui-renders-module-${effect}
                PROPERTIES ENVIRONMENT "SW_SHOW_UI_EFFECT=${effectIndex}")
        math(EXPR effectIndex "${effectIndex} + 1")
    endforeach ()

    # A regex that quietly matched nothing would register no tests and read as a
    # suite that simply has none, which is the one failure mode of parsing a C++
    # header from CMake worth guarding against. The count itself is not asserted
    # here -- effectsListTests.cpp static_asserts it against the same header, and
    # a number in this file would be a third copy of it.
    if (effectIndex LESS 1)
        message(FATAL_ERROR "no effects parsed out of ${effectsListHeader}")
    endif ()
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
