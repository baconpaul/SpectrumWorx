////////////////////////////////////////////////////////////////////////////////
///
/// \file swappahImpl.hpp
/// ---------------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef swappahImpl_hpp__8B69F4EC_B747_4300_B33F_701BABBE80F9
#define swappahImpl_hpp__8B69F4EC_B747_4300_B33F_701BABBE80F9
//------------------------------------------------------------------------------
#include "swappah.hpp"

#include "le/spectrumworx/effects/effects.hpp"

#include <cstdint>

namespace LE::SW::Effects
{

class SwappahImpl : public EffectImpl<Swappah>
{
  public: // LE::Effect required interface.
    ////////////////////////////////////////////////////////////////////////////
    // setup() and process()
    ////////////////////////////////////////////////////////////////////////////

    void setup(IndexRange const &, Engine::Setup const &);
    void process(Engine::ChannelData_AmPh, Engine::Setup const &) const;

  private:
    /// \brief Reorders spectral bands.
    void swapBands(DataRange const &inBuffer) const; /// \throws nothing

  private:
    std::uint16_t band1_;
    std::uint16_t band2_;
    UnpackedMagPhaseMode mode_;
};

} // namespace LE::SW::Effects

#endif // swappahImpl_hpp
