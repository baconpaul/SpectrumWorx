////////////////////////////////////////////////////////////////////////////////
///
/// \file exImploderImpl.hpp
/// ------------------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef exImploderImpl_hpp__26D3CDDD_80CD_4418_806F_BFC41FD88A3D
#define exImploderImpl_hpp__26D3CDDD_80CD_4418_806F_BFC41FD88A3D
//------------------------------------------------------------------------------
#include "exImploder.hpp"

#include "le/spectrumworx/effects/channelStateDynamic.hpp"
#include "le/spectrumworx/effects/effects.hpp"
#include "le/spectrumworx/effects/phase_vocoder/shared.hpp"
#include "le/utility/buffers.hpp"
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

namespace Detail
{
class ExImPloderImpl
{
  public:
    struct DynamicChannelState : DynamicChannelState_<DynamicChannelState>
    {
        Engine::HalfFFTBuffer<float> accumMagn;
        Engine::HalfFFTBuffer<float> accumFreqs;
        auto members() { return std::tie(accumMagn, accumFreqs); }
    };

    struct ChannelState : DynamicChannelState
    {
        void reset();
    };

  protected:
    float setup(int glissando, int threshold, int gate, Engine::Setup const &);

    void process(ChannelState &, Engine::ChannelData_AmPh &, bool thresholdIsLowerBound) const;

  protected:
    void setMagnitudeScale(float magnitudeScale);

  private:
    float gate_;
    float gliss_;
    float nyquist_;
    float threshold_;
    float magnitudeScale_;
};
} // namespace Detail

class PVImploderImpl : public Detail::ExImPloderImpl, public EffectImpl<PVImploder>
{
  public: // LE::Effect required interface.
    ////////////////////////////////////////////////////////////////////////////
    // setup() and process()
    ////////////////////////////////////////////////////////////////////////////

    void setup(IndexRange const &, Engine::Setup const &);
    void process(ChannelState &, Engine::ChannelData_AmPh, Engine::Setup const &) const;
};

using ImploderImpl = PhaseVocoderShared::StandaloneEffect<PVImploderImpl, Imploder>;

class PVExploderImpl : public Detail::ExImPloderImpl, public EffectImpl<PVExploder>
{
  public: // LE::Effect required interface.
    ////////////////////////////////////////////////////////////////////////////
    // setup() and process()
    ////////////////////////////////////////////////////////////////////////////

    void setup(IndexRange const &, Engine::Setup const &);
    void process(ChannelState &, Engine::ChannelData_AmPh, Engine::Setup const &) const;
};

using ExploderImpl = PhaseVocoderShared::StandaloneEffect<PVExploderImpl, Exploder>;

//------------------------------------------------------------------------------
} // namespace Effects
//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
namespace Parameters
{
//------------------------------------------------------------------------------
template <class Parameter> struct DisplayValueTransformer;

template <> struct DisplayValueTransformer<SW::Effects::Detail::ExImPloder::Gate>
{
    template <typename Source>
    static float transform(Source const &value, SW::Engine::Setup const &)
    {
        if (value == SW::Effects::Detail::ExImPloder::Gate::minimum())
            return -std::numeric_limits<float>::infinity();
        return static_cast<float>(value);
    }
    using Suffix = LE::Parameters::UnitString<" dB">;
}; // struct DisplayValueTransformer<SW::Effects::Detail::ExImPloder::Gate>
//------------------------------------------------------------------------------
} // namespace Parameters
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // exImploderImpl_hpp
