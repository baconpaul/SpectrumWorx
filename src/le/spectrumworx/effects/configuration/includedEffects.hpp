////////////////////////////////////////////////////////////////////////////////
///
/// \file includedEffects.hpp
/// -------------------------
///
/// Which effects this build ships, and the index sequence to iterate them with.
/// Both are now trivial -- every effect is always included -- but the names are
/// kept so that the call sites do not have to change.
///
/// Copyright (c) 2012 - 2016. Little Endian Ltd.
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef includedEffects_hpp__7EA7BD90_59C4_4F59_87BB_3E220D923733
#define includedEffects_hpp__7EA7BD90_59C4_4F59_87BB_3E220D923733
//------------------------------------------------------------------------------
#include "constants.hpp"

#include "boost/mpl/range_c.hpp"

#include <array>
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace SW
{
//------------------------------------------------------------------------------
namespace Effects
{
//------------------------------------------------------------------------------

using IncludedEffects = std::array<bool, Constants::numberOfEffects>;

inline constexpr IncludedEffects includedEffects{[] {
    IncludedEffects result{};
    result.fill(true);
    return result;
}()};

/// Shared list of module indices, for Utility::switch_ and Utility::forEach.
using ValidIndices = boost::mpl::range_c<unsigned, 0, Constants::numberOfEffects>;

//------------------------------------------------------------------------------
} // namespace Effects
//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // includedEffects_hpp
