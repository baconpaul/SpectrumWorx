////////////////////////////////////////////////////////////////////////////////
///
/// bandpassImpl.cpp
/// ----------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "bandpassImpl.hpp"

#include "le/spectrumworx/effects/indexRange.hpp"
#include "le/spectrumworx/engine/channelDataAmPh.hpp"
#include "le/spectrumworx/engine/setup.hpp"
#include "le/math/conversion.hpp"
#include "le/math/math.hpp"
#include "le/math/vector.hpp"
#include "le/math/windows.hpp"

namespace LE::SW::Effects
{

////////////////////////////////////////////////////////////////////////////////
//
// Bandpass static member definitions.
//
////////////////////////////////////////////////////////////////////////////////

char const Bandpass::title[] = "Bandpass";
char const Bandpass::description[] = "Band-pass filter.";

////////////////////////////////////////////////////////////////////////////////
//
// Bandstop static member definitions.
//
////////////////////////////////////////////////////////////////////////////////

char const Bandstop::title[] = "Bandstop";
char const Bandstop::description[] = "Band-stop filter.";

////////////////////////////////////////////////////////////////////////////////
//
// Detail::BandGainImpl::setup()
// -----------------------------
//
////////////////////////////////////////////////////////////////////////////////

/// \note A windowed version of both filters stood here, behind
/// LE_BAND_FILTER_USE_ENGINE_WINDOW: an engine window scaled into
/// [1, attenuation] and applied as an up-slope and a down-slope either side of
/// the band, so the edges rolled off instead of stepping. It was marked "Testing
/// phase..." in 2012 and never left it.
///
///   It was not switched off, which is the reason it goes rather than staying as
/// a dormant option. The macro was defined under `#if defined(_DEBUG)` -- MSVC's
/// own debug-runtime macro, set by no build here -- so Clang and GCC never
/// compiled it and MSVC Debug did. Bandpass and Bandstop therefore produced
/// different audio in an MSVC Debug build than in every other build in
/// existence, and the goldens could not see it. Worse, `pUpSlope_` had no
/// initialiser and `BandGainImpl` no constructor, so the first setup() call read
/// it to compute an alignment before anything had written it.
///                                       (07.08.2026.) (SW port)
void Detail::BandGainImpl::setup(IndexRange const &, Engine::Setup const &)
{
    attenuation_ = Math::dB2NormalisedLinear(-parameters().get<Attenuation>());
}

////////////////////////////////////////////////////////////////////////////////
//
// BandpassImpl::process()
// -----------------------
//
////////////////////////////////////////////////////////////////////////////////

void BandpassImpl::process(Engine::ChannelData_AmPh data, Engine::Setup const &) const
{
    using namespace Math;

    multiply(attenuation_, data.full().amps().begin(), data.amps().begin());
    multiply(attenuation_, data.amps().end(), data.full().amps().end());
}

////////////////////////////////////////////////////////////////////////////////
//
// BandstopImpl::process()
// -----------------------
//
////////////////////////////////////////////////////////////////////////////////

void BandstopImpl::process(Engine::ChannelData_AmPh data, Engine::Setup const &) const
{
    Math::multiply(data.amps(), attenuation_);
}

} // namespace LE::SW::Effects
