////////////////////////////////////////////////////////////////////////////////
///
/// \file printer_fwd.hpp
/// ---------------------
///
/// Copyright (c) 2011 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef printer_fwd_hpp__8B053D9F_3726_48FF_BDA2_DA8B0D23D422
#define printer_fwd_hpp__8B053D9F_3726_48FF_BDA2_DA8B0D23D422
//------------------------------------------------------------------------------
#include "le/utility/tchar.hpp"

#include "boost/range/iterator_range_core.hpp"
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace SW
{
LE_IMPL_NAMESPACE_BEGIN(Engine)
class Setup;
LE_IMPL_NAMESPACE_END(Engine)
} // namespace SW
//------------------------------------------------------------------------------
namespace Parameters
{
//------------------------------------------------------------------------------

typedef boost::iterator_range<char *> PrintBuffer;

////////////////////////////////////////////////////////////////////////////////
//
// print()
// -------
//
////////////////////////////////////////////////////////////////////////////////
///
/// Converts a passed Parameter's value into a user friendly string
/// representation.
///
/// \throws nothing
///
////////////////////////////////////////////////////////////////////////////////

template <class Parameter, typename Source>
char const *print(Source parameterValue, SW::Engine::Setup const &engineSetup,
                  PrintBuffer const &buffer);

struct ParameterPrinter;
struct Printer;
struct AutomatedParameterPrinter;

//------------------------------------------------------------------------------
} // namespace Parameters
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // printer_fwd_hpp
