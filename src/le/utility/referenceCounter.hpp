////////////////////////////////////////////////////////////////////////////////
///
/// \file referenceCounter.hpp
/// --------------------------
///
/// Copyright (c) 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef referenceCounter_hpp__61C56D58_836D_4ECC_B55D_84DD64DBAD0D
#define referenceCounter_hpp__61C56D58_836D_4ECC_B55D_84DD64DBAD0D
//------------------------------------------------------------------------------
#include "platformSpecifics.hpp"

#include "assert.hpp"
#include "ignoreUnused.hpp"

#include <atomic>
#include <cstdint>
#include <limits>
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace Utility
{
//------------------------------------------------------------------------------

namespace Detail
{
using counter_t = std ::atomic_uint_fast8_t;
static_assert(sizeof(counter_t) == sizeof(char), "");
} // namespace Detail

// https://channel9.msdn.com/Shows/Going+Deep/Cpp-and-Beyond-2012-Herb-Sutter-atomic-Weapons-2-of-2 @ ~1:20:00
struct ReferenceCount : Detail::counter_t
{
    using value_type = std::uint8_t;
    ReferenceCount(std::uint8_t const initialValue = 0) : Detail::counter_t(initialValue) {}
#ifndef _MSC_VER
    using Detail::counter_t::counter_t;
#endif // !_MSC_VER

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4553) // '==' : operator has no effect; did you intend '='?
#endif                          // _MSC_VER
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wassume"
#endif // __clang__
    /*std::uint8_t*/ void operator++()
    {
        LE_ASSUME((*this) >= 0);
        LE_ASSUME((*this) < std::numeric_limits<std::uint8_t>::max());
        /*return*/ fetch_add(1, std::memory_order_relaxed) /*+ 1*/;
    }
    std::uint8_t operator--()
    {
        auto const result(fetch_sub(1, std::memory_order_acq_rel) - 1);
        LE_ASSUME(result >= 0);
        LE_UNLIKELY(result == 0);
        return static_cast<std::uint8_t>(result);
    }
#ifdef __clang__
#pragma clang diagnostic pop
#endif // __clang__
#ifdef _MSC_VER
#pragma warning(pop)
#endif // _MSC_VER

    void verifyCountEqual(value_type const value)
    {
        LE_ASSERT(this->load(std::memory_order_seq_cst) == value);
        LE::Utility::ignoreUnused(value);
    }
}; // class ReferenceCount

//------------------------------------------------------------------------------
} // namespace Utility
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // referenceCounter_hpp
