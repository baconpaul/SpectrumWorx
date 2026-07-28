////////////////////////////////////////////////////////////////////////////////
///
/// \file typeTraits.hpp
/// --------------------
///
///   Teaches the standard library's type traits about restrict qualified
/// pointers. The TR1 fallbacks it used to carry for pre-2011 libstdc++ are
/// gone along with the Boost.Config macros that selected them.
///
/// Copyright (c) 2011 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef typeTraits_hpp__45496FE2_5F16_4115_8225_39355C7AB4D5
#define typeTraits_hpp__45496FE2_5F16_4115_8225_39355C7AB4D5
//------------------------------------------------------------------------------

#include <ciso646>

#ifdef _STLPORT_VERSION
#error STLPort not supported - please use a more up to date C++ standard library implementation
#endif // _STLPORT_VERSION

#include <type_traits>

#define LE_AUX_TYPE_TRAITS_NAMESPACE_BEGIN()                                                       \
    namespace std                                                                                  \
    {
#define LE_AUX_TYPE_TRAITS_NAMESPACE_END() }

#include "abi.hpp"
//------------------------------------------------------------------------------

LE_AUX_TYPE_TRAITS_NAMESPACE_BEGIN()
template <typename T> using has_trivial_default_constructor = is_trivially_default_constructible<T>;
template <typename T> using has_trivial_destructor = is_trivially_destructible<T>;

/// \note Workarounds for lack of restricted pointer support in most STL's.
/// Check if a third party workaround is already included.
///                                           (11.09.2013.) (Domagoj Saric)
template <typename T> struct is_trivially_default_constructible<T *LE_RESTRICT> : true_type
{
};
template <typename T> struct is_trivially_destructible<T *LE_RESTRICT> : true_type
{
};
#ifndef BOOST_DISPATCH_RESTRICT
template <typename T> struct is_pointer<T *LE_RESTRICT> : true_type
{
};
#endif // BOOST_DISPATCH_RESTRICT
LE_AUX_TYPE_TRAITS_NAMESPACE_END()

#ifdef __clang__
template <typename T>
void *__attribute__((nothrow)) operator new(std::size_t /*count*/, T * LE_RESTRICT *const pStorage)
{
    LE_ASSUME(pStorage);
    return reinterpret_cast<void *>(reinterpret_cast<std::size_t>(pStorage));
}
#endif // __clang__

#undef LE_AUX_TYPE_TRAITS_NAMESPACE_BEGIN
#undef LE_AUX_TYPE_TRAITS_NAMESPACE_END

//------------------------------------------------------------------------------
#endif // typeTraits_hpp
