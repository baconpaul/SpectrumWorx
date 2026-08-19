////////////////////////////////////////////////////////////////////////////////
///
/// \file glyphPainter.cpp
/// ----------------------
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "gui/painters/glyphPainter.hpp"

namespace LE::SW::GUI
{

namespace
{
using namespace GlyphStyle;

/// \brief \p ink centred in \p bounds, which is where every mark here is drawn.
juce::Rectangle<float> centred(juce::Rectangle<float> const bounds, float const width,
                               float const height)
{
    return juce::Rectangle<float>(width, height).withCentre(bounds.getCentre());
}
} // anonymous namespace

////////////////////////////////////////////////////////////////////////////////
//
// GlyphPainter::paintFolderUp()
// -----------------------------
//
////////////////////////////////////////////////////////////////////////////////
///
/// \note One stroked path for the stem and the foot, with a curved joint where
/// they meet, and a filled triangle on top of it. The stem's top end is butted
/// rather than rounded because it does not end there -- the arrowhead's base
/// sits on it, and a round cap would push a bulge out past that base.
///
////////////////////////////////////////////////////////////////////////////////

void GlyphPainter::paintFolderUp(juce::Graphics &graphics, juce::Rectangle<float> const bounds,
                                 juce::Colour const colour)
{
    auto const ink(centred(bounds, upWidth, upHeight));

    /// The stem is centred under the arrowhead, so the head's half width is
    /// also how far in from the left the whole vertical run stands.
    auto const stemX(ink.getX() + upHeadWidth / 2);
    auto const headBase(ink.getY() + upHeadHeight);

    juce::Path bend;
    bend.startNewSubPath(ink.getRight(), ink.getBottom() - upStroke / 2);
    bend.lineTo(stemX, ink.getBottom() - upStroke / 2);
    bend.lineTo(stemX, headBase);

    graphics.setColour(colour);
    graphics.strokePath(bend, juce::PathStrokeType(upStroke, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::butt));

    juce::Path head;
    head.startNewSubPath(stemX, ink.getY());
    head.lineTo(stemX + upHeadWidth / 2, headBase);
    head.lineTo(stemX - upHeadWidth / 2, headBase);
    head.closeSubPath();
    graphics.fillPath(head);
}

void GlyphPainter::paintUser(juce::Graphics &graphics, juce::Rectangle<float> const bounds,
                             juce::Colour const colour)
{
    auto const ink(centred(bounds, userBodyWidth, userHeadDiameter + userGap + userBodyHeight));

    graphics.setColour(colour);
    graphics.fillEllipse(ink.getCentreX() - userHeadDiameter / 2, ink.getY(), userHeadDiameter,
                         userHeadDiameter);
    graphics.fillRoundedRectangle(ink.getX(), ink.getBottom() - userBodyHeight, userBodyWidth,
                                  userBodyHeight, userBodyRadius);
}

void GlyphPainter::paintJog(juce::Graphics &graphics, juce::Rectangle<float> const bounds,
                            bool const pointsRight, juce::Colour const colour)
{
    auto const ink(centred(bounds, jogTriangle, jogTriangle));

    auto const apex(pointsRight ? ink.getRight() : ink.getX());
    auto const base(pointsRight ? ink.getX() : ink.getRight());

    juce::Path triangle;
    triangle.startNewSubPath(apex, ink.getCentreY());
    triangle.lineTo(base, ink.getY());
    triangle.lineTo(base, ink.getBottom());
    triangle.closeSubPath();

    graphics.setColour(colour);
    graphics.fillPath(triangle);
}

////////////////////////////////////////////////////////////////////////////////
//
// GlyphPainter::paintFolder()
// ---------------------------
//
////////////////////////////////////////////////////////////////////////////////
///
/// \note Body and tab as one path rather than two overlapping rounded
/// rectangles: they are drawn in one colour at one alpha, and two shapes sharing
/// an edge antialias against the ground twice over -- which shows as a seam
/// down a mark nine pixels tall.
///
////////////////////////////////////////////////////////////////////////////////

void GlyphPainter::paintFolder(juce::Graphics &graphics, juce::Rectangle<float> const bounds,
                               juce::Colour const colour)
{
    auto const ink(centred(bounds, folderWidth, folderHeight));

    juce::Path folder;
    folder.addRoundedRectangle(ink.getX(), ink.getY(), folderTabWidth,
                               folderTabRise + 2 * folderCornerRadius, folderCornerRadius,
                               folderCornerRadius, true /*top left*/, true /*top right*/,
                               false /*bottom left*/, false /*bottom right*/);
    folder.addRoundedRectangle(ink.getX(), ink.getY() + folderTabRise, folderWidth,
                               folderHeight - folderTabRise, folderCornerRadius);

    graphics.setColour(colour);
    graphics.fillPath(folder);
}

void GlyphPainter::paintLock(juce::Graphics &graphics, juce::Rectangle<float> const bounds,
                             juce::Colour const colour)
{
    auto const ink(centred(bounds, lockWidth, lockHeight));
    auto const bodyTop(ink.getBottom() - lockBodyHeight);

    /// The shackle's wire is stroked, so its path is the line down the middle of
    /// it: half a stroke inside the ink at the top, and a radius that is half
    /// the span between the two legs.
    auto const radius(lockShackleWidth / 2);
    auto const arcCentreY(ink.getY() + lockShackleStroke / 2 + radius);

    juce::Path shackle;
    shackle.startNewSubPath(ink.getCentreX() - radius, bodyTop);
    shackle.lineTo(ink.getCentreX() - radius, arcCentreY);
    shackle.addCentredArc(ink.getCentreX(), arcCentreY, radius, radius, 0.0f,
                          -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi);
    shackle.lineTo(ink.getCentreX() + radius, bodyTop);

    graphics.setColour(colour);
    graphics.strokePath(shackle, juce::PathStrokeType(lockShackleStroke));
    graphics.fillRoundedRectangle(ink.getX(), bodyTop, lockWidth, lockBodyHeight, lockCornerRadius);
}

} // namespace LE::SW::GUI
