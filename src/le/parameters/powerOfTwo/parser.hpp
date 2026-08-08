////////////////////////////////////////////////////////////////////////////////
///
/// \file powerOfTwo/parser.hpp
/// ---------------------------
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef parser_hpp__A72F5B84_0D96_4C13_8E5A_62B70C9DF418
#define parser_hpp__A72F5B84_0D96_4C13_8E5A_62B70C9DF418
//------------------------------------------------------------------------------
#include "tag.hpp"

#include "le/math/conversion.hpp"
#include "le/math/math.hpp"
#include "le/parameters/linear/parser.hpp"

namespace LE::Parameters::Detail
{

////////////////////////////////////////////////////////////////////////////////
///
/// \brief The nearest power of two to whatever the display says.
///
/// \note Nearest, and not exact, because the display need not be reversible to
/// the last digit: the overlap factor prints as the percentage it overlaps by,
/// at one decimal place, and a factor of 16 comes back out of "93.8%" as 16.13.
/// Rounding to the nearest representable factor is what makes that a round trip;
/// requiring an exact answer would make it a refusal.
///
/// \note This one *does* go through DisplayValueTransformer where print() does
/// not -- the note in plugin2Host.hpp explains that asymmetry, and the overlap
/// factor's own print() specialisation is the exception it describes. The
/// primary template's inverse is the identity, so an FFT size, which prints as
/// itself, still reads back as itself.
///
////////////////////////////////////////////////////////////////////////////////

template <class Parameter>
ParsedValue parse(char const *const text, SW::Engine::Setup const &engineSetup,
                  PowerOfTwoParameterTag)
{
    auto const displayValue(Utility::parseNumber(text));
    if (!displayValue)
        return {};

    float const value(DisplayValueTransformer<Parameter>::inverse(static_cast<float>(*displayValue),
                                                                  engineSetup));

    /// \note Clamped here rather than left to parse()'s own clamp, because
    /// PowerOfTwo::round() has no answer for zero and because rounding a value
    /// already past the maximum can only land further past it.
    if (value <= static_cast<float>(Parameter::minimum()))
        return static_cast<float>(Parameter::minimum());
    if (value >= static_cast<float>(Parameter::maximum()))
        return static_cast<float>(Parameter::maximum());

    return static_cast<float>(Math::PowerOfTwo::round(Math::convert<unsigned int>(value)));
}

} // namespace LE::Parameters::Detail

#endif // parser_hpp
