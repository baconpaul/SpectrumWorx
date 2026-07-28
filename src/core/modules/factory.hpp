////////////////////////////////////////////////////////////////////////////////
///
/// \file factory.hpp
/// -----------------
///
/// Copyright (c) 2014 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef moduleFactory_hpp__C444656C_70DA_479E_8BB5_C889A9B1EFA5
#define moduleFactory_hpp__C444656C_70DA_479E_8BB5_C889A9B1EFA5
//------------------------------------------------------------------------------
#include "le/utility/platformSpecifics.hpp"
#include "le/utility/cstdint.hpp"

#include "le/utility/intrusivePtr.hpp"
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace SW
{
//------------------------------------------------------------------------------

struct ModuleFactory
{
    template <class ModuleInterface>
    static LE::Utility::IntrusivePtr<ModuleInterface> create(std::int8_t effectIndex);
}; // struct ModuleFactory

//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // moduleFactory_hpp
