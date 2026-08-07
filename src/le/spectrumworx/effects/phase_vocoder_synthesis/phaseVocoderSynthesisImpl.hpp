////////////////////////////////////////////////////////////////////////////////
///
/// \file phaseVocoderSynthesis.hpp
/// -------------------------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef phaseVocoderSynthesisImpl_hpp__1D255695_1557_4BA1_BCB3_E8C202E852DC
#define phaseVocoderSynthesisImpl_hpp__1D255695_1557_4BA1_BCB3_E8C202E852DC
//------------------------------------------------------------------------------
#include "phaseVocoderSynthesis.hpp"
#include "le/spectrumworx/effects/phase_vocoder/shared.hpp"

namespace LE::SW::Effects
{

class PhaseVocoderSynthesisImpl : public NoParametersEffectImpl<PhaseVocoderSynthesis>
{
  public: // LE::Effect interface.
    ////////////////////////////////////////////////////////////////////////////
    // ChannelState
    ////////////////////////////////////////////////////////////////////////////

    typedef PhaseVocoderShared::SynthesisChannelState ChannelState;

    ////////////////////////////////////////////////////////////////////////////
    // setup() and process()
    ////////////////////////////////////////////////////////////////////////////

    void setup(IndexRange const &, Engine::Setup const &);
    void process(ChannelState &, Engine::ChannelData_AmPh, Engine::Setup const &) const;

  private:
    PhaseVocoderShared::BaseParameters pvParameters_;
};

} // namespace LE::SW::Effects

#endif // phaseVocoderSynthesisImpl_hpp
