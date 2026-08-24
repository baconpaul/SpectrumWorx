////////////////////////////////////////////////////////////////////////////////
///
/// moduleHoverTests.cpp
/// --------------------
///
///   What the *pointer* does to a module strip and to the controls on it, which
/// as of issue #210 is a separate question from what a click does.
///
///   The rule, in one line: hovering marks, clicking selects, and marking never
/// becomes selecting. A hovered control wears the selection's ring at half
/// strength, takes the wheel, and -- when the user has asked for it -- lends the
/// LFO strip its own LFO until the pointer leaves.
///
/// \note No window anywhere in this file, unlike moduleControlFocusTests.cpp:
/// none of this goes through the keyboard, which is the whole point of it.
/// Selection, where a case needs one, is `ModuleControlBase::select()` -- the
/// click's own step with the keyboard left out.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "gui/editorHarness.hpp"

/// \note Before anything that names SW::Module, as moduleControlFocusTests.cpp
/// is: the module chain downcasts a node to it.
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

/// \brief An event over the middle of \p component, carrying no button: a hover
/// is not a press, and JUCE's own mouseEnter carries no modifiers either.
juce::MouseEvent pointerOver(juce::Component &component)
{
    auto const centre(component.getLocalBounds().getCentre().toFloat());
    return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), centre,
                            juce::ModifierKeys(), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &component,
                            &component, juce::Time(), centre, juce::Time(), 1, false);
}

void pointerEnters(juce::Component &component) { component.mouseEnter(pointerOver(component)); }
void pointerLeaves(juce::Component &component) { component.mouseExit(pointerOver(component)); }

/// \brief One editor with one effect in the first slot, and its strip.
///
/// \note Pitch Shifter because it is the effect the other GUI cases reach for and
/// because it has two knobs of its own, which is what a case about "this control
/// and that one" needs.
class RackUnderTest
{
  public:
    explicit RackUnderTest(SWTest::Instance &instance)
    {
        instance.openEditor();
        auto &editor(instance.editor());
        editor.addUserAddedModule(0);
        editor.resyncModuleRack();

        pStrip_ = editor.regionInSlot(0);
        REQUIRE(pStrip_ != nullptr);
        REQUIRE(pStrip_->module().numberOfEffectSpecificParameters() >= 2);
    }

    GUI::ModuleUI &strip() const { return *pStrip_; }
    GUI::ModuleControlBase &control(std::uint8_t const index) const
    {
        return pStrip_->effectSpecificParameterControl(index);
    }
    juce::Component &widget(std::uint8_t const index) const { return control(index).widget(); }

  private:
    GUI::ModuleUI *pStrip_{nullptr};
}; // class RackUnderTest

//------------------------------------------------------------------------------
} // anonymous namespace
//------------------------------------------------------------------------------

TEST_CASE("The pointer marks a control and its strip without selecting either",
          "[gui][modules][hover]")
{
    SWTest::HostSideJuce const juceIsUp;

    SWTest::Instance instance;
    RackUnderTest const rack(instance);
    auto &editor(instance.editor());

    REQUIRE(editor.hoveredControl() == nullptr);
    REQUIRE(editor.activeControl() == nullptr);

    pointerEnters(rack.widget(0));

    CHECK(editor.hoveredControl() == &rack.control(0));
    CHECK(editor.hoveredModule() == &rack.strip());
    CHECK(rack.control(0).isHovered());

    ///   And nothing was selected by it, which is the whole of the change: a
    /// sweep of the pointer across the rack used to be able to move the selection
    /// -- and with it the LFO strip and the shared controls -- under a preference
    /// nobody could get right. \see issue #210 and issue #139.
    CHECK(editor.activeControl() == nullptr);
    CHECK(editor.selectedModule() == nullptr);

    pointerLeaves(rack.widget(0));

    CHECK(editor.hoveredControl() == nullptr);
    CHECK(editor.hoveredModule() == nullptr);
}

TEST_CASE("The pointer moving from a strip onto its knob leaves the strip marked",
          "[gui][modules][hover]")
{
    ///   JUCE marks one component as being under the pointer, so entering a child
    /// *exits* its parent -- and the strip's own hover would go out from under
    /// the user's hand as they reached a knob on it. What makes it work without
    /// arithmetic is the order: the exit is delivered before the enter.
    SWTest::HostSideJuce const juceIsUp;

    SWTest::Instance instance;
    RackUnderTest const rack(instance);
    auto &editor(instance.editor());

    pointerEnters(rack.strip());
    REQUIRE(editor.hoveredModule() == &rack.strip());

    pointerLeaves(rack.strip()); // ...as JUCE announces the knob
    pointerEnters(rack.widget(0));

    CHECK(editor.hoveredModule() == &rack.strip());
    CHECK(editor.hoveredControl() == &rack.control(0));

    // ...and off the knob to somewhere that is not the rack at all.
    pointerLeaves(rack.widget(0));

    CHECK(editor.hoveredModule() == nullptr);
    CHECK(editor.hoveredControl() == nullptr);
}

TEST_CASE("A hover lends the LFO strip to the hovered control and gives it back",
          "[gui][modules][hover][lfo]")
{
    SWTest::HostSideJuce const juceIsUp;
    GUI::preferences().setPreviewLFOOnHover(true);

    SWTest::Instance instance;
    RackUnderTest const rack(instance);
    auto &editor(instance.editor());

    rack.control(0).select();
    REQUIRE(editor.activeControl() == &rack.control(0));
    REQUIRE(editor.displayedControl() == &rack.control(0));

    pointerEnters(rack.widget(1));

    // The strip follows the pointer...
    CHECK(editor.displayedControl() == &rack.control(1));
    CHECK(rack.control(1).isDisplayed());
    // ...and the ring does not. \see issue #210.
    CHECK(editor.activeControl() == &rack.control(0));

    pointerLeaves(rack.widget(1));

    CHECK(editor.displayedControl() == &rack.control(0));
    CHECK(editor.activeControl() == &rack.control(0));
}

TEST_CASE("With the preview off a hover leaves the LFO strip where it was",
          "[gui][modules][hover][lfo]")
{
    SWTest::HostSideJuce const juceIsUp;
    GUI::preferences().setPreviewLFOOnHover(false);

    SWTest::Instance instance;
    RackUnderTest const rack(instance);
    auto &editor(instance.editor());

    rack.control(0).select();
    REQUIRE(editor.displayedControl() == &rack.control(0));

    pointerEnters(rack.widget(1));

    // Marked, and that is all it is: the ring at half strength and the wheel.
    CHECK(rack.control(1).isHovered());
    CHECK(editor.displayedControl() == &rack.control(0));

    pointerLeaves(rack.widget(1));
    CHECK(editor.displayedControl() == &rack.control(0));

    GUI::preferences().setPreviewLFOOnHover(true);
}

TEST_CASE("Clicking a control the pointer was previewing keeps the strip on it",
          "[gui][modules][hover][lfo]")
{
    ///   The order that had to be got right: a click arrives on a control the
    /// preview is already showing, so the strip has to go back to the *outgoing*
    /// selection before the hand-over and then forward to the new one.
    /// `moduleControlDectivated()` asserts that the strip is showing whichever
    /// control it is told about, and would fire otherwise.
    SWTest::HostSideJuce const juceIsUp;
    GUI::preferences().setPreviewLFOOnHover(true);

    SWTest::Instance instance;
    RackUnderTest const rack(instance);
    auto &editor(instance.editor());

    rack.control(0).select();
    pointerEnters(rack.widget(1));
    REQUIRE(editor.displayedControl() == &rack.control(1));

    rack.control(1).select();

    CHECK(editor.activeControl() == &rack.control(1));
    CHECK(editor.displayedControl() == &rack.control(1));

    // ...and the pointer leaving does not now take the strip away with it.
    pointerLeaves(rack.widget(1));
    CHECK(editor.displayedControl() == &rack.control(1));
}
