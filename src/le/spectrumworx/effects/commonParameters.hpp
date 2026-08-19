////////////////////////////////////////////////////////////////////////////////
///
/// \file commonParameters.hpp
/// --------------------------
///
/// \brief Common parameters used by various effects.
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef commonParameters_hpp__15BBAC13_C087_4CED_B108_E5130F26EE10
#define commonParameters_hpp__15BBAC13_C087_4CED_B108_E5130F26EE10
#if defined(_MSC_VER) && !defined(DOXYGEN_ONLY)
#endif // MSVC && !Doxygen
//------------------------------------------------------------------------------
#include "le/parameters/enumerated/parameter.hpp"
#include "le/parameters/uiElements.hpp" // the UIElements below

/// \addtogroup Effects
/// @{

/// \brief SpectrumWorx Effect classes and utilities
namespace LE::SW::Effects
{

/// \addtogroup Effects
/// @{
namespace CommonParameters /// \brief Common parameters used by various effects
{

/// \name Common parameters
/// \brief Common parameters used by various effects
/// @{

/// \typedef Mode
/// \brief Specifies what to operate on.
/// \details
///   - Both      : operate on both magnitudes and phases
///   - Magnitudes: operate only on magnitudes
///   - Phases    : operate only on phases
LE_ENUMERATED_PARAMETER(Mode, Both, Magnitudes, Phases);
/// \typedef SpringType
/// \brief Specifies the direction for vibrato-like effects.
LE_ENUMERATED_PARAMETER(SpringType, Symmetric, Up, Down);

/// @}
} // namespace CommonParameters

/// @}

////////////////////////////////////////////////////////////////////////////////
//
// CommonParameters UIElements definitions.
//
////////////////////////////////////////////////////////////////////////////////

EFFECT_PARAMETER_NAME(CommonParameters::Mode, "Target")
EFFECT_PARAMETER_NAME(CommonParameters::SpringType, "Direction")

EFFECT_ENUMERATED_PARAMETER_STRINGS(CommonParameters, Mode,
    {Both, "Both"},
    {Magnitudes, "Magnitudes"},
    {Phases, "Phases"})

EFFECT_ENUMERATED_PARAMETER_STRINGS(CommonParameters, SpringType,
    {Symmetric, "Symmetric"},
    {Up, "Up"},
    {Down, "Down"})

////////////////////////////////////////////////////////////////////////////////
///
/// \note The two things a target is are listed before the one that is both of
/// them. `Both` is the value zero because it is the default and because it is
/// what every preset and automation lane since 2011 has called zero -- so it
/// stays there, and only the menu reads down in the order a user chooses in.
/// \see Parameters::MenuOrder.
///
////////////////////////////////////////////////////////////////////////////////

EFFECT_ENUMERATED_PARAMETER_MENU_ORDER(CommonParameters, Mode, Magnitudes, Phases, Both)

} // namespace LE::SW::Effects
/// @}

#endif // commonParameters_hpp
