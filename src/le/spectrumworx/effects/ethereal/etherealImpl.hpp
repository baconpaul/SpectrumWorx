////////////////////////////////////////////////////////////////////////////////
///
/// \file etherealImpl.hpp
/// ----------------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef etherealImpl_hpp__A78CE259_D3CA_4556_8B65_AAF88162B342
#define etherealImpl_hpp__A78CE259_D3CA_4556_8B65_AAF88162B342
//------------------------------------------------------------------------------
#include "ethereal.hpp"

#include "le/spectrumworx/effects/effects.hpp"

namespace LE::SW::Effects
{

class EtherealImpl : public EffectImpl<Ethereal>
{
  public: // LE::Effect required interface.
    ////////////////////////////////////////////////////////////////////////////
    // setup() and process()
    ////////////////////////////////////////////////////////////////////////////

    void setup(IndexRange const &, Engine::Setup const &);
    void process(Engine::MainSideChannelData_AmPh, Engine::Setup const &) const;

  private:
    float threshold_;
    UnpackedMagPhaseMode mode_;
};

} // namespace LE::SW::Effects

#endif // etherealImpl_hpp
