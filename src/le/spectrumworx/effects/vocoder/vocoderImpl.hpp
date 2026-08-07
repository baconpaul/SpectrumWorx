////////////////////////////////////////////////////////////////////////////////
///
/// \file talkingWindImpl.hpp
/// -------------------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef vocoderImpl_hpp__A222DFB9_9541_4031_833C_7D332A621211
#define vocoderImpl_hpp__A222DFB9_9541_4031_833C_7D332A621211
//------------------------------------------------------------------------------
#include "vocoder.hpp"

#include "le/spectrumworx/effects/effects.hpp"
#include "le/parameters/uiElements.hpp" // the UIElements below

#include <cstdint>

namespace LE::SW::Effects
{

class VocoderImpl : public EffectImpl<Vocoder>
{
  public: // LE::Effect required interface.
    ////////////////////////////////////////////////////////////////////////////
    // setup() and process()
    ////////////////////////////////////////////////////////////////////////////

    void setup(IndexRange const &, Engine::Setup const &);
    void process(Engine::MainSideChannelData_AmPh, Engine::Setup const &) const;

  private:
    void lowPassSpectrum_cepstrum(DataRange const &spectrum, DataRange const &workBuffer,
                                  Engine::Setup const &) const;
    void lowPassSpectrum_movingAverage(DataRange const &spectrum, DataRange const &workBuffer,
                                       Engine::Setup const &) const;

    FilterMethod::value_type filterMethod() const { return parameters().get<FilterMethod>(); }

  private:
    std::uint16_t cutoff_;
    std::uint16_t filterLength_; // moving average specific
}; // class VocoderImpl

////////////////////////////////////////////////////////////////////////////////
//
// Vocoder UIElements definitions.
//
////////////////////////////////////////////////////////////////////////////////

EFFECT_PARAMETER_NAME(Vocoder ::EnvelopeBorder, "Envelope border")
EFFECT_PARAMETER_NAME(Vocoder ::NoiseIntensity, "Carrier noise")
EFFECT_PARAMETER_NAME(VocoderImpl::FilterMethod, "Filter method")

EFFECT_ENUMERATED_PARAMETER_STRINGS(VocoderImpl, FilterMethod,
    {CepstrumUdoBrick, "Cepstrum - Brick - Udo"},
    {CepstrumBrick, "Cepstrum - Brick"},
    {CepstrumHamming, "Cepstrum - Hamming"},
    {MovingAverage, "Moving average"},
    {Envelope, "Envelope"},
    {MelEnvelope, "Mel envelope"},
    {Passthrough, "Passthrough"})

} // namespace LE::SW::Effects

#endif // vocoderImpl_hpp
