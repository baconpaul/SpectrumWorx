////////////////////////////////////////////////////////////////////////////////
///
/// armonizerImpl.cpp
/// -----------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "armonizerImpl.hpp"

#include "le/spectrumworx/engine/setup.hpp"

namespace LE::SW::Effects
{

////////////////////////////////////////////////////////////////////////////////
//
// Armonizer static member definitions.
//
////////////////////////////////////////////////////////////////////////////////

char const Armonizer::title[] = "Armonizer";
char const Armonizer::description[] = "Add harmonics.";

////////////////////////////////////////////////////////////////////////////////
//
// ArmonizerImpl::setup()
// ----------------------
//
////////////////////////////////////////////////////////////////////////////////

void ArmonizerImpl::setup(IndexRange const &, Engine::Setup const &engineSetup)
{
    // Setup pitch shifter:
    PitchShifter::setup(engineSetup);
    PitchShifter::setPitchScaleFromSemitones(parameters().get<Interval>(),
                                             engineSetup.numberOfBins());
}

/// \todo This is just another pitch shifter. Improve it by adding the ability
/// to add more harmonics.
///                                           (24.10.2011.) (Domagoj Saric)

} // namespace LE::SW::Effects
