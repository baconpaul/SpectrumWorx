////////////////////////////////////////////////////////////////////////////////
///
/// \file dynamic/parser.hpp
/// ------------------------
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef parser_hpp__B316D5F7_94AC_4E20_A85B_0F7C63E19D2A
#define parser_hpp__B316D5F7_94AC_4E20_A85B_0F7C63E19D2A
//------------------------------------------------------------------------------
#include "tag.hpp"

#include "le/parameters/parser_fwd.hpp"
#include "le/utility/lexicalCast.hpp"

namespace LE::Parameters::Detail
{

////////////////////////////////////////////////////////////////////////////////
///
/// \brief A plain number, as the dynamic printer writes one.
///
/// \note What makes this its own overload rather than the linear one is where
/// the range comes from: a dynamic parameter's minimum() and maximum() are
/// functions of the LFO timing rather than constants, and parse() clamps against
/// whatever they say at the moment of the call. No snapping -- the LFO period
/// scale is snapped to its sync grid by the setter that owns the LFO, and this
/// answers a value rather than setting one.
///
////////////////////////////////////////////////////////////////////////////////

template <class Parameter>
ParsedValue parse(char const *const text, SW::Engine::Setup const &, DynamicRangeParameterTag)
{
    auto const displayValue(Utility::parseNumber(text));
    if (!displayValue)
        return {};
    return static_cast<float>(*displayValue);
}

} // namespace LE::Parameters::Detail

#endif // parser_hpp
