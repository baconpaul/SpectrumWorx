////////////////////////////////////////////////////////////////////////////////
///
/// discreteParameterTests.cpp
/// --------------------------
///
///   What an enumerated module parameter's combo box lists, in what order, and
/// what the mouse wheel does to the row that is showing.
///
/// \note The menu is not opened: a menu is a modal window and a test binary has
/// no message loop to answer one with (\see the note at the top of
/// knobMenuTests.cpp). The combo box *is* its own menu here -- GUI::ComboBox
/// derives from GUI::PopupMenuWithSelection -- so the rows are readable without
/// showing it, which is the whole of what these cases ask about.
///
/// \note One case below is the exception and pays for it: what a wheel does
/// while the list is down can only be asked with the list down.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "gui/editorHarness.hpp"

/// \note Before anything that names SW::Module, as elsewhere: the module chain
/// downcasts a node to it and this is the header with the complete type.
#include "core/modules/moduleDSPAndGUI.hpp"

#include "gui/modules/moduleControl.hpp"
#include "gui/modules/moduleUI.hpp"

#include "le/spectrumworx/effects/configuration/effectNames.hpp"
#include "le/spectrumworx/effects/ethereal/ethereal.hpp"
#include "le/spectrumworx/effects/tune_worx/tuneWorx.hpp"
#include "le/spectrumworx/effects/vaxateer/vaxateer.hpp"

#include "le/parameters/parametersUtilities.hpp" // IndexOf

#include "core/host_interop/parameters.hpp" // GlobalParameters

#include <juce_gui_basics/juce_gui_basics.h>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>
//------------------------------------------------------------------------------
namespace
{
//------------------------------------------------------------------------------

using namespace LE;
using namespace LE::SW;

using Key = Effects::Detail::TuneWorxBase::Key;

/// \brief The first control of \p moduleUI that is a combo box.
///
/// \note By parameter index rather than by walking the children, as the other
/// GUI cases do and for the same reason: the widget storage is a compile-time
/// chain of one base class per parameter, so there is no runtime list to iterate.
GUI::ComboBox *firstComboBox(GUI::ModuleUI &moduleUI)
{
    auto const parameters(moduleUI.module().numberOfEffectSpecificParameters());
    for (std::uint8_t index(0); index < parameters; ++index)
    {
        auto &control(moduleUI.effectSpecificParameterControl(index));
        if (auto *const pComboBox = dynamic_cast<GUI::ComboBox *>(&control.widget()))
            return pComboBox;
    }
    return nullptr;
}

/// \brief \p moduleUI's combo box for effect-specific parameter \p index.
GUI::ComboBox &comboBoxFor(GUI::ModuleUI &moduleUI, std::uint8_t const index)
{
    auto &control(moduleUI.effectSpecificParameterControl(index));
    auto *const pComboBox(dynamic_cast<GUI::ComboBox *>(&control.widget()));
    REQUIRE(pComboBox != nullptr);
    return *pComboBox;
}

/// \brief The one strip of \p effectName, in slot 0.
GUI::ModuleUI &stripFor(GUI::SpectrumWorxEditor &editor, char const *const effectName)
{
    auto const effect(Effects::effectIndex(effectName));
    REQUIRE(effect >= 0);
    editor.addUserAddedModule(static_cast<std::uint8_t>(effect));
    editor.resyncModuleRack();
    auto *const pModuleUI(editor.regionInSlot(0));
    REQUIRE(pModuleUI != nullptr);
    return *pModuleUI;
}

//------------------------------------------------------------------------------
} // anonymous namespace
//------------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
///
/// \brief One wheel notch over \p component, positive being away from the user.
///
/// \note Handed straight to `mouseWheelMove()`, which is half a mouse -- the
/// same half the mouseDown cases elsewhere use, and enough for the one question
/// here. Through `juce::Component`, because the override is protected on
/// GUI::ComboBox and public on the base.
///
/// \note 0.3 rather than 1: GUI::ComboBox counts five notches to a row, and a
/// notch is what a wheel detent sends. One row per call is what makes the cases
/// below readable.
///
////////////////////////////////////////////////////////////////////////////////

void scroll(juce::Component &component, float const deltaY)
{
    juce::MouseWheelDetails wheel{};
    wheel.deltaX = 0;
    wheel.deltaY = deltaY;
    wheel.isReversed = false;
    wheel.isSmooth = false;
    wheel.isInertial = false;

    auto const centre(component.getLocalBounds().getCentre().toFloat());
    juce::MouseEvent const event(juce::Desktop::getInstance().getMainMouseSource(), centre,
                                 juce::ModifierKeys(), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &component,
                                 &component, juce::Time(), centre, juce::Time(), 1, false);
    component.mouseWheelMove(event, wheel);
}

/// Every descendant of \p root that is a \p Widget, in child order.
template <typename Widget> std::vector<Widget *> descendantsOfType(juce::Component &root)
{
    std::vector<Widget *> found;
    for (auto *const pChild : root.getChildren())
    {
        if (auto *const pWidget(dynamic_cast<Widget *>(pChild)); pWidget)
            found.push_back(pWidget);
        for (auto *const pDeeper : descendantsOfType<Widget>(*pChild))
            found.push_back(pDeeper);
    }
    return found;
}

////////////////////////////////////////////////////////////////////////////////
///
/// \brief The settings panel's FFT size box, open on the Engine page.
///
/// \note The settings panel rather than a module strip, and the reason is worth
/// stating: the two share `GUI::ComboBox` and its wheel handling, but a module
/// strip's box takes the module selection before it will move -- see
/// `DiscreteParameter::mouseWheelMove()` -- and focus needs a window, which a
/// test binary has to go out of its way to get. What is different about the
/// module box is one guard; what is the same is everything these cases ask
/// about. \see tests/gui/moduleControlFocusTests.cpp for the window.
///
////////////////////////////////////////////////////////////////////////////////

GUI::TitledComboBox &fftSizeBox(GUI::SpectrumWorxEditor &editor)
{
    editor.showSettings(GUI::SpectrumWorxEditor::enginePageIndex);

    /// \note The three engine boxes are laid out top to bottom in declaration
    /// order and the FFT size is the first of them. Said by asking what it holds
    /// rather than by trusting the order.
    auto const boxes(descendantsOfType<GUI::TitledComboBox>(editor));
    REQUIRE(boxes.size() == 3);
    return *boxes.front();
}

TEST_CASE("Tune Worx's scale root is listed from C, and still valued from A",
          "[gui][modules][combo]")
{
    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \note Two claims, and they are two because they disagree by design.
    ///
    ///   What a musician reads down is a chromatic scale, which starts at C --
    /// that is issue #89 and it is a statement about the *rows*.
    ///
    ///   What the parameter holds is an index, and the index is A-based: it is
    /// what every `.swp` since 2011 has stored, what a host automates, and what
    /// the DSP adds to a note offset off a 27.5 Hz A (musicalScales.cpp). So
    /// reordering the enumerators would have silently retuned every preset that
    /// names a key. The rows move; the values do not, and each row carries its
    /// own.
    ///
    ////////////////////////////////////////////////////////////////////////////
    SWTest::HostSideJuce const juceIsUp;

    SWTest::Instance instance;
    instance.openEditor();

    auto &strip(stripFor(instance.editor(), "TuneWorx"));
    auto *const pComboBox(firstComboBox(strip));
    REQUIRE(pComboBox != nullptr);
    auto &comboBox(*pComboBox);

    std::vector<Key::value_type> const expected{Key::C,   Key::Cis, Key::D,   Key::Dis,
                                                Key::E,   Key::F,   Key::Fis, Key::G,
                                                Key::Gis, Key::A,   Key::Ais, Key::B};

    REQUIRE(comboBox.numberOfItems() == expected.size());

    auto const &names(Parameters::DiscreteValues<Key>::strings);
    for (unsigned int row(0); row < expected.size(); ++row)
    {
        INFO("row " << row);
        auto const value(expected[row]);
        CHECK(comboBox.getItemText(row) == juce::String(names[value]));

        // The row's own value, which is what selecting it writes.
        comboBox.setSelectedIndex(row);
        CHECK(comboBox.getSelectedID() == value);
    }

    // C first is the point of the reorder; A stays value zero.
    CHECK(comboBox.getItemText(0) == "C");
    CHECK(Key::A == 0);
}

TEST_CASE("Every black key is named both ways", "[gui][modules][combo]")
{
    /// \note Sharp first and flat after it, for all five, because there is no key
    /// signature here to pick one -- a chromatic root is any of the twelve. \see
    /// issue #89 and the note beside the value strings in tuneWorx.hpp.
    auto const &names(Parameters::DiscreteValues<Key>::strings);

    CHECK(std::string(names[Key::Ais]) == "A#/Bb");
    CHECK(std::string(names[Key::Cis]) == "C#/Db");
    CHECK(std::string(names[Key::Dis]) == "D#/Eb");
    CHECK(std::string(names[Key::Fis]) == "F#/Gb");
    CHECK(std::string(names[Key::Gis]) == "G#/Ab");

    // ...and the seven naturals are still just themselves.
    CHECK(std::string(names[Key::A]) == "A");
    CHECK(std::string(names[Key::B]) == "B");
    CHECK(std::string(names[Key::C]) == "C");
    CHECK(std::string(names[Key::D]) == "D");
    CHECK(std::string(names[Key::E]) == "E");
    CHECK(std::string(names[Key::F]) == "F");
    CHECK(std::string(names[Key::G]) == "G");
}

TEST_CASE("A shared target is listed in the order it is chosen in", "[gui][modules][combo]")
{
    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \note `CommonParameters::Mode` is declared Both, Magnitudes, Phases and
    /// listed Magnitudes, Phases, Both: the two things it can be, and then the
    /// one that is both of them. Zero stays Both because zero is the default and
    /// because that is what every preset and automation lane written since 2011
    /// calls it -- so the rows move and the values do not, which is the whole of
    /// what `MenuOrder` is for.
    ///
    ///   The specialisation is on the shared parameter rather than on an effect,
    /// so it is one declaration for the three effects that take it. Ethereal is
    /// the one asked here; Inserter and Swappah get the same menu.
    ///
    ////////////////////////////////////////////////////////////////////////////
    SWTest::HostSideJuce const juceIsUp;

    SWTest::Instance instance;
    instance.openEditor();

    auto &strip(stripFor(instance.editor(), "Ethereal"));

    using Mode = Effects::Ethereal::Mode;
    auto &comboBox(
        comboBoxFor(strip, Parameters::IndexOf<Effects::Ethereal::Parameters, Mode>::value));

    std::vector<Mode::value_type> const expected{Mode::Magnitudes, Mode::Phases, Mode::Both};

    REQUIRE(comboBox.numberOfItems() == expected.size());

    auto const &names(Parameters::DiscreteValues<Mode>::strings);
    for (unsigned int row(0); row < expected.size(); ++row)
    {
        INFO("row " << row);
        auto const value(expected[row]);
        CHECK(comboBox.getItemText(row) == juce::String(names[value]));

        // The row's own value, which is what selecting it writes.
        comboBox.setSelectedIndex(row);
        CHECK(comboBox.getSelectedID() == value);
    }

    CHECK(comboBox.getItemText(0) == "Magnitudes");
    CHECK(Mode::Both == 0);

    /// \note The parameter still reads its own declaration order, which is what
    /// the host, a preset and the DSP switch see. Only the menu was reordered.
    CHECK(std::string(names[0]) == "Both");
}

TEST_CASE("A parameter with no menu order of its own is listed as declared",
          "[gui][modules][combo]")
{
    /// \note The other half: `MenuOrder` is empty by default and `menuOrder()`
    /// then hands back 0, 1, 2 ... so a row number *is* a value, which is what
    /// all but two of the enumerated parameters want. Ethereal's swap condition
    /// is the nearest one to ask, and it sits on the same strip as the target
    /// above.
    SWTest::HostSideJuce const juceIsUp;

    SWTest::Instance instance;
    instance.openEditor();

    auto &strip(stripFor(instance.editor(), "Ethereal"));

    using Condition = Effects::Ethereal::Condition;
    auto &comboBox(
        comboBoxFor(strip, Parameters::IndexOf<Effects::Ethereal::Parameters, Condition>::value));

    REQUIRE(comboBox.numberOfItems() == Condition::numberOfDiscreteValues);
    for (unsigned int row(0); row < comboBox.numberOfItems(); ++row)
    {
        INFO("row " << row);
        comboBox.setSelectedIndex(row);
        CHECK(comboBox.getSelectedID() == row);
    }
}

TEST_CASE("A swap condition is listed in full and read abbreviated", "[gui][modules][combo]")
{
    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \note Issue #120. A menu is as wide as its longest line; the box that
    /// shows what was picked is sixty pixels, and "Main: >Thr >Side" in sixty
    /// pixels is a smear. So a value carries two strings and Vaxateer's swap
    /// condition is the example: the menu keeps the sentence, the box gets the
    /// initials.
    ///
    ///   Nothing else moves -- `getItemText()` is what the host readout, the
    /// knob menu and a preset all go through, and it is still the full one.
    ///
    ////////////////////////////////////////////////////////////////////////////
    SWTest::HostSideJuce const juceIsUp;

    SWTest::Instance instance;
    instance.openEditor();

    auto &strip(stripFor(instance.editor(), "Vaxateer"));

    /// \note By index rather than by "the first combo box": Vaxateer's RMS target
    /// is one too, and it is the one before this.
    using Mode = Effects::Vaxateer::Mode;
    auto &comboBox(
        comboBoxFor(strip, Parameters::IndexOf<Effects::Vaxateer::Parameters, Mode>::value));

    REQUIRE(comboBox.numberOfItems() == Mode::numberOfDiscreteValues);

    comboBox.setSelectedID(Mode::M6);
    CHECK(comboBox.getSelectedItemText() == "Sidechain: >Threshold <Main");
    CHECK(comboBox.getSelectedItemShortText() == "SC: >T <M");

    // Every row, so that a list that grows an entry cannot go half-abbreviated.
    auto const &full(Parameters::DiscreteValues<Mode>::strings);
    auto const &shortened(Parameters::shortValueStrings<Mode>());
    for (unsigned int row(0); row < comboBox.numberOfItems(); ++row)
    {
        INFO("row " << row);
        CHECK(comboBox.getItemText(row) == juce::String(full[row]));
        CHECK(comboBox.getItemShortText(row) == juce::String(shortened[row]));
        CHECK(comboBox.getItemShortText(row).length() < comboBox.getItemText(row).length());
    }
}

TEST_CASE("A parameter with no abbreviations reads the same either way", "[gui][modules][combo]")
{
    /// \note The other half of issue #120, and the half that covers 22 of the 23
    /// enumerated parameters: `ShortValues` defaults to null and
    /// `shortValueStrings()` hands back the full list, so a box shows what its
    /// menu says. Vaxateer's own RMS target is the nearest one to ask.
    SWTest::HostSideJuce const juceIsUp;

    SWTest::Instance instance;
    instance.openEditor();

    auto &strip(stripFor(instance.editor(), "Vaxateer"));

    using RMSTarget = Effects::Vaxateer::RMSTarget;
    auto &comboBox(
        comboBoxFor(strip, Parameters::IndexOf<Effects::Vaxateer::Parameters, RMSTarget>::value));

    REQUIRE(comboBox.numberOfItems() == RMSTarget::numberOfDiscreteValues);
    for (unsigned int row(0); row < comboBox.numberOfItems(); ++row)
    {
        INFO("row " << row);
        CHECK(comboBox.getItemShortText(row) == comboBox.getItemText(row));
    }

    CHECK(comboBox.getItemText(RMSTarget::SideRMS) == "Sidechain");
}

////////////////////////////////////////////////////////////////////////////////
///
/// \note Issue #124. Away from the user is *down* the list -- the later value --
/// which is what the same gesture does to a knob, and deliberately not what
/// juce::ComboBox and sst-jucegui's DiscreteParamEditor do. A module strip's box
/// stands in a row of parameter editors with knobs either side of it, and a
/// control that went the other way from the control beside it would be answering
/// a question about lists that the user is not asking.
///
/// \note The value has to reach the parameter and not only the box. A combo box
/// that stepped its own display and published nothing looks exactly right until
/// the sound does not change.
///
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("A wheel over a combo box steps a row and publishes it", "[gui][combo]")
{
    using LE::SW::GlobalParameters::FFTSize;

    SWTest::HostSideJuce const juceIsUp;
    SWTest::Instance instance;
    instance.openEditor();

    auto &box(fftSizeBox(instance.editor()));
    auto &parameters(instance.programMain().parameters());

    auto const opened(box.getValue());
    REQUIRE(opened == parameters.get<FFTSize>());

    // Away from the user: down the list, which for a power of two is the larger.
    scroll(box, +0.3f);
    auto const stepped(box.getValue());
    REQUIRE(stepped != opened);
    CHECK(stepped == opened * 2);
    CHECK(parameters.get<FFTSize>() == stepped);

    // And back, which says the two directions are one gesture rather than two
    // arbitrary ones.
    scroll(box, -0.3f);
    CHECK(box.getValue() == opened);
    CHECK(parameters.get<FFTSize>() == opened);
}

////////////////////////////////////////////////////////////////////////////////
///
/// \note A wheel has no idea where a list begins, so wrapping would turn "keep
/// scrolling" into "start over at the other end" -- which is a value the user
/// never asked for, and for the FFT size an expensive one.
///
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("A wheel stops at the ends of a combo box rather than wrapping", "[gui][combo]")
{
    SWTest::HostSideJuce const juceIsUp;
    SWTest::Instance instance;
    instance.openEditor();

    auto &box(fftSizeBox(instance.editor()));

    auto const rows(box.numberOfItems());
    REQUIRE(rows > 1);

    /// \note Twice the list's length of notches, so it would have wrapped more
    /// than once if it wrapped at all. Which end is which does not matter here
    /// and the case does not say: what it asks is that a wheel held against
    /// either one stays there.
    for (unsigned int notch(0); notch < 2 * rows; ++notch)
        scroll(box, +0.3f);
    auto const oneEnd(box.getValue());

    scroll(box, +0.3f);
    CHECK(box.getValue() == oneEnd);

    for (unsigned int notch(0); notch < 2 * rows; ++notch)
        scroll(box, -0.3f);
    auto const theOther(box.getValue());
    REQUIRE(theOther != oneEnd);

    scroll(box, -0.3f);
    CHECK(box.getValue() == theOther);
}

////////////////////////////////////////////////////////////////////////////////
///
/// \note Not while the list is down. The menu scrolls itself, the box under it
/// is not what the pointer is over, and a selection moving underneath an open
/// list is how a user ends up with a value nobody picked.
///
/// \note The menu never runs *modally* here -- that needs a message loop -- so
/// what this drives is the flag `showMenu()` sets, which is the thing the guard
/// reads. \see GUI::PopupMenu::menuActive().
///
///   Its window is made all the same, though, which is the part that reads as
/// "nothing is shown" and is not: `showMenuAsync()` puts the menu on the desktop
/// before it returns. So this case needs a window server with a window manager
/// behind it like any other, and skips where there is none. \see
/// SWTest::aWindowCanBeMade(), which spells out what Xvfb does instead.
///
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("A wheel does nothing while the combo box's menu is open", "[gui][combo]")
{
    SWTest::HostSideJuce const juceIsUp;

    if (!SWTest::aWindowCanBeMade())
        SKIP(SWTest::noWindow);

    SWTest::Instance instance;
    instance.openEditor();

    auto &box(fftSizeBox(instance.editor()));
    auto const opened(box.getValue());

    static_cast<juce::Component &>(box).mouseDown(juce::MouseEvent(
        juce::Desktop::getInstance().getMainMouseSource(), {}, juce::ModifierKeys(), 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, &box, &box, juce::Time(), {}, juce::Time(), 1, false));
    REQUIRE(box.menuActive());

    scroll(box, -0.3f);
    CHECK(box.getValue() == opened);

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \note Before the editor goes, and this is the one case in the file that
    /// needs saying. The settings panel's box is not a module strip's: its menu
    /// has no parent component, so it is a real desktop window, and leaving it
    /// up outlives the editor rather than the other way about.
    ///
    ////////////////////////////////////////////////////////////////////////////
    juce::PopupMenu::dismissAllActiveMenus();
}
