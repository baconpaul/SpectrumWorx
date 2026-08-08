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

#include "le/math/conversion.hpp"
#include "le/math/math.hpp"

#include <type_traits>

namespace LE::Parameters
{

namespace Detail
{

////////////////////////////////////////////////////////////////////////////////
///
/// \brief \p value as a value of \p Parameter: inside its range, and whole where
/// the parameter is.
///
/// \note Said once, here, rather than in each tag's parse(): what every one of
/// them produces is a number in the parameter's own units, and what every caller
/// needs is one the parameter can actually be set to. A host may type anything,
/// and `Parameter::setValue` answers an out-of-range value with an assertion --
/// in a release build, by storing it.
///
////////////////////////////////////////////////////////////////////////////////

template <class Parameter> float parsedValueForParameter(float const value)
{
    float const clamped(Math::clamp(value, static_cast<float>(Parameter::minimum()),
                                    static_cast<float>(Parameter::maximum())));
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
    return Detail::parsedValueForParameter<Parameter>(*value);
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
