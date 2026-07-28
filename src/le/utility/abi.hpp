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

#define LE_NOTHROW __declspec(nothrow)
#define LE_NOEXCEPT
#define LE_NOALIAS __declspec(noalias)
#define LE_NOTHROWNOALIAS __declspec(nothrow noalias)
#define LE_RESTRICTNOALIAS __declspec(restrict noalias)
#define LE_NOTHROWRESTRICTNOALIAS __declspec(nothrow restrict noalias)
#define LE_CONST_FUNCTION LE_NOALIAS
#define LE_PURE_FUNCTION LE_NOALIAS

#define LE_CDECL __cdecl

#define LE_RESTRICT __restrict

#define LE_DLL_EXPORT __declspec(dllexport)
#define LE_DLL_IMPORT __declspec(dllimport)

#define LE_ASSUME(condition) __assume(condition)

// MSVC has no branch hints; the profile guided optimiser is the substitute.
#define LE_LIKELY(expression) (expression)
#define LE_UNLIKELY(expression) (expression)

#define LE_CURRENT_FUNCTION __FUNCSIG__

#elif defined(__GNUC__)

#if __cplusplus < 201103L
#error LE SDKs require GCC 4.7+ or Clang 3.2+ with enabled C++11 support (-std=c++11)
#endif // __cplusplus

#define LE_NOTHROW __attribute__((nothrow))
#ifdef __clang__
#define LE_NOEXCEPT LE_NOTHROW
#else
#define LE_NOEXCEPT noexcept
#endif             // __clang__
#define LE_NOALIAS //__declspec( noalias )
#define LE_NOTHROWNOALIAS LE_NOTHROW LE_NOALIAS
#define LE_RESTRICTNOALIAS __attribute__((malloc)) LE_NOALIAS
#define LE_NOTHROWRESTRICTNOALIAS __attribute__((nothrow, malloc)) LE_NOALIAS
#define LE_CONST_FUNCTION __attribute__((const)) // http://lwn.net/Articles/285332
#define LE_PURE_FUNCTION __attribute__((pure))

#ifdef __i386__
#define LE_CDECL __attribute__((cdecl))
#else
#define LE_CDECL
#endif // __i386__

#define LE_RESTRICT __restrict__

#define LE_DLL_EXPORT __attribute__((visibility("default")))
#define LE_DLL_IMPORT __attribute__((visibility("default")))

#define LE_ASSUME(condition)                                                                       \
    do                                                                                             \
    {                                                                                              \
        if (!(condition))                                                                          \
            __builtin_unreachable();                                                               \
    } while (0)

#define LE_LIKELY(expression) __builtin_expect(!!(expression), 1)
#define LE_UNLIKELY(expression) __builtin_expect(!!(expression), 0)

#define LE_CURRENT_FUNCTION __PRETTY_FUNCTION__

#else

#error LE unsupported compiler

#endif

// Byte order, previously taken from boost/detail/endian.hpp. Preprocessor
// macros rather than std::endian because the call sites are #if's.
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define LE_BIG_ENDIAN
#else
#define LE_LITTLE_ENDIAN
#endif

// Language feature detection, previously taken from boost/config.hpp.
#if !defined(__cpp_exceptions) && !defined(_CPPUNWIND) && !defined(__EXCEPTIONS)
#define LE_NO_EXCEPTIONS
#endif
#if !defined(__cpp_rtti) && !defined(_CPPRTTI) && !defined(__GXX_RTTI)
#define LE_NO_RTTI
#endif

//------------------------------------------------------------------------------
#endif // abi_hpp
