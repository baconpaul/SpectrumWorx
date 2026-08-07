////////////////////////////////////////////////////////////////////////////////
///
/// \file smootherImpl.hpp
/// ----------------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef smootherImpl_hpp__17DDFA09_71A5_4CDD_9A50_B60BB89DD53B
#define smootherImpl_hpp__17DDFA09_71A5_4CDD_9A50_B60BB89DD53B
//------------------------------------------------------------------------------
#include "smoother.hpp"

#include "le/spectrumworx/effects/effects.hpp"

namespace LE::SW::Effects
{

class SmootherImpl : public EffectImpl<Smoother>
{
  public: // LE::Effect interface.
    ////////////////////////////////////////////////////////////////////////////
    // setup() and process()
    ////////////////////////////////////////////////////////////////////////////

    void setup(IndexRange const &, Engine::Setup const &);
    void process(Engine::ChannelData_AmPh, Engine::Setup const &) const;

  private:
    unsigned int filterLenHalf_;
};

} // namespace LE::SW::Effects

#endif // smootherImpl_hpp
