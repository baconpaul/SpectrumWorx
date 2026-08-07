////////////////////////////////////////////////////////////////////////////////
///
/// wobblerImpl.cpp
/// ---------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "wobblerImpl.hpp"

#include "le/spectrumworx/engine/channelDataAmPh.hpp"
#include "le/spectrumworx/engine/setup.hpp"
#include "le/math/constants.hpp"
#include "le/math/conversion.hpp"
#include "le/math/vector.hpp"

namespace LE::SW::Effects
{

////////////////////////////////////////////////////////////////////////////////
//
// Wobbler static member definitions.
//
////////////////////////////////////////////////////////////////////////////////

char const Wobbler::title[] = "Wobbler";
char const Wobbler::description[] = "Amplitude modulation.";

////////////////////////////////////////////////////////////////////////////////
//
// WobblerImpl::setup()
// --------------------
//
////////////////////////////////////////////////////////////////////////////////

void WobblerImpl::setup(IndexRange const &, Engine::Setup const &engineSetup)
{
    period_ = engineSetup.milliSecondsToSteps(parameters().get<Period>());
}

////////////////////////////////////////////////////////////////////////////////
//
// WobblerImpl::process()
// ----------------------
//
////////////////////////////////////////////////////////////////////////////////

void WobblerImpl::process(ChannelState &cs, Engine::ChannelData_AmPh data,
                          Engine::Setup const &) const
{
    using namespace Math;

    float const amplitude(parameters().get<Amplitude>());
    float const pregain(parameters().get<PreGain>());

    float const gain(
        dB2NormalisedLinear(pregain + amplitude * std::sin(Math::Constants::twoPi *
                                                           convert<float>(cs.frameCounter.value()) /
                                                           convert<float>(period_))));

    cs.frameCounter.nextValueFor(period_);

    multiply(data.amps(), gain);
}

} // namespace LE::SW::Effects
