////////////////////////////////////////////////////////////////////////////////
///
/// \file windowsLite.hpp
/// ---------------------
///
/// "Least invasive windows.h inclusion".
///
/// Copyright (c) 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef windowsLite_hpp__6F4C2A18_7B90_4E63_9D21_0C5A8E37B142
#define windowsLite_hpp__6F4C2A18_7B90_4E63_9D21_0C5A8E37B142
//------------------------------------------------------------------------------
#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // WIN32_LEAN_AND_MEAN

#include <windows.h>

#undef min
#undef max

#endif // _WIN32
//------------------------------------------------------------------------------
#endif // windowsLite_hpp
//------------------------------------------------------------------------------
