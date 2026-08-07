////////////////////////////////////////////////////////////////////////////////
///
/// \file exaggeratorImpl.hpp
/// -------------------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef exaggeratorImpl_hpp__FF94E183_1CB9_4A44_8A1A_47BF21E50306
#define exaggeratorImpl_hpp__FF94E183_1CB9_4A44_8A1A_47BF21E50306
//------------------------------------------------------------------------------
#include "exaggerator.hpp"

#include "le/spectrumworx/effects/effects.hpp"

namespace LE::SW::Effects
{

class ExaggeratorImpl : public EffectImpl<Exaggerator>
{
  public: // LE::Effect required interface.
    ////////////////////////////////////////////////////////////////////////////
    // setup() and process()
    ////////////////////////////////////////////////////////////////////////////

    void setup(IndexRange const &, Engine::Setup const &);
    void process(Engine::ChannelData_AmPh, Engine::Setup const &) const;

  private:
    float exaggerate_;
}; // class ExaggeratorImpl

} // namespace LE::SW::Effects

#endif // exaggeratorImpl_hpp
