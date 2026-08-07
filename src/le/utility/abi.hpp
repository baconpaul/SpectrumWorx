////////////////////////////////////////////////////////////////////////////////
///
/// \file abi.hpp
/// -------------
///
///   A collection of complier/platform specific ABI defining macros.
///
/// Copyright (c) 2010 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef abi_hpp__10DEE3C0_20E4_4B36_B0A8_B02ADEFD7F38
#define abi_hpp__10DEE3C0_20E4_4B36_B0A8_B02ADEFD7F38
//------------------------------------------------------------------------------
#ifdef __ANDROID__
#include "sys/cdefs.h"
#endif // __ANDROID__
//------------------------------------------------------------------------------

#ifndef __cplusplus
#error LE SDKs are C++ libraries and have to be compiled with a C++ compiler
#endif // __cplusplus

#if defined(_MSC_VER) && !defined(__clang__)

#if _MSC_VER < 1600
#error LE SDKs require MSVC10 SP1 (Visual Studio 2010) or later
#endif // _MSC_VER

#define LE_RESTRICT __restrict

// MSVC has no branch hints; the profile guided optimiser is the substitute.
#define LE_LIKELY(expression) (expression)
#define LE_UNLIKELY(expression) (expression)

#elif defined(__GNUC__)

#if __cplusplus < 201103L
#error LE SDKs require GCC 4.7+ or Clang 3.2+ with enabled C++11 support (-std=c++11)
#endif // __cplusplus

#define LE_RESTRICT __restrict__

#define LE_LIKELY(expression) __builtin_expect(!!(expression), 1)
#define LE_UNLIKELY(expression) __builtin_expect(!!(expression), 0)

#else

#error LE unsupported compiler

#endif

// Byte order, previously taken from boost/detail/endian.hpp. Preprocessor
// macros rather than std::endian because the call sites are #if's.
/// \note LE_BIG_ENDIAN and LE_LITTLE_ENDIAN used to be defined here, from
/// __BYTE_ORDER__. The four sites that consulted them now use std::endian,
/// which does not need every one of them to be a preprocessor branch.
///                                           (28.07.2026.) (SW port)

//------------------------------------------------------------------------------
#endif // abi_hpp
