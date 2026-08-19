////////////////////////////////////////////////////////////////////////////////
///
/// \file glyphPainter.hpp
/// ----------------------
///
///   The small marks that stand for a word: the four in the preset browser's
/// navigation row, and the folder in front of a directory in its list.
///
///   None of these came out of the skin -- there was no navigation row to draw
/// them in. They are issue #44's mock-up, measured off it and divided by the
/// 1.5 the editor is drawn at, so the numbers below are skin pixels and what
/// lands on screen at the resting zoom is the mock-up's own: a nine pixel head
/// on the user, a five pixel body under it.
///
/// \note Colour is the caller's, unlike every other painter here. A glyph in
/// this row *is* its state -- white for off and the accent for on, which is
/// what issue #44 asks for -- so there is nothing for the painter to look up.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef glyphPainter_hpp__5C1A8E37_49B2_4D6F_A0E3_2B7C4F91D6A8
#define glyphPainter_hpp__5C1A8E37_49B2_4D6F_A0E3_2B7C4F91D6A8
//------------------------------------------------------------------------------
#include <juce_graphics/juce_graphics.h>

namespace LE::SW::GUI
{

////////////////////////////////////////////////////////////////////////////////
///
/// \namespace GlyphStyle
///
////////////////////////////////////////////////////////////////////////////////

/// \brief Each mark's ink, in skin pixels, inside the widget that holds it.
///
/// \note The ink is centred in its widget rather than filling it: a glyph is a
/// click target as well as a drawing, and sixteen pixels square is the smaller
/// of those two jobs done. \see GlyphPainter, every entry point of which insets
/// to the size below before drawing.
namespace GlyphStyle
{
/// \name The widgets the marks are drawn in
///
/// \note All one height, which is the row's. The widths differ because the
/// marks do: what they have in common is that each is a little wider than its
/// ink, so that a pointer aimed at a five pixel arrowhead lands on something.
///
/// \note The row itself was already there -- twenty pixels between the bottom
/// of the Save row's frame and the top of the list's -- and held the "Ignore
/// external audio" toggle. \see PresetBrowser's constructor for the four x
/// positions, which sit with every other placement in that panel.
///@{
int constexpr rowTop{58};
int constexpr rowHeight{16};
int constexpr upWidgetWidth{16};
int constexpr userWidgetWidth{17};
int constexpr jogWidgetWidth{11};
///@}

/// \brief The padlock, which is not in that row: it is the editor's, beside the
/// sidechain source. \see BackgroundPainter::sideChainLockBounds().
///@{
int constexpr lockWidgetWidth{14};
int constexpr lockWidgetHeight{16};
///@}

////////////////////////////////////////////////////////////////////////////////
/// \name Up one folder
///
///   A stem with an arrowhead on top of it and a foot turning right at the
/// bottom, which is what the issue drew as
///
///     ^
///     |
///     +--
///
////////////////////////////////////////////////////////////////////////////////
///@{
float constexpr upWidth{10.0f};
float constexpr upHeight{10.7f};
/// The stem and the foot, which are one pen.
float constexpr upStroke{2.4f};
float constexpr upHeadWidth{5.4f};
float constexpr upHeadHeight{4.7f};
///@}

////////////////////////////////////////////////////////////////////////////////
/// \name The user
///
///   A head over a pair of shoulders, and nothing else -- the issue asked for
/// the simplified mark rather than the stylised one.
////////////////////////////////////////////////////////////////////////////////
///@{
float constexpr userHeadDiameter{6.0f};
float constexpr userBodyWidth{9.4f};
float constexpr userBodyHeight{3.4f};
/// \note Not half the height: at half, the shoulders come out a capsule. The
/// mock-up rounds the top two corners hard and leaves the foot nearly square,
/// which is the difference between shoulders and a pill.
float constexpr userBodyRadius{1.4f};
/// The neck, which is the whole of what makes the two shapes read as one.
float constexpr userGap{1.3f};
///@}

////////////////////////////////////////////////////////////////////////////////
/// \name The jog
///
///   Two triangles pointing away from each other, one per button. Each is
/// centred in its own widget and the widgets abut, so what separates them is
/// the pair of insets and nothing else -- which is what makes the two read as
/// one control rather than as two buttons that happen to be near each other.
////////////////////////////////////////////////////////////////////////////////
///@{
float constexpr jogTriangle{9.4f}; ///< as wide as it is tall
///@}

////////////////////////////////////////////////////////////////////////////////
/// \name The folder in front of a directory's name in the list
///
/// \note juce::LookAndFeel::getDefaultFolderImage() until now, which JUCE 8
/// answers with a Drawable, may answer with nothing at all, and draws in
/// whatever its own look and feel says rather than in this skin's accent.
////////////////////////////////////////////////////////////////////////////////
///@{
float constexpr folderWidth{11.0f};
float constexpr folderHeight{9.0f};
/// The tab along the back of it: how much of the width it takes and how far it
/// stands above the body.
///@{
float constexpr folderTabWidth{4.6f};
float constexpr folderTabRise{2.0f};
///@}
float constexpr folderCornerRadius{1.2f};
///@}

////////////////////////////////////////////////////////////////////////////////
/// \name The padlock that says a preset may not bring its own audio file
///
///   A body with a shackle standing on it. Shut and open are the same drawing
/// -- what says which is the colour, as everywhere else in this row.
////////////////////////////////////////////////////////////////////////////////
///@{
float constexpr lockWidth{9.4f};
float constexpr lockHeight{10.7f};
float constexpr lockBodyHeight{6.0f};
/// The shackle, measured down the middle of its wire rather than around the
/// outside of it, because that is what a stroked path is placed by.
///@{
float constexpr lockShackleWidth{5.3f};
float constexpr lockShackleStroke{1.4f};
///@}
float constexpr lockCornerRadius{1.1f};
///@}
} // namespace GlyphStyle

////////////////////////////////////////////////////////////////////////////////
///
/// \class GlyphPainter
///
////////////////////////////////////////////////////////////////////////////////

class GlyphPainter
{
  public:
    /// \brief Up one folder: draws GlyphStyle's arrow centred in \p bounds.
    static void paintFolderUp(juce::Graphics &, juce::Rectangle<float> bounds, juce::Colour);

    /// \brief A head and shoulders, centred in \p bounds.
    static void paintUser(juce::Graphics &, juce::Rectangle<float> bounds, juce::Colour);

    /// \brief One half of the jog, centred in \p bounds.
    static void paintJog(juce::Graphics &, juce::Rectangle<float> bounds, bool pointsRight,
                         juce::Colour);

    /// \brief The mark in front of a directory in the preset list.
    static void paintFolder(juce::Graphics &, juce::Rectangle<float> bounds, juce::Colour);

    /// \brief A padlock, centred in \p bounds.
    static void paintLock(juce::Graphics &, juce::Rectangle<float> bounds, juce::Colour);

  public:
    GlyphPainter() = delete; // a drawing, not an object
}; // class GlyphPainter

} // namespace LE::SW::GUI

#endif // glyphPainter_hpp
