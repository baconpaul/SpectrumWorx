////////////////////////////////////////////////////////////////////////////////
///
/// \file linear/parser.hpp
/// -----------------------
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef parser_hpp__8F41C60D_7B25_4A9E_B3D2_51E0A7C4986F
#define parser_hpp__8F41C60D_7B25_4A9E_B3D2_51E0A7C4986F
//------------------------------------------------------------------------------
#include "tag.hpp"

#include "le/parameters/parser_fwd.hpp"
#include "le/parameters/uiElements.hpp" // DisplayValueTransformer
#include "le/utility/lexicalCast.hpp"

namespace LE::Parameters::Detail
{

////////////////////////////////////////////////////////////////////////////////
///
/// \brief Every parameter whose display is a number: linear float, linear
/// integer, and -- through the tag hierarchy, exactly as printLinear() serves
/// them -- symmetric.
///
/// \note One overload for the two linear tags where print() has two, because the
/// difference between them is how many decimal places are *written*. Reading
/// puts the value back into the parameter's own type, which parse() does for
/// every tag at once.
///
////////////////////////////////////////////////////////////////////////////////

template <class Parameter>
ParsedValue parse(char const *const text, SW::Engine::Setup const &engineSetup,
                  LinearParameterTag const &)
{
    auto const displayValue(Utility::parseNumber(text));
    if (!displayValue)
        return {};
    return DisplayValueTransformer<Parameter>::inverse(static_cast<float>(*displayValue),
                                                       engineSetup);
}

} // namespace LE::Parameters::Detail

#endif // parser_hpp
