////////////////////////////////////////////////////////////////////////////////
///
/// \file gainImpl.hpp
/// ------------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef gainImpl_hpp__FD752E55_5DB0_41FF_80CF_BFB7CE1C42E9
#define gainImpl_hpp__FD752E55_5DB0_41FF_80CF_BFB7CE1C42E9
//------------------------------------------------------------------------------
#include "gain.hpp"

#include "le/spectrumworx/effects/effects.hpp"

namespace LE::SW::Effects
{

class GainImpl : public NoParametersEffectImpl<Gain>
{
  public: // LE::Effect interface.
    ////////////////////////////////////////////////////////////////////////////
    // setup() and process()
    ////////////////////////////////////////////////////////////////////////////

    static void setup(IndexRange const &, Engine::Setup const &) {}
    static void process(Engine::ChannelData_AmPh const &, Engine::Setup const &) {}
};

} // namespace LE::SW::Effects

#endif // gainImpl_hpp
