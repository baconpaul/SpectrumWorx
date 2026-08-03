////////////////////////////////////////////////////////////////////////////////
///
/// \file clear.hpp
/// ---------------
///
/// Copyright (c) 2013 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef clear_hpp__834DF3B1_52B1_4110_899E_A926DA81EF95
#define clear_hpp__834DF3B1_52B1_4110_899E_A926DA81EF95
//------------------------------------------------------------------------------
#include "platformSpecifics.hpp"

#include <cstring>
#include <type_traits>
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace Utility
{
//------------------------------------------------------------------------------

namespace Detail
{
template <unsigned int size> void clear(void *LE_RESTRICT const pPODObject)
{
    std::memset(pPODObject, 0, size);
}
} // namespace Detail

template <typename POD> void clear(POD &pod)
{
    /// \note `std::is_pod<POD>::value || __has_trivial_assign(POD)` stood here,
    /// the second arm for a 2011 Boost bug the two links below are about. Both
    /// halves are deprecated -- is_pod in C++20, __has_trivial_assign in favour
    /// of __is_trivially_assignable -- and neither says what memsetting to zero
    /// actually needs, which is that the bytes may be copied around and that the
    /// all-zero pattern is a value the type could have had anyway. is_pod also
    /// demanded standard layout, which memset does not care about.
    ///
    /// https://svn.boost.org/trac/boost/ticket/5635
    /// http://boost.2283326.n4.nabble.com/TypeTraits-A-patch-for-clang-s-intrinsics-was-type-traits-is-enum-on-scoped-enums-doesn-t-works-as-e-td3781550.html
    ///                                       (02.08.2026.) (SW port)
    static_assert(std::is_trivially_copyable_v<POD> &&
                      std::is_trivially_default_constructible_v<POD>,
                  "Will not memset a non trivial type.");
    Detail::clear<sizeof(pod)>(&pod);
}

//------------------------------------------------------------------------------
} // namespace Utility
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // clear_hpp
