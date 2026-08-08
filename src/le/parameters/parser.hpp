////////////////////////////////////////////////////////////////////////////////
///
/// \file parser.hpp
/// ----------------
///
///   print() backwards. One file per parameter tag beside the printer that goes
/// the other way, and one entry point -- parse() -- that dispatches on the tag
/// exactly as print() does.
///
/// \note What the printers do not have and this does: parse() answers
/// std::optional. A display transform is not onto -- "off", "", "M3.Wet" are all
/// text no value corresponds to -- and a parser that has to answer *something*
/// answers a wrong value. That is not a hypothetical: the CLAP entry point's
/// first implementation ran strtod over the text and returned the result as if
/// display units were storage units, and clap-validator caught an input gain
/// going 0.001 -> "-60dB" -> -60.0 -> "nandB".
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef parser_hpp__2A6F19B4_D573_4C81_9E60_B48D02F7351C
#define parser_hpp__2A6F19B4_D573_4C81_9E60_B48D02F7351C
//------------------------------------------------------------------------------
#include "parser_fwd.hpp"

#include "boolean/parser.hpp"
#include "dynamic/parser.hpp"
#include "enumerated/parser.hpp"
#include "linear/parser.hpp"
#include "powerOfTwo/parser.hpp"
#include "symmetric/parser.hpp"
#include "trigger/parser.hpp"

/// \note The printer, because the range check below measures its own tolerance
/// by printing the parameter's bounds and reading them back rather than by
/// assuming a number of decimal places. \see displayRounding.
#include "printer.hpp"

#include "le/math/conversion.hpp"
#include "le/math/math.hpp"

#include <array>
#include <cmath>
#include <type_traits>

namespace LE::Parameters
{

namespace Detail
{

////////////////////////////////////////////////////////////////////////////////
///
/// \brief \p value as a value of \p Parameter -- whole where the parameter is --
/// or nothing at all when it is not one: outside the range is **refused**, not
/// clamped.
///
/// \note Refused, because clamping answers a question nobody asked. "What value
/// of this parameter displays as `999 dB`" has no answer when the parameter stops
/// at 20, and a host that is handed 20 has been told the text was understood --
/// so it commits the edit and the user's typo silently becomes a real parameter
/// change. `false` puts the field back where it was, which is what every other
/// unreadable string already does here.
///
/// \note The tolerance is what keeps that from breaking the round trip, and it is
/// the display's own precision rather than an epsilon: a bound is *printed*
/// rounded, so re-reading the print of a maximum can legitimately land just past
/// it. `displayRounding` is that half-step, in the parameter's own units, and it
/// is measured rather than assumed -- see below.
///
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
///
/// \brief How far past \p bound a *correctly* read display of it can land, in the
/// parameter's own units.
///
/// \details Measured rather than assumed, by running the bound through this
/// parameter's own printer and back: an input gain's maximum of 2.0 prints as
/// "6" dB, and "6" dB reads back as 1.9953 -- so 0.0047 outside the range is a
/// number this plugin itself produced and must accept. Anything further out is a
/// number nothing here could have shown.
///
/// \note Per bound, not one tolerance for both. On a dB parameter the two differ
/// by five orders of magnitude -- the same 0.5 dB of print rounding is 0.005 at
/// the top of a 0.001..2 range and 0.00006 at the bottom -- so a single figure is
/// either uselessly lax at one end or wrong at the other.
///
/// \note Two `snprintf`s per typed value. This runs when a user finishes typing
/// into a text field; it is not on any hot path.
///
////////////////////////////////////////////////////////////////////////////////

template <class Parameter>
float displayRounding(float const bound, SW::Engine::Setup const &engineSetup)
{
    std::array<char, 64> buffer{};
    char const *const printed(
        Parameters::print<Parameter>(Math::convert<typename Parameter::value_type>(bound),
                                     engineSetup, PrintBuffer{buffer.data(), buffer.size()}));

    auto const readBack(Detail::parse<Parameter>(printed, engineSetup, typename Parameter::Tag()));
    return readBack ? std::abs(*readBack - bound) : 0;
}

template <class Parameter>
ParsedValue parsedValueForParameter(float const value, SW::Engine::Setup const &engineSetup)
{
    auto const minimum(static_cast<float>(Parameter::minimum()));
    auto const maximum(static_cast<float>(Parameter::maximum()));

    if (value < minimum)
    {
        if (value < (minimum - displayRounding<Parameter>(minimum, engineSetup)))
            return {};
    }
    else if (value > maximum)
    {
        if (value > (maximum + displayRounding<Parameter>(maximum, engineSetup)))
            return {};
    }

    /// \note Inside the tolerance but outside the range is clamped rather than
    /// refused: it is a bound printed rounded and read back, and the value it
    /// means is the bound.
    float const clamped(Math::clamp(value, minimum, maximum));

    if constexpr (std::is_floating_point_v<typename Parameter::value_type>)
        return clamped;
    else
        return static_cast<float>(Math::round(clamped));
}

} // namespace Detail

template <class Parameter>
ParsedValue parse(char const *const text, SW::Engine::Setup const &engineSetup)
{
    auto const value(Detail::parse<Parameter>(text, engineSetup, typename Parameter::Tag()));
    if (!value)
        return {};
    return Detail::parsedValueForParameter<Parameter>(*value, engineSetup);
}

////////////////////////////////////////////////////////////////////////////////
///
/// \struct ParameterValueParser
///
/// \brief The parse() counterpart of AutomatedParameterPrinter: a functor
/// invokeFunctorOnIndexedParameter() can run against a parameter chosen at
/// runtime.
///
/// \note It takes no parameter *object*, where the printer's arms take one and
/// pick their value out of it. Nothing here needs one -- text and a range is the
/// whole of what parsing a value takes -- and not taking one is what keeps this
/// away from the trap the printer documents at length in spectrumWorxCLAP.cpp:
/// the Linear arm's `Parameter parameterValue;` default-constructs a parameter
/// whose dynamic range is found by walking to the LFO that owns it, and a
/// detached temporary has no owner to find.
///
////////////////////////////////////////////////////////////////////////////////

struct ParameterValueParser
{
    using result_type = ParsedValue;

    template <class Parameter> result_type operator()() const
    {
        return Parameters::parse<Parameter>(text, engineSetup);
    }

    char const *text;
    SW::Engine::Setup const &engineSetup;
}; // struct ParameterValueParser

} // namespace LE::Parameters

#endif // parser_hpp
