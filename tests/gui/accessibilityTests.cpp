////////////////////////////////////////////////////////////////////////////////
///
/// accessibilityTests.cpp
/// ----------------------
///
///   What a screen reader is told and what the keyboard does.
///
/// \note Handlers are built off the widget: JUCE only hands one out on a window.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "gui/editorHarness.hpp"

// before anything that names SW::Module: the chain downcasts to it
#include "core/modules/moduleDSPAndGUI.hpp"

#include "gui/accessibility/keyboardEdits.hpp"
#include "gui/accessibility/traversal.hpp"
#include "gui/editor/auxiliaryComponents.hpp"
#include "gui/modules/moduleControl.hpp"
#include "gui/modules/moduleUI.hpp"
#include "gui/preset_browser/presetBrowser.hpp"

#include <juce_gui_basics/juce_gui_basics.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>
//------------------------------------------------------------------------------
namespace
{
//------------------------------------------------------------------------------

using namespace LE;
using namespace LE::SW;
using GUI::ModuleControlBase;
using Editor = GUI::SpectrumWorxEditor;

template <typename Widget> ModuleControlBase *firstControlOfType(GUI::ModuleUI &moduleUI)
{
    auto const parameters(moduleUI.module().numberOfEffectSpecificParameters());
    for (std::uint8_t index(0); index < parameters; ++index)
    {
        auto &control(moduleUI.effectSpecificParameterControl(index));
        if (dynamic_cast<Widget *>(&control.widget()) != nullptr)
            return &control;
    }
    return nullptr;
}

GUI::ModuleUI &stripWith(Editor &editor, char const *const effect, std::uint8_t const slot = 0)
{
    editor.addUserAddedModule(static_cast<std::uint8_t>(SWTest::effectByStreamingName(effect)));
    editor.resyncModuleRack();
    auto *const pModuleUI(editor.regionInSlot(slot));
    REQUIRE(pModuleUI != nullptr);
    return *pModuleUI;
}

bool hostHeardValueFor(SWTest::Instance const &instance, ParameterID const id)
{
    auto const &edits(instance.hostEdits());
    return std::any_of(edits.begin(), edits.end(), [&](SWTest::HostEdit const &edit) {
        return (edit.kind == SWTest::HostEdit::Kind::Value) && (edit.id == id.binaryValue);
    });
}

juce::KeyPress key(int const code, int const modifiers = 0)
{
    return juce::KeyPress(code, juce::ModifierKeys(modifiers), 0);
}

std::ptrdiff_t positionOf(std::vector<juce::Component *> const &order,
                          juce::Component const &component)
{
    auto const found(std::find(order.begin(), order.end(), &component));
    REQUIRE(found != order.end());
    return found - order.begin();
}

//------------------------------------------------------------------------------
} // anonymous namespace
//------------------------------------------------------------------------------

TEST_CASE("A module knob names its module and reads its value", "[gui][accessibility]")
{
    SWTest::HostSideJuce const juceIsUp;
    SWTest::Instance instance;
    instance.openEditor();
    auto &editor(instance.editor());

    stripWith(editor, "Freeze", 0);
    auto &second(stripWith(editor, "Freeze", 1));

    auto *const pControl(firstControlOfType<GUI::ModuleKnob>(second));
    REQUIRE(pControl != nullptr);

    auto const pHandler(pControl->widget().createAccessibilityHandler());
    REQUIRE(pHandler != nullptr);
    CHECK(pHandler->getRole() == juce::AccessibilityRole::slider);
    CHECK(pHandler->getTitle() == "Module 2 - " + juce::String(pControl->name()));

    auto *const pValue(pHandler->getValueInterface());
    REQUIRE(pValue != nullptr);
    CHECK(pValue->getCurrentValueAsString() == pControl->getValueText());
    CHECK_FALSE(pValue->isReadOnly());

    auto const pStrip(second.createAccessibilityHandler());
    CHECK(pStrip->getRole() == juce::AccessibilityRole::group);
    CHECK(pStrip->getTitle() == "Module 2: " + second.getName());
}

TEST_CASE("The arrow keys edit a module knob and the host hears it", "[gui][accessibility]")
{
    SWTest::HostSideJuce const juceIsUp;
    SWTest::Instance instance;
    instance.openEditor();
    auto &editor(instance.editor());

    auto &strip(stripWith(editor, "Freeze"));
    auto *const pControl(firstControlOfType<GUI::ModuleKnob>(strip));
    REQUIRE(pControl != nullptr);
    pControl->select();

    auto &knob(dynamic_cast<juce::Slider &>(pControl->widget()));
    knob.setValue(knob.getMinimum(), juce::dontSendNotification);
    auto const before(pControl->getValue());
    instance.hostEdits().clear();

    CHECK(pControl->widget().keyPressed(key(juce::KeyPress::upKey)));
    CHECK(pControl->getValue() > before);
    CHECK(hostHeardValueFor(instance, pControl->parameterMenuID()));

    SECTION("home goes to the top of the range")
    {
        CHECK(pControl->widget().keyPressed(key(juce::KeyPress::homeKey)));
        CHECK(knob.getValue() >= knob.getMaximum() - knob.getInterval());
    }
}

TEST_CASE("A knob an LFO drives refuses the keyboard", "[gui][accessibility][lfo]")
{
    SWTest::HostSideJuce const juceIsUp;
    SWTest::Instance instance;
    instance.openEditor();
    auto &editor(instance.editor());

    auto &strip(stripWith(editor, "Freeze"));
    auto *const pControl(firstControlOfType<GUI::ModuleKnob>(strip));
    REQUIRE(pControl != nullptr);
    pControl->select();
    editor.setLFOEnabled(*pControl, true);
    REQUIRE(pControl->isLFOEnabled());

    auto const before(pControl->getValue());
    CHECK(pControl->widget().keyPressed(key(juce::KeyPress::upKey)));
    CHECK(pControl->getValue() == before);

    auto const pHandler(pControl->widget().createAccessibilityHandler());
    CHECK(pHandler->getValueInterface()->isReadOnly());
}

TEST_CASE("The menu keys raise a control's parameter menu, as in Six Sines",
          "[gui][accessibility][menu]")
{
    SWTest::HostSideJuce const juceIsUp;
    SWTest::Instance instance;
    instance.openEditor();
    auto &editor(instance.editor());

    auto &strip(stripWith(editor, "Freeze"));
    auto *const pControl(firstControlOfType<GUI::ModuleKnob>(strip));
    REQUIRE(pControl != nullptr);

    REQUIRE(juce::Component::getNumCurrentlyModalComponents() == 0);

    SECTION("shift F10")
    {
        CHECK(pControl->widget().keyPressed(
            key(juce::KeyPress::F10Key, juce::ModifierKeys::shiftModifier)));
    }
    SECTION("the context menu key")
    {
        CHECK(pControl->widget().keyPressed(key(GUI::Accessibility::contextMenuKeyCode)));
    }

    CHECK(juce::Component::getNumCurrentlyModalComponents() == 1);
    juce::PopupMenu::dismissAllActiveMenus();
}

TEST_CASE("An LED's value moves without juce::Button announcing it", "[gui][accessibility][lfo]")
{
    SWTest::HostSideJuce const juceIsUp;
    SWTest::Instance instance;
    instance.openEditor();
    auto &editor(instance.editor());

    // TuneWorx, whose semitones are LEDs
    auto &strip(stripWith(editor, "TuneWorx"));
    auto *const pControl(firstControlOfType<GUI::ModuleLEDTextButton>(strip));
    REQUIRE(pControl != nullptr);
    auto &button(dynamic_cast<juce::Button &>(pControl->widget()));

    auto const before(pControl->getValue());
    pControl->setValue(before ? 0.0f : 1.0f);

    CHECK(pControl->getValue() != before);
    // juce::Button::setToggleState() is what posts a value change
    CHECK_FALSE(button.getToggleState());

    auto const pHandler(pControl->widget().createAccessibilityHandler());
    CHECK(pHandler->getRole() == juce::AccessibilityRole::toggleButton);
    CHECK(pHandler->getCurrentState().isChecked() == (pControl->getValue() != 0));
}

TEST_CASE("The tab key walks the knobs, then the rack in slot order", "[gui][accessibility]")
{
    SWTest::HostSideJuce const juceIsUp;
    SWTest::Instance instance;
    instance.openEditor();
    auto &editor(instance.editor());

    auto &first(stripWith(editor, "Freeze", 0));
    auto &second(stripWith(editor, "Freeze", 1));

    GUI::Accessibility::Traverser traverser(GUI::Accessibility::Traverser::Purpose::keyboard);
    auto const order(traverser.getAllComponents(&editor));

    auto const &firstKnob(first.effectSpecificParameterControl(0).widget());
    auto const &secondKnob(second.effectSpecificParameterControl(0).widget());

    CHECK(positionOf(order, editor.inKnob()) < positionOf(order, editor.mixKnob()));
    CHECK(positionOf(order, editor.mixKnob()) < positionOf(order, firstKnob));
    CHECK(positionOf(order, firstKnob) < positionOf(order, secondKnob));

    // and a strip that moves takes its controls with it
    editor.applyModuleDrop(0, {Editor::ModuleDrop::swap, 1});
    editor.resyncModuleRack();
    auto const moved(traverser.getAllComponents(&editor));
    CHECK(positionOf(moved, secondKnob) < positionOf(moved, firstKnob));
}

TEST_CASE("Return opens a folder in the preset browser and backspace leaves it",
          "[gui][accessibility][presets]")
{
    SWTest::HostSideJuce const juceIsUp;
    SWTest::Instance instance;
    instance.openEditor(Editor::PanelPlacement::overlay);
    auto &editor(instance.editor());
    editor.showFactoryBank({});

    auto *const pBrowser(editor.presetBrowser());
    REQUIRE(pBrowser != nullptr);

    juce::ListBox *pList(nullptr);
    for (auto *const pChild : pBrowser->getChildren())
        if (auto *const pListBox = dynamic_cast<juce::ListBox *>(pChild))
            pList = pListBox;
    REQUIRE(pList != nullptr);

    auto &model(*pList->getListBoxModel());
    REQUIRE(model.getNumRows() > 0);
    auto const firstBank(model.getNameForRow(0));
    CHECK(firstBank.endsWith(" folder"));

    pList->selectRow(0);
    CHECK(pList->keyPressed(key(juce::KeyPress::returnKey)));
    CHECK(model.getNameForRow(0) != firstBank);

    // juce::ListBox's backspace; the key listener for no selection is out of reach
    model.deleteKeyPressed(pList->getLastRowSelected());
    CHECK(model.getNameForRow(0) == firstBank);
}
