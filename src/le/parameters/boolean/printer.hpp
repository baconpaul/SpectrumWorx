////////////////////////////////////////////////////////////////////////////////
///
/// \file printer.hpp
/// -----------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef printer_hpp__D017E02E_44C0_40FE_BC6B_4493C53BF028
#define printer_hpp__D017E02E_44C0_40FE_BC6B_4493C53BF028
//------------------------------------------------------------------------------
#include "tag.hpp"

#include "le/parameters/printer_fwd.hpp"

namespace LE::Parameters::Detail
{
template <class Parameter>
char const *print(bool const parameterValue, SW::Engine::Setup const &, PrintBuffer const &,
                  BooleanParameterTag)
{
    return parameterValue ? "yes" : "no";
}

template <class Parameter>
char const *print(float const &parameterValue, SW::Engine::Setup const &, PrintBuffer const &,
                  BooleanParameterTag)
{
    return Math::round(parameterValue) ? "yes" : "no";
}
} // namespace LE::Parameters::Detail

#endif // printer_hpp
