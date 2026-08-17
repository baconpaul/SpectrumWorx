////////////////////////////////////////////////////////////////////////////////
///
/// whispererImpl.cpp
/// -----------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "whispererImpl.hpp"

#include "le/math/constants.hpp"
#include "le/math/math.hpp"
#include "le/spectrumworx/engine/channelDataAmPh.hpp"

namespace LE::SW::Effects
{

////////////////////////////////////////////////////////////////////////////////
//
// Whisperer static member definitions.
//
////////////////////////////////////////////////////////////////////////////////

char const Whisperer::title[] = "Whisperer";
char const Whisperer::description[] = "Whispering sound.";

////////////////////////////////////////////////////////////////////////////////
//
// WhispererImpl::process()
// ------------------------
//
////////////////////////////////////////////////////////////////////////////////

void WhispererImpl::process(ChannelState &cs, Engine::ChannelData_AmPh data,
                            Engine::Setup const &) const
{
    for (auto &phase : data.phases())
    {
        phase = cs.rng.ranged(Math::Constants::twoPi);
    }
}

} // namespace LE::SW::Effects
