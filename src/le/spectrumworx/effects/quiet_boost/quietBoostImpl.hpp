////////////////////////////////////////////////////////////////////////////////
///
/// \file quietBoostimpl.hpp
/// ------------------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef quietBoostImpl_hpp__5E3F3097_A492_4350_8DFC_1E0B575039A0
#define quietBoostImpl_hpp__5E3F3097_A492_4350_8DFC_1E0B575039A0
//------------------------------------------------------------------------------
#include "quietBoost.hpp"

#include "le/spectrumworx/effects/effects.hpp"

namespace LE::SW::Effects
{

class QuietBoostImpl : public EffectImpl<QuietBoost>
{
  public: // LE::Effect interface.
    ////////////////////////////////////////////////////////////////////////////
    // setup() and process()
    ////////////////////////////////////////////////////////////////////////////

    static void setup(IndexRange const &, Engine::Setup const &) {}
    void process(Engine::ChannelData_AmPh, Engine::Setup const &) const;
};

} // namespace LE::SW::Effects

#endif // quietBoostImpl_hpp
