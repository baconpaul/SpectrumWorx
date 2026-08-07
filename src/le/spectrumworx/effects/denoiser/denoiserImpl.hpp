////////////////////////////////////////////////////////////////////////////////
///
/// \file denoiserImpl.hpp
/// ----------------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef denoiserImpl_hpp__8FEB141B_4DF4_4E5B_ABDE_D546693193E0
#define denoiserImpl_hpp__8FEB141B_4DF4_4E5B_ABDE_D546693193E0
//------------------------------------------------------------------------------
#include "denoiser.hpp"

#include "le/spectrumworx/effects/effects.hpp"

namespace LE::SW::Effects
{

class DenoiserImpl : public EffectImpl<Denoiser>
{
  public: // LE::Effect required interface.
    ////////////////////////////////////////////////////////////////////////////
    // setup() and process()
    ////////////////////////////////////////////////////////////////////////////

    void setup(IndexRange const &, Engine::Setup const &);
    void process(Engine::MainSideChannelData_AmPh, Engine::Setup const &) const;

  private:
    float factor_;
};

} // namespace LE::SW::Effects

#endif // denoiserImpl_hpp
