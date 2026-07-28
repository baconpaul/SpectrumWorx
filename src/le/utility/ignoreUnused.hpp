////////////////////////////////////////////////////////////////////////////////
///
/// \file ignoreUnused.hpp
/// ----------------------
///
///   Replaces Boost.Core's ignore_unused and ignore_unused_variable_warning
/// for the parameters that only exist to be asserted on.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef ignoreUnused_hpp__4C3A9F71_2E64_4B0D_9C57_1D8B6A0F3E24
#define ignoreUnused_hpp__4C3A9F71_2E64_4B0D_9C57_1D8B6A0F3E24
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace Utility
{
//------------------------------------------------------------------------------

template <typename... T> constexpr void ignoreUnused(T const &...) noexcept {}

//------------------------------------------------------------------------------
} // namespace Utility
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // ignoreUnused_hpp
