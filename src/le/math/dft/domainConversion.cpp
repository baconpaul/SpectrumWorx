////////////////////////////////////////////////////////////////////////////////
///
/// domainConversion.cpp
/// --------------------
///
/// Copyright (c) 2010 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "domainConversion.hpp"

#include "le/math/vector.hpp"

#include "le/utility/assert.hpp"

namespace LE::Math
{

void reim2AmPh(float const *const reals, float const *const imags, // input
               float *const amplitudes, float *const phases,       // output
               std::uint16_t const numberOfSamples)
{
    LE_ASSERT(reals && imags && amplitudes && phases);

    rectangular2polar(reals, imags, amplitudes, phases, numberOfSamples);
}

void amph2ReIm(float const *const amplitudes, float const *const phases, // input
               float *const reals, float *const imags,                   // output
               std::uint16_t const numberOfSamples)
{
    LE_ASSERT(amplitudes && phases && reals && imags);

    polar2rectangular(amplitudes, phases, reals, imags, numberOfSamples);
}

} // namespace LE::Math
