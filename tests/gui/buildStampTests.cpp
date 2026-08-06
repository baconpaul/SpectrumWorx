////////////////////////////////////////////////////////////////////////////////
///
/// buildStampTests.cpp
/// -------------------
///
///   The strip along the editor's bottom edge that says when this binary was
/// built and from what commit.
///
///   It exists to answer one question -- "is the plugin the host just loaded the
/// one I built a minute ago" -- and it can only answer it if three things hold:
/// that the editor is actually taller than its artwork, that the strip is
/// *painted* rather than merely reserved, and that what it says is a build stamp
/// rather than an empty string. A bar showing nothing looks exactly like a bar
/// showing a stale date to everything except an eye, so each is a case here.
///
/// \note What no test in this process can check is that the stamp is *fresh*:
/// the strings were compiled in, and a build that failed to regenerate them
/// would produce a binary whose stamp is self-consistently wrong. That property
/// belongs to the build and is arranged for in src/buildStamp.cmake, where the
/// custom target that rewrites the source is documented.
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

#include "configuration/buildStamp.hpp"

#include <juce_gui_basics/juce_gui_basics.h>

#include <catch2/catch_test_macros.hpp>

#include <set>
//------------------------------------------------------------------------------
namespace
{
using namespace LE;
using namespace LE::SW;

using Editor = GUI::SpectrumWorxEditor;

/// The strip under the artwork, in an editor of \p width.
juce::Rectangle<int> buildStampBar(int const width)
{
    return {0, Editor::artworkHeight, width, Editor::buildStampHeight};
}

juce::Image rendered(Editor &editor)
{
    juce::Image image(juce::Image::ARGB, editor.getWidth(), editor.getHeight(), true);
    juce::Graphics graphics(image);
    editor.paintEntireComponent(graphics, true);
    return image;
}

std::set<juce::uint32> coloursOver(juce::Image const &image, juce::Rectangle<int> const &area)
{
    REQUIRE(image.getBounds().contains(area));

    juce::Image::BitmapData const pixels(image, juce::Image::BitmapData::readOnly);

    std::set<juce::uint32> colours;
    for (int y(area.getY()); y < area.getBottom(); ++y)
        for (int x(area.getX()); x < area.getRight(); ++x)
            colours.insert(pixels.getPixelColour(x, y).getARGB());
    return colours;
}
} // anonymous namespace

TEST_CASE("The editor is its artwork plus a build-stamp bar", "[gui][buildstamp]")
{
    SWTest::HostSideJuce const juce;
    SWTest::Instance instance;

    /// \note The overlay arrangement, so that the width is the skin's own and the
    /// case is about the height alone.
    instance.openEditor(Editor::PanelPlacement::overlay);
    auto &editor(instance.editor());

    // The skin still decides the artwork, and the constants still agree with it.
    auto const &background(GUI::resourceBitmap<GUI::EditorBackground>());
    CHECK(background.getWidth() == Editor::estimatedWidth);
    CHECK(background.getHeight() == Editor::artworkHeight);

    CHECK(editor.getWidth() == Editor::estimatedWidth);
    CHECK(editor.getHeight() == Editor::artworkHeight + Editor::buildStampHeight);
    CHECK(editor.getHeight() == Editor::estimatedHeight);

    /// \note And the bar came out of the editor's *bottom*, not out of a panel's
    /// rectangle: overlayY is measured against the artwork, so a panel sits where
    /// it always sat and ends above the artwork's last row.
    CHECK(Editor::overlayY + Editor::overlayHeight <= Editor::artworkHeight);
}

TEST_CASE("The build-stamp bar is painted, and says something", "[gui][buildstamp]")
{
    SWTest::HostSideJuce const juce;
    SWTest::Instance instance;
    instance.openEditor(Editor::PanelPlacement::overlay);
    auto &editor(instance.editor());

    auto const image(rendered(editor));

    /// \note Colours rather than "not transparent". The failure this is for is a
    /// bar that was filled and then never written into, which is opaque, covers
    /// its rectangle, and tells nobody anything. Text on a flat fill is more than
    /// one colour; a flat fill is exactly one.
    auto const colours(coloursOver(image, buildStampBar(editor.getWidth())));
    CHECK(colours.size() > 1);

    // Nothing of the skin is left showing through it either.
    CHECK(coloursOver(image, buildStampBar(editor.getWidth())).count(0x00000000u) == 0);
}

TEST_CASE("The stamp names a date, a time and a commit", "[gui][buildstamp]")
{
    juce::String const date(BuildStamp::date);
    juce::String const time(BuildStamp::time);
    juce::String const commit(BuildStamp::commit);

    /// \note The shapes, not the values: what has actually gone wrong with
    /// generated version strings in this tree is a substitution that did not
    /// happen, which leaves either an empty string or the @NAME@ placeholder.
    CHECK(date.length() == 10);
    CHECK(date[4] == '-');
    CHECK(date[7] == '-');
    CHECK(date.substring(0, 4).getIntValue() >= 2026);

    CHECK(time.length() == 8);
    CHECK(time[2] == ':');
    CHECK(time[5] == ':');

    CHECK(commit.isNotEmpty());
    CHECK_FALSE(commit.contains("@"));

    /// \note Not "== 9": a tree that is not a repository stamps "no-git", which
    /// is a legitimate build and a legitimate thing for the bar to say. What is
    /// never legitimate is an unsubstituted template.
    CHECK((commit.startsWith("no-git") || commit.length() >= 7));

    // ...and all three reach the one line the editor draws.
    auto const line(GUI::SpectrumWorxEditor::buildStampText());
    CHECK(line.contains(date));
    CHECK(line.contains(time));
    CHECK(line.contains(commit));
}
