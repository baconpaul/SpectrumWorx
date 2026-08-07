////////////////////////////////////////////////////////////////////////////////
///
/// \file phlip.hpp
/// ---------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef phlipImpl_hpp__B76D3C03_8A84_4A56_86EB_8038FA5A77EC
#define phlipImpl_hpp__B76D3C03_8A84_4A56_86EB_8038FA5A77EC
//------------------------------------------------------------------------------
#include "phlip.hpp"

#include "le/spectrumworx/effects/effects.hpp"

namespace LE::SW::Effects
{

class PhlipImpl : public EffectImpl<Phlip>
{
  public: // LE::Effect required interface.
    ////////////////////////////////////////////////////////////////////////////
    // setup() and process()
    ////////////////////////////////////////////////////////////////////////////

    void setup(IndexRange const &, Engine::Setup const &);
    void process(Engine::ChannelData_AmPh, Engine::Setup const &) const;

  private:
    unsigned int step_;
    bool oddEvenAdjustment_;
};

} // namespace LE::SW::Effects

#endif // phlip__hpp
