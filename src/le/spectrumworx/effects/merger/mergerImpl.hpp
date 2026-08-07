////////////////////////////////////////////////////////////////////////////////
///
/// \file mergerImpl.hpp
/// --------------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef mergerImpl_hpp__6BCD6590_5CD7_40EE_9566_2D273B6A9B1C
#define mergerImpl_hpp__6BCD6590_5CD7_40EE_9566_2D273B6A9B1C
//------------------------------------------------------------------------------
#include "merger.hpp"

#include "le/spectrumworx/effects/effects.hpp"

namespace LE::SW::Effects
{

class MergerImpl : public EffectImpl<Merger>
{
  public: // LE::Effect required interface.
    ////////////////////////////////////////////////////////////////////////////
    // setup() and process()
    ////////////////////////////////////////////////////////////////////////////

    void setup(IndexRange const &, Engine::Setup const &);
    void process(Engine::MainSideChannelData_AmPh, Engine::Setup const &) const;

  private:
    float threshold_;
};

} // namespace LE::SW::Effects

#endif // mergerImpl_hpp
