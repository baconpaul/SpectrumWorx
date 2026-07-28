////////////////////////////////////////////////////////////////////////////////
///
/// \file domainConversion.hpp
/// --------------------------
///
/// Copyright (c) 2010 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef domainConversion_hpp__8429E54E_2E4F_4552_8B77_DE27CCF355F2
#define domainConversion_hpp__8429E54E_2E4F_4552_8B77_DE27CCF355F2
//------------------------------------------------------------------------------
#include "le/utility/platformSpecifics.hpp"

#include <cstdint>
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
LE_IMPL_NAMESPACE_BEGIN(Math)
//------------------------------------------------------------------------------

void reim2AmPh(float const *reals, float const *imags, // input
               float *amplitudes, float *phases,       // output
               std::uint16_t numberOfSamples);

void amph2ReIm(float const *amplitudes, float const *phases, // input
               float *reals, float *imags,                   // output
               std::uint16_t numberOfSamples);

//------------------------------------------------------------------------------
LE_IMPL_NAMESPACE_END(Math)
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // domainConversion_hpp
