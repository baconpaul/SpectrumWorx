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
//   This header is force-included rather than #included, because what it sets
// has to be seen before any other header. cmake/sw-our-sources.cmake attaches it
// per source file, to files under src/, tests/ and tools/ and to nothing else;
// tests/checkODRHeaderScope.cmake fails the build if that ever stops being true.
//
//   What it is *for* changed on 04.08.2026 and the difference is worth knowing.
// It used to also define LE_IMPL_NAMESPACE_BEGIN/END, which 47 files used to
// open a namespace without declaring where the macro came from -- so a file that
// missed this header did not fail cleanly, it failed as thirty-odd errors that
// named everything except the cause. Those macros are written out now, and every
// one of our 148 translation units compiles with the force-include removed.
//
//   So what is left is configuration rather than syntax, and it is all the more
// silent for it: the NDEBUG policy below decides whether the ~1200 asserts exist
// at all, and the Windows blocks decide which API surface everything after them
// sees. A translation unit that misses this header now builds -- as a different
// build. That is the thing checkODRHeaderScope.cmake is guarding, and it is a
// worse failure than the one it was written for, not a better one.
//
//   It used to be a PUBLIC compile option on sw-dsp, so it reached every
// translation unit of every target that links sw-dsp -- JUCE, fmt and
// clap-wrapper included. Five separate Windows failures came of that, none of
// them in our code; stage 7.5 of doc/tech/old/implementation_sequence.md lists them.
//
//   Nothing here applies to C, and <cstddef> below is a hard error in it. Now
// that the header only reaches our own sources, none of which are C, this guard
// is belt rather than braces -- kept because the guard that was *supposed* to do
// this job, $<COMPILE_LANGUAGE:CXX>, is silently ignored for compile options by
// the Visual Studio generator, and a header that is inert in C cannot be
// mis-applied by a generator.
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

/// \note LE_IMPL_NAMESPACE_BEGIN/END stood here. Under LE_SW_SDK_BUILD they
/// nested an anonymous namespace inside the named one, so that two SDKs sharing
/// this code could be linked into one binary without their internals clashing;
/// otherwise they were `namespace X {` and `}`. There is no SDK build any more
/// (doc/tech/todo.md's stage 7 settled that macro), so they were 55 obfuscated
/// namespace openings across 47 files -- and 47 files that used them without
/// declaring where they came from, which is the whole reason this header is
/// force-included. Both are written out as of 04.08.2026.
///                                           (06.10.2014.) (Domagoj Saric)

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
// Implementation note:
//   WIN32_LEAN_AND_MEAN was defined here too, and is not any more. It trims
// <windows.h> down, which is a fine thing to ask for in our own translation
// units and not ours to ask for in anybody else's -- and this header is
// force-included into every one of them. Among the headers it excludes is
// shellapi.h, so clap-wrapper's standalone entry point lost CommandLineToArgvW.
//
//   le/utility/windowsLite.hpp defines it for itself, which is where the request
// belongs: in the header that then includes <windows.h>.
//                                        (30.07.2026.) (SW port)

// We use std::min/std::max(). Kept: NOMINMAX only suppresses two macros that
// nothing wants, and the third-party code here defines it for itself anyway.
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
// Implementation note:
//   _CRT_DISABLE_PERFCRIT_LOCKS was defined here. It is not a hint: MSVC's
// <stdio.h> reads it and rewrites the standard names as macros --
//
//     #define fwrite _fwrite_nolock
//     #define fflush _fflush_nolock
//     #define fputc  _fputc_nolock
//
// -- so any qualified call turns into one naming a function that does not exist
// there. `std::fwrite( ... )` becomes `std::_fwrite_nolock( ... )`, and fmt,
// which qualifies properly, stopped compiling: "'_fwrite_nolock': is not a
// member of 'std'".
//
//   Force-included, so this was done to every dependency in the build to save a
// lock acquisition per stdio call in ours. It also silently makes stdio
// non-thread-safe, which is a poor trade to impose on code that never asked.
//                                        (30.07.2026.) (SW port)
//
//   And it came back without us. On MSVC 14.51 `std::fputc` still expands to
// `std::_fputc_nolock` -- nothing in this tree or under libs/ defines
// _CRT_DISABLE_PERFCRIT_LOCKS, so the rewrite is the toolchain's own. What is
// certain is the shape: `fputc` is a macro there and `fputs` on the adjacent
// line is not. Our five one-character writes were all newlines, so they are
// `std::fputs( "\n", stderr )` now -- one call, no macro to be caught by, and
// nothing to re-diagnose the next time a compiler decides differently.
//                                        (05.08.2026.) (SW port)

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
