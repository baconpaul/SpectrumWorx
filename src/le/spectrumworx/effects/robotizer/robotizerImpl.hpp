////////////////////////////////////////////////////////////////////////////////
///
/// \file robotizerImpl.hpp
/// -----------------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef robotizerImpl_hpp__29E16506_821A_4004_B789_C7903123F1B7
#define robotizerImpl_hpp__29E16506_821A_4004_B789_C7903123F1B7
//------------------------------------------------------------------------------
#include "robotizer.hpp"

#include "le/spectrumworx/effects/effects.hpp"

namespace LE::SW::Effects
{

class RobotizerImpl : public NoParametersEffectImpl<Robotizer>
{
  public: // LE::Effect interface.
    ////////////////////////////////////////////////////////////////////////////
    // setup() and process()
    ////////////////////////////////////////////////////////////////////////////

    static void setup(IndexRange const &, Engine::Setup const &) {}
    void process(Engine::ChannelData_AmPh, Engine::Setup const &) const;
};

} // namespace LE::SW::Effects

#endif // robotizer_hpp
