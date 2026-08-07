////////////////////////////////////////////////////////////////////////////////
///
/// \file shapelessImpl.hpp
/// -----------------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef shapelessImpl_hpp__DBB27280_19FF_4CF5_B1E6_A9CFB52749B7
#define shapelessImpl_hpp__DBB27280_19FF_4CF5_B1E6_A9CFB52749B7
//------------------------------------------------------------------------------
#include "shapeless.hpp"

#include "le/spectrumworx/effects/effects.hpp"

#include <cstdint>

namespace LE::SW::Effects
{

class ShapelessImpl : public EffectImpl<Shapeless>
{
  public: // LE::Effect required interface.
    ////////////////////////////////////////////////////////////////////////////
    // setup() and process()
    ////////////////////////////////////////////////////////////////////////////

    void setup(IndexRange const &, Engine::Setup const &);
    void process(Engine::MainSideChannelData_AmPh, Engine::Setup const &) const;

  private:
    std::uint16_t width_;
}; // class ShapelessImpl

} // namespace LE::SW::Effects

#endif // shapelessImpl_hpp
