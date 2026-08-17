////////////////////////////////////////////////////////////////////////////////
///
/// \file freqverbImpl.hpp
/// ----------------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef freqverbImpl_hpp__338562C9_17C6_4009_876E_4756164099DC
#define freqverbImpl_hpp__338562C9_17C6_4009_876E_4756164099DC
//------------------------------------------------------------------------------
#include "freqverb.hpp"

#include "le/math/math.hpp"
#include "le/spectrumworx/effects/channelStateDynamic.hpp"
#include "le/spectrumworx/effects/effects.hpp"
#include "le/spectrumworx/effects/phase_vocoder/shared.hpp"
#include "le/spectrumworx/engine/buffers.hpp"

#include <cstdint>

namespace LE::SW::Effects
{

class FreqverbImpl : public EffectImpl<Freqverb>
{
  public: // LE::Effect required interface.
    struct ChannelState : DynamicChannelState_<ChannelState>
    {
        Engine::HalfFFTBuffer<> feedbackSumReals;
        Engine::HalfFFTBuffer<> feedbackSumImags;
        PhaseVocoderShared::PitchShifter::ChannelState ps;
        auto members() { return std::tie(feedbackSumReals, feedbackSumImags, ps); }

        /// \note Outside members(): it owns no engine storage, and reset() must
        /// not restart the stream. The draws were global, which made the echo
        /// depend on the host's block size. \see Math::Rng and issue #86.
        Math::Rng rng;

        void seed(std::uint64_t const seed) { rng.seed(seed); }
    };

    ////////////////////////////////////////////////////////////////////////////
    // setup() and process()
    ////////////////////////////////////////////////////////////////////////////

    void setup(IndexRange const &, Engine::Setup const &);
    void process(ChannelState &, Engine::ChannelData_ReIm, Engine::Setup const &) const;

  private:
    float roomLevel_;
    std::uint16_t noEchoBin_;

    PhaseVocoderShared::PitchShifter ps_;
}; // class FreqverbImpl

} // namespace LE::SW::Effects

#endif // freqverbImpl_hpp
