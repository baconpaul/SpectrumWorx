////////////////////////////////////////////////////////////////////////////////
///
/// \file sumoPitchImpl.hpp
/// -----------------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef sumoPitchImpl_hpp__FD93AFF5_C447_439F_96EC_CED05B70909C
#define sumoPitchImpl_hpp__FD93AFF5_C447_439F_96EC_CED05B70909C
//------------------------------------------------------------------------------
#include "sumoPitch.hpp"

#include "le/spectrumworx/effects/channelStateDynamic.hpp"
#include "le/spectrumworx/effects/effects.hpp"
#include "le/spectrumworx/effects/phase_vocoder/shared.hpp"
#include "le/analysis/pitch_detector/pitchDetector.hpp"
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace SW
{
//------------------------------------------------------------------------------
namespace Effects
{
//------------------------------------------------------------------------------

class SumoPitchImpl : public EffectImpl<SumoPitch>
{
  public: // LE::Effect interface.
    ////////////////////////////////////////////////////////////////////////////
    // ChannelState
    ////////////////////////////////////////////////////////////////////////////

    struct DynamicChannelState : DynamicChannelState_<DynamicChannelState>
    {
        PhaseVocoderShared::PitchShifter::ChannelState main;
        PhaseVocoderShared::PitchShifter::ChannelState side;
        auto members() { return std::tie(main, side); }
    };

    struct ChannelState : DynamicChannelState
    {
        float prevPitchScaleMainSemitones;
        float prevPitchScaleSideSemitones;

        PitchDetector::ChannelState pdState;

        void reset();
    };

    ////////////////////////////////////////////////////////////////////////////
    // setup() and process()
    ////////////////////////////////////////////////////////////////////////////

    void setup(IndexRange const &, Engine::Setup const &);
    void process(ChannelState &, Engine::ChannelData_AmPh2ReIm, Engine::Setup const &) const;

  private:
    float amount_;
    float pitchChangeLimitSemitones_;

    PhaseVocoderShared::PitchShifter ps_;
};

//------------------------------------------------------------------------------
} // namespace Effects
//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // sumoPitchImpl_hpp
