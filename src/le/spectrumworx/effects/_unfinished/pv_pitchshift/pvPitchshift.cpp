////////////////////////////////////////////////////////////////////////////////
///
/// pvPitchshift.cpp
/// ----------------
///
/// Copyright (c) 2009. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "pvPitchshift.hpp"
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace Algorithms
{
//------------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
//
// PVPitchshift static member definitions.
//
////////////////////////////////////////////////////////////////////////////////

char const PVPitchshift::title[] = "Pitch Shifter (pvd)";
char const PVPitchshift::description[] = "Pitch shift.";

////////////////////////////////////////////////////////////////////////////////
//
// PVPitchshift::setup()
// ---------------------
//
////////////////////////////////////////////////////////////////////////////////

void PVPitchshift::setup(EngineSetup const &engineSetup, Parameters const &myParameters)
{
    pitchShiftParameters_.setup(myParameters, engineSetup);
}

////////////////////////////////////////////////////////////////////////////////
//
// PVPitchshift::process()
// -----------------------
//
////////////////////////////////////////////////////////////////////////////////

void PVPitchshift::process(ChannelData_AmPh &data) const
{
    PhaseVocoderShared::pitchShiftAndScale(data.amplitudes.begin(), data.phases.begin(),
                                           pitchShiftParameters_);
}

//------------------------------------------------------------------------------
} // namespace Algorithms
//------------------------------------------------------------------------------
} // namespace LE
