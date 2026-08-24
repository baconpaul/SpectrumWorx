////////////////////////////////////////////////////////////////////////////////
///
/// \file moduleStripPainter.cpp
/// ----------------------------
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "gui/painters/moduleStripPainter.hpp"

#include "gui/colourMap.hpp"

namespace LE::SW::GUI
{

////////////////////////////////////////////////////////////////////////////////
//
// paintModuleStrip()
// ------------------
//
////////////////////////////////////////////////////////////////////////////////

void paintModuleStrip(juce::Graphics &graphics, juce::Rectangle<float> const bounds,
                      Highlight const highlight)
{
    float const halo(highlight == Highlight::Selected  ? 1.0f
                     : highlight == Highlight::Hovered ? hoverStrength
                                                       : 0.0f);
    FramePainter::paint(graphics, bounds, moduleStripFrame, ColourMap::getColour(ColourMap::Accent),
                        ColourMap::getColour(ColourMap::ModuleBackground), halo);
}

} // namespace LE::SW::GUI
