////////////////////////////////////////////////////////////////////////////////
///
/// \file pitchShifter.hpp
/// ----------------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef pitchShifter_hpp__4124BAA3_F146_4A5D_9C9B_BB09EFCE82DB
#define pitchShifter_hpp__4124BAA3_F146_4A5D_9C9B_BB09EFCE82DB
#if defined(_MSC_VER) && !defined(DOXYGEN_ONLY)
#endif // MSVC && !Doxygen
//------------------------------------------------------------------------------
#include "le/spectrumworx/effects/parameters.hpp"
#include "le/parameters/symmetric/parameter.hpp"
#include "le/parameters/uiElements.hpp" // the UIElements below

namespace LE::SW::Effects
{

namespace Detail
{
struct PitchShifterBase
{
#ifdef LE_PV_USE_TSS
    LE_DEFINE_PARAMETER(TSSSensitivity, LinearFloat, Minimum<0>, Default<65>, Maximum<100>,
                        Unit<"%">);
/// The parameter list is a comma separated one now, so a conditional member of
/// it carries its own separator.
#define LE_PV_TSS_SENSITIVITY() , TSSSensitivity
#else
#define LE_PV_TSS_SENSITIVITY()
#endif // LE_PV_USE_TSS

    LE_DEFINE_PARAMETER(SemiTones, SymmetricFloat, MaximumOffset<24>, Unit<"'">);
    LE_DEFINE_PARAMETER(Cents, SymmetricInteger, MaximumOffset<100>, Unit<"''">);
    LE_DEFINE_PARAMETERS(SemiTones, Cents LE_PV_TSS_SENSITIVITY());

#undef LE_PV_TSS_SENSITIVITY

    /// \typedef SemiTones
    /// \brief Specifies the number of semitones to pitch shift.
    /// \typedef Cents
    /// \brief Specifies the number of cents to pitch shift (adds to
    /// semitones).

    static bool const usesSideChannel = false;

    static char const description[];
};
} // namespace Detail

////////////////////////////////////////////////////////////////////////////////
///
/// \class PitchShifter
///
/// \ingroup Effects
///
/// \brief Pitch shift only into the selected band.
///
/// This module is as straight forward as it gets. It shifts the pitch of the
/// incoming signal in semitones and cents. You can go anywhere between two
/// octaves higher and two octaves lower than the original signal.
///
////////////////////////////////////////////////////////////////////////////////

struct PitchShifter : Detail::PitchShifterBase
{
    static char const title[];
};

////////////////////////////////////////////////////////////////////////////////
///
/// \class PVPitchShifter
///
/// \ingroup Effects
///
/// \brief Pitch shift only into the selected band.
///
////////////////////////////////////////////////////////////////////////////////

struct PVPitchShifter : Detail::PitchShifterBase
{
    static char const title[];
};

////////////////////////////////////////////////////////////////////////////////
//
// PitchShifter UIElements definitions.
//
////////////////////////////////////////////////////////////////////////////////

EFFECT_PARAMETER_NAME(Detail::PitchShifterBase::SemiTones, "Semitones")
EFFECT_PARAMETER_NAME(Detail::PitchShifterBase::Cents, "Cents")
/// \note LE_PV_USE_TSS, which is what decides whether the parameter exists at
/// all. It read LE_PV_TSS_DYNAMIC_THRESHOLD while it lived in the .cpp, so
/// defining one of the pair without the other produced a parameter nothing
/// names -- which was a link error then and is a compile error now.
#ifdef LE_PV_USE_TSS
EFFECT_PARAMETER_NAME(Detail::PitchShifterBase::TSSSensitivity, "Transient sensitivity")
#endif // LE_PV_USE_TSS

} // namespace LE::SW::Effects

#endif // pitchShifter_hpp
