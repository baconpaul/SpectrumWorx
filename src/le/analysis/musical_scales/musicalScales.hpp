////////////////////////////////////////////////////////////////////////////////
///
/// \file musicalScales.hpp
/// -----------------------
///
/// Copyright (c) 2010 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef musicalScales_hpp__95B29F4C_895F_43C1_9919_6770BC2FED62
#define musicalScales_hpp__95B29F4C_895F_43C1_9919_6770BC2FED62
//------------------------------------------------------------------------------
#include "le/utility/platformSpecifics.hpp"

#include <array>
#include <cstdint>

namespace LE::Music
{

////////////////////////////////////////////////////////////////////////////////
///
/// \class Scale
///
////////////////////////////////////////////////////////////////////////////////

class Scale
{
  public:
    using ToneOffsets = std::array<std::uint8_t, 12>;

    Scale();

    float snap2Scale(float freq, std::uint8_t keyIndex) const;

    void tonesUpdated(std::uint8_t snappeTo, std::uint8_t bypassed);

    ToneOffsets &toneOffsets() { return toneOffsets_; }
    ToneOffsets const &toneOffsets() const { return toneOffsets_; }

    std::uint8_t numberOfTones() const { return numberOfTones_; }
    std::uint8_t numberOfBypassed() const { return 0; }

  private:
    ToneOffsets::value_type toneOffset(std::uint8_t index) const;

  private:
    std::uint8_t numberOfTones_;
    std::int8_t targetPitchChangeDirection_;
    float centerTone_;
    mutable float lastPitchScale_;
    ToneOffsets toneOffsets_;
}; // class Scale

} // namespace LE::Music

#endif // musicalScales_hpp
