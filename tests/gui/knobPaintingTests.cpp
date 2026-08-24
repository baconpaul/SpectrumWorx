////////////////////////////////////////////////////////////////////////////////
///
/// knobPaintingTests.cpp
/// ---------------------
///
///   Both knobs are drawn rather than blitted as of 14/15.08.2026, which turned
/// five film strips into two namespaces of numbers.
///
///   How they *look* is checked by looking: `sw-show-ui knobs` is a contact
/// sheet of both knobs across their whole travel, in both polarities, both
/// sizes, selected and not, and reading it takes a second. That is the
/// right instrument for a drawing, and it is the one to reach for when
/// ModuleKnobStyle or EditorKnobStyle is touched.
///
///   So this file is deliberately small. It holds the two things an eye is bad
/// at -- noticing that something it can see *should not be there*, and noticing
/// nothing at all when a binding has quietly died -- and nothing else.
///
/// \note **Nothing here may name a colour, a radius or a stop.** The point of the
/// two style namespaces is that those are adjustable; a case that pinned one
/// would fail the first time somebody made the wedge red, which is a decision
/// rather than a regression. Same rule as skinTests.cpp, for the same reason.
/// Every case below compares two renders of the same painter, so a change of
/// palette moves both sides together and the case stays true.
///
/// \note A fuller set lived here while the port was being verified: which
/// 10-degree sectors the wedge grew into, that it opened from the left stop when
/// unipolar and from twelve o'clock when bipolar, that the halo changed nothing
/// inside the face, and the pointer's bearing recovered from the render by
/// high-passing an angular profile. All of it passed, and all of it was
/// scaffolding for one change -- measuring by machine what the contact sheet
/// says at a glance. It is in the history if a knob ever needs re-fitting.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "gui/gui.hpp"
#include "gui/modules/moduleUI.hpp"

#include <juce_gui_basics/juce_gui_basics.h>

#include <catch2/catch_test_macros.hpp>
//------------------------------------------------------------------------------
namespace
{
//------------------------------------------------------------------------------

namespace GUI = LE::SW::GUI;

/// The knob is painted at its skin size into an image of its own, on a flat
/// ground no part of the drawing uses, so "this pixel was painted" has an answer.
constexpr int size{51};

template <typename Painter> juce::Image render(Painter &&paint)
{
    juce::Image image(juce::Image::ARGB, size, size, true, juce::SoftwareImageType{});
    juce::Graphics graphics(image);
    graphics.fillAll(juce::Colour(0xFF204060));
    paint(graphics, juce::Rectangle<float>(0, 0, size, size));
    return image;
}

juce::Image moduleKnob(float const value, bool const bipolar,
                       GUI::Highlight const highlight = GUI::Highlight::None,
                       std::optional<juce::Range<float>> const lfoRange = std::nullopt)
{
    return render([&](juce::Graphics &graphics, juce::Rectangle<float> const bounds) {
        GUI::paintModuleKnob(graphics, bounds, value, bipolar, highlight, lfoRange);
    });
}

juce::Image editorKnob(float const value)
{
    return render([&](juce::Graphics &graphics, juce::Rectangle<float> const bounds) {
        GUI::paintEditorKnob(graphics, bounds, value);
    });
}

bool differ(juce::Image const &a, juce::Image const &b)
{
    for (int y(0); y < size; ++y)
        for (int x(0); x < size; ++x)
            if (a.getPixelAt(x, y) != b.getPixelAt(x, y))
                return true;
    return false;
}

//------------------------------------------------------------------------------
} // anonymous namespace
//------------------------------------------------------------------------------

TEST_CASE("Both knobs draw the value they are given", "[gui][knob]")
{
    juce::ScopedJuceInitialiser_GUI const juceInitialiser;

    //   The binding, and only the binding: a painter that ignored its argument
    // would draw a knob that looks perfectly correct in a screenshot and never
    // moves in the editor.
    CHECK(differ(moduleKnob(0.0f, false), moduleKnob(1.0f, false)));
    CHECK(differ(moduleKnob(0.0f, true), moduleKnob(1.0f, true)));
    CHECK(differ(editorKnob(0.0f), editorKnob(1.0f)));

    // ...and the polarity is a parameter rather than a decoration: at the centre
    // of the range a unipolar wedge is half open and a bipolar one is shut.
    CHECK(differ(moduleKnob(0.5f, false), moduleKnob(0.5f, true)));
}

TEST_CASE("A module knob draws its three highlights differently", "[gui][knob]")
{
    juce::ScopedJuceInitialiser_GUI const juceInitialiser;

    /// \note Three renders and three inequalities rather than a colour: what
    /// would go wrong here is a hover drawn at the selection's strength, or at
    /// none at all, and either reads as "the pointer does nothing". \see issue
    /// #210.
    auto const plain(moduleKnob(0.5f, false, GUI::Highlight::None));
    auto const hovered(moduleKnob(0.5f, false, GUI::Highlight::Hovered));
    auto const selected(moduleKnob(0.5f, false, GUI::Highlight::Selected));

    CHECK(differ(plain, hovered));
    CHECK(differ(plain, selected));
    CHECK(differ(hovered, selected));
}

TEST_CASE("An LFO range is drawn, and only where there is one", "[gui][knob][lfo]")
{
    juce::ScopedJuceInitialiser_GUI const juceInitialiser;

    ///   The band the knob wears instead of following the sweep, and the binding
    /// to the bounds it stands for. \see Preferences::showLFOAnimation().
    auto const none(moduleKnob(0.5f, false));
    CHECK(differ(none, moduleKnob(0.5f, false, GUI::Highlight::None, {{0.1f, 0.9f}})));

    // ...and it is the *bounds* that are drawn, not merely the fact of them.
    CHECK(differ(moduleKnob(0.5f, false, GUI::Highlight::None, {{0.1f, 0.9f}}),
                 moduleKnob(0.5f, false, GUI::Highlight::None, {{0.6f, 0.9f}})));

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \note **And the value is not drawn beside it.** The band replaces the
    /// wedge rather than joining it: an LFO's bounds are absolute over the
    /// parameter's range rather than an excursion around where it was left, so
    /// the value under a running LFO is not where the parameter is. Drawn as well
    /// it was both a wrong answer and, being opaque, the thing covering the right
    /// one.
    ///
    ///   Two values and one band, which is exact where a pixel count is not: with
    /// the wedge still drawn under the band the two rendered differently, and
    /// with it drawn over them they differed along the arc they shared -- so
    /// "something changed" passed either way while nothing useful was visible.
    ///
    ////////////////////////////////////////////////////////////////////////////
    CHECK_FALSE(differ(moduleKnob(0.0f, false, GUI::Highlight::None, {{0.1f, 0.9f}}),
                       moduleKnob(1.0f, false, GUI::Highlight::None, {{0.1f, 0.9f}})));
}

TEST_CASE("Both knobs sweep through the same arc", "[gui][knob]")
{
    //   Not a measurement, a statement: KnobPainter holds the arc so the two
    // cannot drift apart, and both film strips were fitted to it independently.
    STATIC_CHECK(GUI::KnobPainter::angleFor(0.0f) == -GUI::KnobPainter::halfSweepDegrees);
    STATIC_CHECK(GUI::KnobPainter::angleFor(0.5f) == 0.0f);
    STATIC_CHECK(GUI::KnobPainter::angleFor(1.0f) == GUI::KnobPainter::halfSweepDegrees);
}
