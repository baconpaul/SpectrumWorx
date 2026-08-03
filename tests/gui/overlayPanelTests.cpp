////////////////////////////////////////////////////////////////////////////////
///
/// overlayPanelTests.cpp
/// ---------------------
///
///   The two panels that share one rectangle: that opening either one paints
/// over it, that each of the settings tabs paints a *page* and not just a tab
/// bar, and that the two are mutually exclusive.
///
///   Written against the bug 6.4 found by looking at a picture. Clicking the
/// SpectrumWorx logo asked the settings panel for tab 3 of three; JUCE clamps an
/// out-of-range index to -1, so the panel opened with no page in it. As a
/// transparent desktop window that was invisible; as an overlay it is a hole in
/// the editor. Nothing headless could see it, because `sw-show-ui --render`
/// asserted an exit code and a panel that paints nothing exits 0 -- which is the
/// week_two.md §2.3 row this file is one half of. `--render` measures the whole
/// canvas now; a 191 x 363 hole in a 563 x 376 editor is 33 % of it and would
/// pass that floor comfortably, so the region is what has to be looked at.
///
/// \note Pixels rather than the component tree, deliberately. "Is the About page
/// a child of the tabbed component" is a question the broken build answered
/// correctly -- the page existed, the tab index did not select it. What was wrong
/// is what was on the screen, so that is what is measured.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "gui/editorHarness.hpp"

/// \note Before anything that names SW::Module: the module chain downcasts a
/// node to it, and this is the header with the complete type.
#include "core/modules/moduleDSPAndGUI.hpp"

#include <juce_gui_basics/juce_gui_basics.h>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
//------------------------------------------------------------------------------
namespace
{
using namespace LE;
using namespace LE::SW;

using Editor = GUI::SpectrumWorxEditor;

/// The rectangle both panels are given by SpectrumWorxEditor::openOverlay().
juce::Rectangle<int> overlayRectangle()
{
    return {Editor::overlayX, Editor::overlayY, Editor::overlayWidth, Editor::overlayHeight};
}

/// \brief The editor as an image, which is what `--render` does and what a user
/// sees.
juce::Image rendered(Editor &editor)
{
    juce::Image image(juce::Image::ARGB, editor.getWidth(), editor.getHeight(), true);
    juce::Graphics graphics(image);
    editor.paintEntireComponent(graphics, true);
    return image;
}

/// What fraction of \p area two renders disagree about.
double differenceOver(juce::Image const &left, juce::Image const &right,
                      juce::Rectangle<int> const &area)
{
    REQUIRE(left.getBounds() == right.getBounds());
    REQUIRE(left.getBounds().contains(area));

    juce::Image::BitmapData const leftPixels(left, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData const rightPixels(right, juce::Image::BitmapData::readOnly);

    std::size_t different{0};
    for (int y(area.getY()); y < area.getBottom(); ++y)
        for (int x(area.getX()); x < area.getRight(); ++x)
            different += (leftPixels.getPixelColour(x, y) != rightPixels.getPixelColour(x, y));

    return double(different) / double(area.getWidth() * area.getHeight());
}

/// \brief The smallest rectangle holding every pixel two renders disagree about
/// outside \p ignore, or an empty one if they agree everywhere outside it.
juce::Rectangle<int> differenceBoundsOutside(juce::Image const &left, juce::Image const &right,
                                             juce::Rectangle<int> const &ignore)
{
    REQUIRE(left.getBounds() == right.getBounds());

    juce::Image::BitmapData const leftPixels(left, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData const rightPixels(right, juce::Image::BitmapData::readOnly);

    juce::Rectangle<int> bounds;
    for (int y(0); y < left.getHeight(); ++y)
        for (int x(0); x < left.getWidth(); ++x)
        {
            if (ignore.contains(x, y))
                continue;
            if (leftPixels.getPixelColour(x, y) == rightPixels.getPixelColour(x, y))
                continue;
            bounds =
                bounds.isEmpty() ? juce::Rectangle<int>(x, y, 1, 1) : bounds.getUnion({x, y, 1, 1});
        }
    return bounds;
}

/// \brief How much of \p area is something other than its commonest colour.
///
/// \see tools/show-ui/main.cpp, which asks the same question of a whole page.
double drawnFractionOver(juce::Image const &image, juce::Rectangle<int> const &area)
{
    juce::Image::BitmapData const pixels(image, juce::Image::BitmapData::readOnly);

    std::map<juce::uint32, std::size_t> histogram;
    for (int y(area.getY()); y < area.getBottom(); ++y)
        for (int x(area.getX()); x < area.getRight(); ++x)
            ++histogram[pixels.getPixelColour(x, y).getARGB()];

    std::size_t modal{0};
    for (auto const &[colour, count] : histogram)
        modal = std::max(modal, count);

    auto const total(std::size_t(area.getWidth()) * std::size_t(area.getHeight()));
    return double(total - modal) / double(total);
}

////////////////////////////////////////////////////////////////////////////////
///
/// \note Measured on 03.08.2026 rather than guessed. A real panel covers 96-99 %
/// of the rectangle; a panel with no page in it covers its 16 px tab bar and
/// nothing else, which measures **2.8 %** -- taken by reverting the 6.4 fix and
/// reading the number off the failure, not by arithmetic on the tab bar's height.
/// Two thirds is a long way below the first and a long way above the second,
/// which is the whole of what this constant has to be.
///
////////////////////////////////////////////////////////////////////////////////

constexpr double leastOfTheRectangleAPanelCovers{0.66};

//------------------------------------------------------------------------------
} // anonymous namespace
//------------------------------------------------------------------------------

TEST_CASE("Opening the preset browser paints over the overlay rectangle", "[gui][overlay]")
{
    SWTest::HostSideJuce const juce;
    SWTest::Instance instance;
    instance.openEditor();
    auto &editor(instance.editor());

    auto const closed(rendered(editor));

    editor.showPresetBrowser(true);
    auto const open(rendered(editor));

    CHECK(differenceOver(closed, open, overlayRectangle()) > leastOfTheRectangleAPanelCovers);
    CHECK(drawnFractionOver(open, overlayRectangle()) > 0);

    // ...and shutting it puts the editor back, so the panel is an overlay rather
    // than something that ate what was under it.
    editor.showPresetBrowser(false);
    CHECK(differenceOver(closed, rendered(editor), overlayRectangle()) == 0);
}

TEST_CASE("Every settings tab paints a page and not just a tab bar", "[gui][overlay]")
{
    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \note The shape of the 6.4 bug, stated. "The panel opened and there was
    /// nothing in it" is, as a fraction of the overlay rectangle, the 16 px tab
    /// bar against the 99 % a real page covers -- so a page that fails to build
    /// or fails to be selected is a large, obvious number here, and was an exit
    /// code of 0 before.
    ///
    ////////////////////////////////////////////////////////////////////////////
    SWTest::HostSideJuce const juce;
    SWTest::Instance instance;
    instance.openEditor();
    auto &editor(instance.editor());

    auto const closed(rendered(editor));

    for (unsigned int page(0); page < Editor::numberOfSettingsPages; ++page)
    {
        CAPTURE(page);
        editor.showSettings(page);
        auto const open(rendered(editor));

        CHECK(differenceOver(closed, open, overlayRectangle()) > leastOfTheRectangleAPanelCovers);
    }
}

TEST_CASE("Clicking the logo opens the About page, not an empty panel", "[gui][overlay]")
{
    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \note The 6.4 bug itself, and it is a case of its own because the index
    /// was never the panel's: `showSettings()` was given `numberOfSettingsPages`
    /// by a mouse handler, JUCE clamped 3-of-three to -1, and the panel opened
    /// with no page selected. Every case above calls `showSettings()` with an
    /// index it chose itself and so can never see it -- checked by putting the
    /// old value back and watching them stay green, which is what sent this case
    /// through `mouseDown` instead.
    ///
    ///   With the old value back this one goes red, and by a margin that says the
    /// measure is the right one: the panel covers **2.8 %** of its rectangle
    /// instead of 99 %. That 2.8 % is the tab bar, drawn over nothing.
    ///                                       (03.08.2026.) (SW port)
    ///
    /// \note Through `juce::Component`, because the override is private on the
    /// editor and public on the base -- access is checked against the static type
    /// of the expression, and a mouse handler being unreachable from the outside
    /// is exactly why this path had no coverage.
    ///
    ////////////////////////////////////////////////////////////////////////////
    SWTest::HostSideJuce const juce;
    SWTest::Instance instance;

    // What the About page looks like, opened the way the settings button opens it.
    instance.openEditor();
    auto const closed(rendered(instance.editor()));
    instance.editor().showSettings(Editor::aboutPageIndex);
    auto const aboutPage(rendered(instance.editor()));

    /// \note A second editor rather than shutting the panel on the first:
    /// `showSettings()` is what the button's handler calls *after* deciding to
    /// open, so it is not a toggle and calling it again leaves the panel up.
    instance.closeEditor();
    instance.openEditor();
    auto &editor(instance.editor());
    REQUIRE(differenceOver(closed, rendered(editor), overlayRectangle()) == 0);

    /// \note (37, 321) is the middle of the logo's hit area, which
    /// `SpectrumWorxEditor::mouseDown` spells as {12, 290, 51, 63}.
    juce::Point<float> const logo(37, 321);
    static_cast<juce::Component &>(editor).mouseDown(juce::MouseEvent(
        juce::Desktop::getInstance().getMainMouseSource(), logo,
        juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        &editor, &editor, juce::Time(), logo, juce::Time(), 1, false));

    auto const clicked(rendered(editor));

    // It opened something...
    CHECK(differenceOver(closed, clicked, overlayRectangle()) > leastOfTheRectangleAPanelCovers);
    // ...and it is the About page rather than a panel with no page in it.
    CHECK(differenceOver(aboutPage, clicked, overlayRectangle()) == 0);
}

TEST_CASE("The two panels are mutually exclusive and land in the same place", "[gui][overlay]")
{
    // `openOverlay()` asserts the invariant; this is what it looks like on
    // screen. Both panels are given one 191 x 363 rectangle because the editor is
    // 563 x 376 and fixed, with no free column -- so "open the other one" has to
    // mean "and shut this one", or they draw on top of each other.
    SWTest::HostSideJuce const juce;
    SWTest::Instance instance;
    instance.openEditor();
    auto &editor(instance.editor());

    editor.showSettings(Editor::interfacePageIndex);
    auto const settingsAlone(rendered(editor));

    // Through the browser, which is the order tools/show-ui's editor-settings
    // page uses and the order a user takes.
    editor.showPresetBrowser(true);
    auto const browser(rendered(editor));
    CHECK(differenceOver(settingsAlone, browser, overlayRectangle()) > 0);

    editor.showSettings(Editor::interfacePageIndex);
    CHECK(differenceOver(settingsAlone, rendered(editor), overlayRectangle()) == 0);

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \note Neither panel paints outside its rectangle -- but swapping them is
    /// not confined to it either, and the difference is the point. 660 pixels of
    /// the 563 x 376 editor change out there, and they are the two buttons that
    /// opened the panels: `showSettings()` un-toggles the presets button and the
    /// other way about, which is what makes the exclusion visible to a user
    /// rather than only true.
    ///
    ///   Stated as "everything that moved is in the left column", because that
    /// is where those buttons are and it needs no constant this file does not
    /// already have. An overlay that leaked past its own edge would land in the
    /// module strips, which are to the right of `overlayX`.
    ///
    ////////////////////////////////////////////////////////////////////////////
    auto const moved(differenceBoundsOutside(settingsAlone, browser, overlayRectangle()));
    CAPTURE(moved.toString().toStdString());

    auto const leftColumn(editor.getLocalBounds().withWidth(Editor::overlayX));
    CHECK(!moved.isEmpty());
    CHECK(leftColumn.contains(moved));
}
