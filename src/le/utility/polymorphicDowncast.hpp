////////////////////////////////////////////////////////////////////////////////
///
/// \file polymorphicDowncast.hpp
/// -----------------------------
///
///   Replaces Boost.Conversion's polymorphic_downcast: a static_cast that is
/// checked with dynamic_cast in debug builds. Unlike the Boost original this
/// compiles without RTTI, dropping to a plain static_cast rather than failing.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef polymorphicDowncast_hpp__D51A73E8_6C29_4F84_A0B7_39E2C6D1B845
#define polymorphicDowncast_hpp__D51A73E8_6C29_4F84_A0B7_39E2C6D1B845
//------------------------------------------------------------------------------
#include "assert.hpp"

namespace LE::Utility
{

template <class Target, class Source> Target polymorphicDowncast(Source *const pSource) noexcept
{
#if !defined(NDEBUG) && !defined(LE_NO_RTTI)
    LE_ASSERT_MSG(dynamic_cast<Target>(pSource) == pSource, "Invalid downcast.");
#endif
    return static_cast<Target>(pSource);
}

} // namespace LE::Utility

#endif // polymorphicDowncast_hpp
