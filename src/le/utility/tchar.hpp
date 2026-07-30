////////////////////////////////////////////////////////////////////////////////
///
/// \file tchar.hpp
/// ---------------
///
/// Selected (Microsoft's) tchar.h bits for compilers that do not provide them.
///
/// Copyright (c) 2010 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef tchar_hpp__5137b405_2BF4_4714_8BB3_8F3342D64267
#define tchar_hpp__5137b405_2BF4_4714_8BB3_8F3342D64267
//------------------------------------------------------------------------------
#include <string_view>
#ifdef _MSC_VER
#include "tchar.h"
#else
#include <cstdlib>
#include <cstring>

#ifdef _UNICODE
typedef wchar_t TCHAR;

#define _T(x) L##x

#define _itot _itow
#define _ltot _ltow
#define _ultot _ultow

#define _tcscpy wcscpy
#define _tcslen wcslen
#define _tcscmp wcscmp
#define _tcsrchr wcsrchr

#define _stprintf swprintf
#define _sntprintf _snwprintf
#else
typedef char TCHAR;

#define _T(x) x

#define _itot _itoa
#define _ltot _ltoa
#define _ultot _ultoa

#define _tcscpy strcpy
#define _tcslen strlen
#define _tcscmp strcmp
#define _tcsrchr strrchr

#define _stprintf sprintf
#define _sntprintf snprintf
#endif
#endif // _MSC_VER

#ifdef _WIN32
extern "C" __declspec(dllimport) int __cdecl wsprintfA(__out char *,
                                                       __in __format_string char const *, ...);
extern "C" __declspec(dllimport) int __cdecl wsprintfW(__out wchar_t *,
                                                       __in __format_string wchar_t const *, ...);
#define LE_INT_SPRINTFA ::wsprintfA
#define LE_INT_SPRINTFW ::wsprintfW
#else // _WIN32
#define LE_INT_SPRINTFA std::sprintf
#define LE_INT_SPRINTFW std::swprintf
#endif // _WIN32

#ifdef _UNICODE
#define LE_INT_SPRINTF LE_INT_SPRINTFW
#else
#define LE_INT_SPRINTF LE_INT_SPRINTFA
#endif // _UNICODE

#include "platformSpecifics.hpp"

//...mrmlj...quick-fixes for Boost.Range restrict support...
/// \note Boost.Range is gone; only NT2/Boost.SIMD still needs to be told about
/// restrict qualified pointers, and it brings its own Boost with it.
///                                           (28.07.2026.) (SW port)
#ifdef LE_HAS_NT2
#include "boost/dispatch/meta/is_iterator.hpp"
#include "boost/type_traits/detail/yes_no_type.hpp"

#include <type_traits>
namespace boost
{
#ifdef BOOST_DISPATCH_NO_RESTRICT
template <typename T> struct is_pointer;
template <typename T> struct is_pointer<T *LE_RESTRICT> : std::true_type
{
};
template <typename T> struct is_pointer<T const *LE_RESTRICT> : std::true_type
{
};
#endif // BOOST_DISPATCH_NO_RESTRICT
template <typename T> struct remove_const;
template <typename T> struct remove_const<T *LE_RESTRICT const>
{
    typedef T *LE_RESTRICT type;
};
template <typename T> struct remove_const<T const *LE_RESTRICT const>
{
    typedef T const *LE_RESTRICT type;
};
} // namespace boost
#endif // LE_HAS_NT2

/// \note There was a global operator==( std::string_view, std::string_view )
/// here, comparing with memcmp over begin(). The standard library has provided
/// that comparison since C++17, so it was a second candidate for every
/// string_view comparison in the codebase -- and it only ever compiled because a
/// string_view iterator happens to be a `char const *` in libc++ and libstdc++.
/// MSVC's is a class type, which is what finally objected.
///                                           (30.07.2026.) (SW port)

//------------------------------------------------------------------------------
#endif // tchar_hpp
