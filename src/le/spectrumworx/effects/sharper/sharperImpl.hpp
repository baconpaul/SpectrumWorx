////////////////////////////////////////////////////////////////////////////////
///
/// \file sharperImpl.hpp
/// ---------------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef sharperImpl_hpp__F315fE63_42EA_4CF4_AE4D_180C4A01F4DE
#define sharperImpl_hpp__F315fE63_42EA_4CF4_AE4D_180C4A01F4DE
//------------------------------------------------------------------------------
#include "sharper.hpp"

#include "le/spectrumworx/effects/effects.hpp"

namespace LE::SW::Effects
{

class SharperImpl : public EffectImpl<Sharper>
{
  public: // LE::Effect required interface.
    ////////////////////////////////////////////////////////////////////////////
    // setup() and process()
    ////////////////////////////////////////////////////////////////////////////

    void setup(IndexRange const &, Engine::Setup const &);
    void process(Engine::ChannelData_AmPh, Engine::Setup const &) const;

  private:
    unsigned int filterLenHalf_;
    float intensity_;
    float cutoff_;
};

} // namespace LE::SW::Effects

#endif // sharperImpl_hpp
