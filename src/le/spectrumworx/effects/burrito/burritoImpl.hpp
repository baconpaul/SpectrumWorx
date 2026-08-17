////////////////////////////////////////////////////////////////////////////////
///
/// \file burritoImpl.hpp
/// ---------------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef burritoImpl_hpp__C27A00BA_023C_4D1F_9742_F431F45C6018
#define burritoImpl_hpp__C27A00BA_023C_4D1F_9742_F431F45C6018
//------------------------------------------------------------------------------
#include "burrito.hpp"

#include "le/math/math.hpp"
#include "le/spectrumworx/effects/channelStateDynamic.hpp"
#include "le/spectrumworx/effects/effects.hpp"
#include "le/spectrumworx/effects/indexRange.hpp"
#include "le/spectrumworx/engine/buffers.hpp"

namespace LE::SW::Effects
{

class BurritoImpl : public EffectImpl<Burrito>
{
  public: // LE::Effect required interface.
    ////////////////////////////////////////////////////////////////////////////
    // ChannelState
    ////////////////////////////////////////////////////////////////////////////

    struct DynamicChannelState : DynamicChannelState_<DynamicChannelState>
    {
        Engine::HalfFFTBuffer<bool> positions;
        auto members() { return std::tie(positions); }

        /// \note Outside members(): no engine storage, and reset() must not
        /// restart the stream. \see Math::Rng and issue #86 -- which this effect
        /// escaped only because its default period is longer than the test
        /// render and its default mode is a no-op when side == main.
        Math::Rng rng;

        /// \note Reached through the CompoundChannelState below: only this half
        /// declares a seed(), so the name resolves unambiguously.
        void seed(std::uint64_t const seed) { rng.seed(seed); }
    };

    using ChannelState = CompoundChannelState<ModuloCounterChannelState, DynamicChannelState>;

    ////////////////////////////////////////////////////////////////////////////
    // setup() and process()
    ////////////////////////////////////////////////////////////////////////////

    void setup(IndexRange const &, Engine::Setup const &);
    void process(ChannelState &, Engine::MainSideChannelData_AmPh, Engine::Setup const &) const;

  private:
    IndexRange::value_type range_;
    IndexRange::value_type period_;
    float sideGain_;
}; // class BurritoImpl

} // namespace LE::SW::Effects

#endif // burritoImpl_hpp
