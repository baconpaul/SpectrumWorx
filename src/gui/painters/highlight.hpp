////////////////////////////////////////////////////////////////////////////////
///
/// \file highlight.hpp
/// -------------------
///
///   What a control says about the pointer and the selection, and how strongly.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef highlight_hpp__0C7B34E9_18A6_4D52_B7F1_6A29D4E8C305
#define highlight_hpp__0C7B34E9_18A6_4D52_B7F1_6A29D4E8C305
//------------------------------------------------------------------------------

namespace LE::SW::GUI
{

////////////////////////////////////////////////////////////////////////////////
///
/// \brief The three things a module control or a module strip can be.
///
///   Selected and hovered are two questions, not two points on one scale -- a
/// control the pointer is on while another one is selected is both -- but only
/// one of them can be drawn, and the selection is the one that wins. So this is
/// what a widget hands its painter after it has decided. \see issue #210.
///
////////////////////////////////////////////////////////////////////////////////

enum class Highlight
{
    None,
    Hovered, ///< the pointer is on it: whatever Selected draws, at hoverStrength
    Selected ///< and this is the one the LFO strip and the shared controls follow
}; // enum class Highlight

/// \brief What a hover is drawn at, against a selection's full strength.
///
/// \note Half, which is what issue #210 asks for and what makes the two tell
/// apart at a glance rather than on inspection.
float constexpr hoverStrength{0.5f};

} // namespace LE::SW::GUI

//------------------------------------------------------------------------------
#endif // highlight_hpp
