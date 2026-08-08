////////////////////////////////////////////////////////////////////////////////
///
/// \file enumerated/parser.hpp
/// ---------------------------
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef parser_hpp__4E8D31A0_6C57_49B2_A0D4_7F13B85C296E
#define parser_hpp__4E8D31A0_6C57_49B2_A0D4_7F13B85C296E
//------------------------------------------------------------------------------
#include "tag.hpp"

#include "le/parameters/parser_fwd.hpp"
#include "le/parameters/uiElements.hpp" // DiscreteValues

#include <cstring>

namespace LE::Parameters::Detail
{

////////////////////////////////////////////////////////////////////////////////
///
/// \brief The value whose name this is.
///
/// \note By name only, deliberately: an enumerated parameter prints a word and
/// nothing else, so a number here would be an index into a list the user never
/// saw. The linear overload would happily read one -- EnumeratedParameterTag
/// derives from LinearIntegerParameterTag -- which is why this exact-match
/// overload has to exist rather than be left to the tag hierarchy.
///
////////////////////////////////////////////////////////////////////////////////

template <class Parameter>
ParsedValue parse(char const *const text, SW::Engine::Setup const &, EnumeratedParameterTag)
{
    if (!text)
        return {};

    auto const &strings(DiscreteValues<Parameter>::strings);
    for (std::size_t value(0); value < strings.size(); ++value)
    {
        if (std::strcmp(text, strings[value]) == 0)
            return static_cast<float>(value);
    }
    return {};
}

} // namespace LE::Parameters::Detail

#endif // parser_hpp
