////////////////////////////////////////////////////////////////////////////////
///
/// phasevolutionImpl.cpp
/// ---------------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "phasevolutionImpl.hpp"

#include "le/math/constants.hpp"
#include "le/math/math.hpp"
#include "le/spectrumworx/engine/channelDataAmPh.hpp"
#include "le/spectrumworx/engine/setup.hpp"

#include "le/utility/assert.hpp"

namespace LE::SW::Effects
{

////////////////////////////////////////////////////////////////////////////////
//
// Phasevolution static member definitions.
//
////////////////////////////////////////////////////////////////////////////////

char const Phasevolution::title[] = "Phasevolution";
char const Phasevolution::description[] = "Accelerated phase change.";

////////////////////////////////////////////////////////////////////////////////
//
// PhasevolutionImpl::setup()
// --------------------------
//
////////////////////////////////////////////////////////////////////////////////

void PhasevolutionImpl::setup(IndexRange const &, Engine::Setup const &engineSetup)
{
    stepTime_ = engineSetup.stepTime();
}

////////////////////////////////////////////////////////////////////////////////
//
// PhasevolutionImpl::process()
// ----------------------------
//
////////////////////////////////////////////////////////////////////////////////

void PhasevolutionImpl::process(ChannelState &cs, Engine::ChannelData_AmPh data,
                                Engine::Setup const &setup) const
{
    using namespace Math;
    using namespace Math::Constants;

    float const phasePeriod(parameters().get<PhasePeriod>()); // / 1000.0f; // seconds
    float const wola(setup.windowOverlappingFactor<float>());

    cs.time_ = modulo(cs.time_ + stepTime_, phasePeriod);
    cs.phaseShift_ = modulo(cs.phaseShift_ + (twoPi * cs.time_ / phasePeriod / wola), twoPi);

    float const phaseShift(cs.phaseShift_);
    for (auto &phase : data.phases())
    {
        phase = modulo(phase + phaseShift, twoPi);
        LE_ASSERT(phase >= -twoPi && phase <= twoPi);
    }
}

} // namespace LE::SW::Effects
