////////////////////////////////////////////////////////////////////////////////
///
/// \file channelStateStatic.hpp
/// ----------------------------
///
/// Copyright (c) 2012 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef channelStateStatic_hpp__00473652_1B0C_4BDC_A10A_D2AA913A036A
#define channelStateStatic_hpp__00473652_1B0C_4BDC_A10A_D2AA913A036A
//------------------------------------------------------------------------------
#include "le/utility/platformSpecifics.hpp"
#include "le/utility/span.hpp"

namespace LE::SW
{

namespace Engine
{
struct StorageFactors;
typedef LE::Utility::Span<char> Storage;
} // namespace Engine

namespace Effects
{

struct StaticChannelState
{
    static unsigned char requiredStorage(Engine::StorageFactors const &) { return 0; }
    static void resize(Engine::StorageFactors const &, Engine::Storage const &) {}
}; // struct StaticChannelState

} // namespace Effects

} // namespace LE::SW

#endif // channelStateStatic_hpp
