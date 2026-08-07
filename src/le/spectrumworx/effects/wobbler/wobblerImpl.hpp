////////////////////////////////////////////////////////////////////////////////
///
/// \file wobblerImpl.hpp
/// ---------------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef wobblerImpl_hpp__A1154887_29E9_43FB_BFA9_4D29C344D513
#define wobblerImpl_hpp__A1154887_29E9_43FB_BFA9_4D29C344D513
//------------------------------------------------------------------------------
#include "wobbler.hpp"

#include "le/spectrumworx/effects/effects.hpp"
#include "le/utility/buffers.hpp"

namespace LE::SW::Effects
{

class WobblerImpl : public EffectImpl<Wobbler>
{
  public: // LE::Effect required interface.
    ////////////////////////////////////////////////////////////////////////
    // ChannelState
    ////////////////////////////////////////////////////////////////////////

    typedef ModuloCounterChannelState ChannelState;

    ////////////////////////////////////////////////////////////////////////////
    // setup() and process()
    ////////////////////////////////////////////////////////////////////////////

    void setup(IndexRange const &, Engine::Setup const &);
    void process(ChannelState &, Engine::ChannelData_AmPh, Engine::Setup const &) const;

  private:
    unsigned int period_;
};

} // namespace LE::SW::Effects

#endif // wobblerImpl_hpp
