////////////////////////////////////////////////////////////////////////////////
///
/// \file vibrato.hpp
/// -----------------
///
/// Copyright (c) 2010 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef vibrato_hpp__0403A3EC_F970_4C57_A875_32284F0B5916
#define vibrato_hpp__0403A3EC_F970_4C57_A875_32284F0B5916
//------------------------------------------------------------------------------
#include "le/spectrumworx/effects/commonParameters.hpp"
#include "le/spectrumworx/effects/effects.hpp"
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace SW
{
//------------------------------------------------------------------------------
namespace Effects
{
//------------------------------------------------------------------------------

class VibratoEffect
{
  public:
    typedef ModuloCounterChannelState ChannelState;

  protected:
    void setup(unsigned int const &periodInMilliseconds, Engine::Setup const &);

    float calculateNewPitch(ChannelState &, CommonParameters::SpringType,
                            unsigned int vibratoDepthInSemitones) const;

  private:
    unsigned int period_;
}; // class VibratoEffect

//------------------------------------------------------------------------------
} // namespace Effects
//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // vibrato_hpp
