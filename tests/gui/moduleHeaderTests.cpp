////////////////////////////////////////////////////////////////////////////////
///
/// moduleHeaderTests.cpp
/// ---------------------
///
///   The two lines above the rack -- an effect's name and, under it, either its
/// description or the name of the control the user is on.
///
///   They are the editor's state rather than any strip's, which is what makes
/// them worth a file: every strip that goes away has to leave them saying
/// something true, and the strip is in no position to say what that is. So the
/// rule is that the header is *derived* from what the editor is still pointing
/// at -- the active control, else the selected strip, else nothing -- and every
/// case here is one way of losing the thing it was about.
///
/// \note No window, for the reason moduleHoverTests.cpp has none: selection here
/// is `ModuleUI::activate()`, the click's own step with the keyboard left out.
/// Offscreen JUCE refuses keyboard focus outright.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "gui/editorHarness.hpp"

/// \note Before anything that names SW::Module, as moduleHoverTests.cpp is: the
/// module chain downcasts a node to it.
#include "core/modules/moduleDSPAndGUI.hpp"

#include "gui/modules/moduleControl.hpp"
#include "gui/modules/moduleUI.hpp"
#include "gui/preferences.hpp"

#include <juce_gui_basics/juce_gui_basics.h>

#include <catch2/catch_test_macros.hpp>
//------------------------------------------------------------------------------
namespace
{
using namespace LE;
using namespace LE::SW;

namespace GUI = LE::SW::GUI;

/// \note In the plugin a removal arrives as a posted message, because it is
/// reached from inside the strip's own button callback and the strip cannot be
/// destroyed under it. A headless run has no message loop, so the resync is
/// driven directly. \see twoInstanceTests.cpp.
void settle(GUI::SpectrumWorxEditor &editor) { editor.resyncModuleRack(); }

GUI::ModuleUI &addModule(GUI::SpectrumWorxEditor &editor, std::int8_t const effect,
                         std::uint8_t const slot)
{
    editor.addUserAddedModule(effect);
    settle(editor);
    auto *const pStrip(editor.regionInSlot(slot));
    REQUIRE(pStrip != nullptr);
    return *pStrip;
}

/// \brief An event over the middle of \p component, carrying no button.
juce::MouseEvent pointerOver(juce::Component &component)
{
    auto const centre(component.getLocalBounds().getCentre().toFloat());
    return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), centre,
                            juce::ModifierKeys(), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &component,
                            &component, juce::Time(), centre, juce::Time(), 1, false);
}

//------------------------------------------------------------------------------
} // anonymous namespace
//------------------------------------------------------------------------------

TEST_CASE("Emptying the rack empties the header above it", "[gui][modules][header]")
{
    ///   The reported fault: eject the last module and the effect's name and
    /// description stay on screen over an empty rack. \see issue #237.
    ///
    ///   Invisible with strips left over, because the next click on one of them
    /// overwrites the header; at zero there is nothing left to click.
    SWTest::HostSideJuce const juceIsUp;

    SWTest::Instance instance;
    instance.openEditor();
    auto &editor(instance.editor());

    auto &strip(addModule(editor, 0, 0));
    strip.activate();

    REQUIRE(editor.selectedModule() == &strip);
    REQUIRE(editor.headerName() == strip.getName());
    REQUIRE(editor.headerName().isNotEmpty());
    REQUIRE(editor.headerDescription() == strip.description());
    REQUIRE(editor.headerDescription().isNotEmpty());

    editor.removeModule(strip);
    settle(editor);

    REQUIRE(editor.regionInSlot(0) == nullptr);
    CHECK(editor.selectedModule() == nullptr);
    CHECK(editor.headerName().isEmpty());
    CHECK(editor.headerDescription().isEmpty());
    CHECK(editor.headerValue().isEmpty());

    instance.closeEditor();
}

TEST_CASE("Removing a strip nobody is on leaves the header alone", "[gui][modules][header]")
{
    ///   The other half of the fix: the header is blanked because it was naming
    /// the strip that went, not because a strip went. Removing one while the
    /// user is on another must not wipe what that other one is saying.
    SWTest::HostSideJuce const juceIsUp;

    SWTest::Instance instance;
    instance.openEditor();
    auto &editor(instance.editor());

    addModule(editor, 0, 0);
    auto &second(addModule(editor, 1, 1));
    second.activate();

    REQUIRE(editor.selectedModule() == &second);
    auto const name(editor.headerName());
    auto const description(editor.headerDescription());
    REQUIRE(name.isNotEmpty());

    auto *const pFirst(editor.regionInSlot(0));
    REQUIRE(pFirst != nullptr);
    editor.removeModule(*pFirst);
    settle(editor);

    // the survivor moved down a slot, but it is still the selected one and still
    // what the header is about
    REQUIRE(editor.regionInSlot(0) == &second);
    CHECK(editor.selectedModule() == &second);
    CHECK(editor.headerName() == name);
    CHECK(editor.headerDescription() == description);

    instance.closeEditor();
}

TEST_CASE("Removing a strip the pointer was previewing restates the header",
          "[gui][modules][header]")
{
    ///   A hover over a control on an *unselected* strip lends the header that
    /// control's name without selecting anything. Eject that strip and the
    /// borrowed name has to go back to whatever the selection says -- the same
    /// derivation the pointer leaving would have run.
    SWTest::HostSideJuce const juceIsUp;

    SWTest::Instance instance;
    instance.openEditor();
    auto &editor(instance.editor());

    GUI::preferences().setPreviewLFOOnHover(true);

    auto &selected(addModule(editor, 0, 0));
    auto &hovered(addModule(editor, 1, 1));
    selected.activate();

    REQUIRE(hovered.module().numberOfEffectSpecificParameters() >= 1);

    auto &control(hovered.effectSpecificParameterControl(0));
    control.widget().mouseEnter(pointerOver(control.widget()));

    REQUIRE(editor.displayedControl() == &control);
    REQUIRE(editor.headerName() == hovered.getName());

    editor.removeModule(hovered);
    settle(editor);

    CHECK(editor.displayedControl() == nullptr);
    CHECK(editor.selectedModule() == &selected);
    CHECK(editor.headerName() == selected.getName());
    CHECK(editor.headerDescription() == selected.description());

    instance.closeEditor();
}
