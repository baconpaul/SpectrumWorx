////////////////////////////////////////////////////////////////////////////////
///
/// \file blenderImpl.hpp
/// ---------------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef blenderImpl_hpp__F2596834_8722_40A2_B5AB_B2E91D94182A
#define blenderImpl_hpp__F2596834_8722_40A2_B5AB_B2E91D94182A
//------------------------------------------------------------------------------
#include "blender.hpp"

#include "le/spectrumworx/effects/effects.hpp"
#include "le/spectrumworx/engine/buffers.hpp"

namespace LE::SW::Effects
{

class BlenderImpl : public EffectImpl<Blender>
{
  public: // LE::Effect interface.
    //////////////////////////////////////////////////////////////////////////////
    // setup() and process()
    //////////////////////////////////////////////////////////////////////////////

    void setup(IndexRange const &, Engine::Setup const &);
    void process(Engine::MainSideChannelData_ReIm, Engine::Setup const &) const;

  private:
    float amount_;
};

} // namespace LE::SW::Effects

#endif // blenderImpl_hpp
