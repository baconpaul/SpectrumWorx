////////////////////////////////////////////////////////////////////////////////
//
// LittleEndian root ODR and ABI configuration header.
// ---------------------------------------------------
//
// Copyright (c) 2009 - 2016. Little Endian Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
//
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef leConfigurationAndODRHeader_h__C79BE937_90BA_4DF1_9D66_5429633644F6
#define leConfigurationAndODRHeader_h__C79BE937_90BA_4DF1_9D66_5429633644F6
#ifdef _MSC_VER
#pragma once
#endif // _MSC_VER
//------------------------------------------------------------------------------

// Implementation note:
//   Nothing here applies to C, and <cstddef> below is a hard error in it. That
// matters because this header is force-included rather than #included: it
// reaches every translation unit of every target that links sw-dsp, and linking
// a JUCE module adds that module's own sources to the consuming target --
// juce_graphics ships Sheenbidi as C.
//
//   dsp.cmake asks for the flag only on CXX and OBJCXX, via
// $<COMPILE_LANGUAGE:...>. The Ninja and Makefile generators honour that; the
// Visual Studio generator does not, and hands it to the C sources too. Guarding
// here rather than there is the version that cannot be got wrong by a generator:
// a header that is inert in C can be force-included into anything.
//                                        (30.07.2026.) (SW port)
#ifdef __cplusplus
//------------------------------------------------------------------------------

#ifndef LE_CHECKED_BUILD
// By default we use checked builds in all non-release builds.
#ifdef NDEBUG
#define LE_CHECKED_BUILD 0
#else
#define LE_CHECKED_BUILD 1
#endif // NDEBUG
#endif // LE_CHECKED_BUILD

// Include asserts in all "checked" builds.
#undef NDEBUG
#if !LE_CHECKED_BUILD
#define NDEBUG
#ifndef LE_PUBLIC_BUILD
#define LE_PUBLIC_BUILD
#endif // LE_PUBLIC_BUILD
#endif // LE_CHECKED_BUILD

/// \note Automatically build SDK projects with "hidden"/"static"/"anonymous"
/// implementation details in order to enable simultaneous usage of multiple
/// SDKs which internally use the same functionality (which would otherwise
/// cause symbol clashes).
///                                           (06.10.2014.) (Domagoj Saric)
#ifdef LE_SW_SDK_BUILD
#define LE_IMPL_NAMESPACE_BEGIN(namespaceName)                                                     \
    namespace namespaceName                                                                        \
    {                                                                                              \
    namespace                                                                                      \
    {
#define LE_IMPL_NAMESPACE_END(namespaceName)                                                       \
    }                                                                                              \
    }
#else
#define LE_IMPL_NAMESPACE_BEGIN(namespaceName)                                                     \
    namespace namespaceName                                                                        \
    {
#define LE_IMPL_NAMESPACE_END(namespaceName) }
#endif // LE_SW_SDK_BUILD

/// \note A quick way to disable the requriement for
/// LE::Utility::assertionFailed to be defined in auxiliary projects when
/// LE_ENABLE_ASSERT_HANDLER is globally defined in the CMakeLists.txt file.
///                                           (15.11.2013.) (Domagoj Saric)
#ifdef LE_DISABLE_ASSERT_HANDLER
#undef LE_ENABLE_ASSERT_HANDLER
#endif // LE_DISABLE_ASSERT_HANDLER

////////////////////////////////////////////////////////////////////////////////
//
// Operating system specifics.
// ---------------------------
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Microsoft Windows.
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#include "sdkddkver.h"

// Implementation note:
//   There were four #undefs above this block -- of WINVER, _WIN32_WINNT,
// _WIN32_IE and NTDDI_VERSION -- which made the #ifndef below unconditionally
// true. So whatever the build, or sdkddkver.h above, had settled on was thrown
// away and every translation unit was pinned to Vista SP2.
//
//   This header is force-included into all of them, JUCE's among them, and JUCE
// 8 draws through Direct2D: pinned to a 2009 API surface its backend cannot see
// ID2D1DeviceContext3/4, IDWriteFactory4, IDCompositionDevice or the
// DWRITE_GLYPH_IMAGE_FORMATS_* enumerators, and fails to compile in its own
// sources -- a hundred errors, none of them in this project's code.
//
//   Without the #undefs sdkddkver.h has already chosen, and it chooses the
// newest the installed SDK supports. What remains is a floor for the case where
// nothing has: Windows 10, which is what JUCE 8 requires.
//                                        (30.07.2026.) (SW port)
#ifndef _WIN32_WINNT
#define WINVER _WIN32_WINNT_WIN10
#define _WIN32_WINNT _WIN32_WINNT_WIN10
#define _WIN32_IE 0x0A00
#define NTDDI_VERSION NTDDI_WIN10
#endif // _WIN32_WINNT

#ifndef LEB_INCLUDE_FULL_WINDOWS_HEADERS
// Exclude rarely-used stuff from Windows headers.
#define WIN32_LEAN_AND_MEAN

// We use std::min/std::max().
#define NOMINMAX
#endif // LEB_INCLUDE_FULL_WINDOWS_HEADERS
#endif // _WIN32

////////////////////////////////////////////////////////////////////////////////
//
// Build tool specifics.
// ---------------------
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Microsoft Visual C++.
////////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)

#pragma once

// Implementation note:
//   A deficiency in CMake allows only MBCS or UNICODE Visual Studio builds.
// To work around this we use the CMake default (MBCS) and then manually
// undefine the relevant macro here (to get ASCII behaviour). Last tested
// with CMake 2.8.2.
//                                        (21.10.2010.) (Domagoj Saric)
#ifdef _MBCS
#undef _MBCS
#endif // _MBCS

// Implementation note:
//   Was ( _MSC_VER < 1400 ) || ( _MSC_VER > 1900 ) -- VS 2005 through VS 2015,
// which means every compiler newer than 2015 was warned about, once per
// translation unit. A ceiling on a supported compiler version is a guess about
// the future that ages into noise; only the floor is a fact, and the fact is now
// C++20 rather than whatever VS 2005 could manage.
//                                        (30.07.2026.) (SW port)
#if _MSC_VER < 1929
#pragma message(                                                                                   \
    "WARNING: SpectrumWorx needs an MSVC with C++20 support -- 19.29 (VS 2019 16.10) or newer.")
#endif

#if (defined(_M_IX86) && (_M_IX86_FP == 1))
#define LE_HAS_SSE1
#endif

#if (defined(_M_IX86) && (_M_IX86_FP >= 2)) || defined(_M_X64)
#define LE_HAS_SSE1
#define LE_HAS_SSE2
#endif

// Implementation note:
//   Guarded because this header is force-included ahead of everything, ours and
// third-party alike, and the third party sets some of these too: JUCE's Harfbuzz
// unit defines _CRT_SECURE_NO_WARNINGS itself and got a macro-redefinition
// warning for its trouble. Defining a macro someone else also defines is only
// silent when both spell it the same way, which is not a thing to rely on.
//                                        (30.07.2026.) (SW port)
#ifndef _ATL_SECURE_NO_WARNINGS
#define _ATL_SECURE_NO_WARNINGS
#endif
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef _SCL_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS
#endif
#ifndef _CRT_DISABLE_PERFCRIT_LOCKS
#define _CRT_DISABLE_PERFCRIT_LOCKS
#endif

#define __STDC_WANT_SECURE_LIB__ LE_CHECKED_BUILD
#define _CRT_SECURE_CPP_OVERLOAD_SECURE_NAMES LE_CHECKED_BUILD
#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES LE_CHECKED_BUILD
#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES_COUNT LE_CHECKED_BUILD
#define _SECURE_ATL LE_CHECKED_BUILD
#ifndef _ITERATOR_DEBUG_LEVEL
#define _SECURE_SCL LE_CHECKED_BUILD
#define _ITERATOR_DEBUG_LEVEL LE_CHECKED_BUILD
#endif // _ITERATOR_DEBUG_LEVEL
#if defined(DEBUG) || defined(_DEBUG)
#define _HAS_ITERATOR_DEBUGGING LE_CHECKED_BUILD
#if LE_CHECKED_BUILD
#undef _ITERATOR_DEBUG_LEVEL
#define _ITERATOR_DEBUG_LEVEL 2
#endif // LE_CHECKED_BUILD
#endif // DEBUG || _DEBUG

#if !__STDC_WANT_SECURE_LIB__
////////////////////////////////////////////////////////////////////////
//
// Implementation note:
//
//   Certain headers and/or libraries do not properly use the
// __STDC_WANT_SECURE_LIB__ macro and, for example, have hardcoded calls
// to "secure CRT" functions or they only check for the
// __STDC_SECURE_LIB__ (or the deprecated __GOT_SECURE_LIB__) macro and
// expect the secure versions to exist if those macros are defined to a
// sufficiently high version. Unfortunately the __STDC_SECURE_LIB__ can
// not be predefined or undefined as the crtdefs.h header forcibly
// defines it. This section provides a workaround for the mentioned
// problems: forcibly includes crtdefs.h, undefines the two macros and
// then defines several (pseudo) secure CRT function versions as
// required by certain 'broken' headers/libraries.
//                                           (25.02.2009.) (Domagoj)
//
////////////////////////////////////////////////////////////////////////

#include "crtdefs.h"
#undef __STDC_SECURE_LIB__
#undef __GOT_SECURE_LIB__

// Implementation note:
//   MSVC's CRT _wcstok_s_l function is defined even if
// __STDC_SECURE_LIB__ is not and it references the wcstok_s function.
//                                    (15.09.2011.) (Domagoj Saric)
#define wcstok_s(strToken, strDelimit, context) wcstok(strToken, strDelimit)

// <required by Dinkumware STL's <xlocnum> header>
#define sprintf_s _snprintf
// </required by Dinkumware STL's <xlocnum> header>

// <required by ATL>
#include "string.h"
#include "wchar.h"
#define DEFINE_SECURE_MEMFUNCTION(nonSecureName, dataType)                                         \
    __inline errno_t nonSecureName##_s(dataType *const dest, size_t const numberOfElements,        \
                                       dataType const *const src, size_t const count)              \
    {                                                                                              \
        (void)numberOfElements;                                                                    \
        nonSecureName(dest, src, count);                                                           \
        return 0;                                                                                  \
    }

DEFINE_SECURE_MEMFUNCTION(memcpy, void)
DEFINE_SECURE_MEMFUNCTION(memmove, void)
DEFINE_SECURE_MEMFUNCTION(wmemcpy, wchar_t)
DEFINE_SECURE_MEMFUNCTION(wmemmove, wchar_t)

#define DEFINE_SECURE_STRFUNCTION(nonSecureName, dataType)                                         \
    __inline errno_t nonSecureName##_s(dataType *const strDestination,                             \
                                       size_t const numberOfElements,                              \
                                       dataType const *const strSource)                            \
    {                                                                                              \
        (void)numberOfElements;                                                                    \
        nonSecureName(strDestination, strSource);                                                  \
        return 0;                                                                                  \
    }

DEFINE_SECURE_STRFUNCTION(strcpy, char)
DEFINE_SECURE_STRFUNCTION(wcscat, wchar_t)
DEFINE_SECURE_STRFUNCTION(strcat, char)
DEFINE_SECURE_STRFUNCTION(wcscpy, wchar_t)

#define DEFINE_SECURE_STRNFUNCTION DEFINE_SECURE_MEMFUNCTION
DEFINE_SECURE_STRNFUNCTION(strncpy, char)
DEFINE_SECURE_STRNFUNCTION(wcsncpy, wchar_t)

#undef DEFINE_SECURE_STRNFUNCTION
#undef DEFINE_SECURE_STRFUNCTION
#undef DEFINE_SECURE_MEMFUNCTION

#define swprintf_s _snwprintf
// </required by ATL>

#endif // !__STDC_WANT_SECURE_LIB__

//   As we use a lot of heavy template (meta)programing it is actually
// useful to instruct the MSVC++ compiler to be maximally aggressive with
// inlining.
#pragma inline_depth(255)
#pragma inline_recursion(on)

#elif defined(__GNUC__) // compiler

#if defined(__SSE__)
#define LE_HAS_SSE1
// Implementation note:
//   When compiling for the iOS simulator __SSE__ and __SSE2__ get
// defined without __MMX__ which causes compilation errors.
//                                    (28.11.2011.) (Domagoj Saric)
#if !defined(__MMX__)
#define __MMX__
#endif
#endif

#if defined(__SSE2__)
#define LE_HAS_SSE2
#endif

#else

#error Your compiler is not supported by the Little Endian build system.

#endif // compiler

#ifdef __APPLE__
#include <cstddef>
#if !defined(_LIBCPP_VERSION) && (__GLIBCXX__ < 20110325)
namespace std
{
typedef void const *const nullptr_t;
}
#endif // old stdlibc++
#endif // __APPLE__

////////////////////////////////////////////////////////////////////////////////
//
// 3rd party library specifics.
// ----------------------------
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Boost.
////////////////////////////////////////////////////////////////////////////////

/// \note What is left here only reaches Boost.Fusion, Boost.MPL and
/// Boost.Preprocessor, the three libraries stage 2 kept. The Spirit, Karma,
/// Phoenix, TR1, endian, throw_exception and compiler-config tuning that used
/// to live here went with the libraries it configured.
///                                           (28.07.2026.) (SW port)
#define BOOST_NO_IOSTREAM
#ifdef NDEBUG
#define BOOST_NO_TYPEID // implies LE_NO_RTTI
#endif                  // NDEBUG

#ifndef BOOST_EXCEPTION_DISABLE
#define BOOST_EXCEPTION_DISABLE
#endif // BOOST_EXCEPTION_DISABLE

#include <ciso646>

/// \note Import the fixes/workarounds for Boost.Range's lack of restricted
/// pointer support from NT2.
///                                           (11.09.2013.) (Domagoj Saric)
#if defined(LE_HAS_NT2)
#include "boost/dispatch/meta/is_iterator.hpp"
#endif // LE_HAS_NT2

//------------------------------------------------------------------------------
#endif // __cplusplus
//------------------------------------------------------------------------------
#endif // leConfigurationAndODRHeader_h
