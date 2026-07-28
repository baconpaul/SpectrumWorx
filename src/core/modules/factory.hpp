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

    /// \note create() malloc()s and placement-news the *derived*
    /// ModuleInterface::Impl<Effect>, so destruction has to run that type's
    /// destructor and free() the same storage. The two
    /// intrusive_ptr_release_deleter overloads used to say `delete &module`,
    /// which is mismatched with malloc and, since ModuleInterface's destructor
    /// is not virtual outside the SDK build, would not have run the derived
    /// destructor either.
    ///                                       (28.07.2026.) (SW port)
    template <class ModuleInterface> static void destroy(ModuleInterface const &);
}; // struct ModuleFactory

//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // moduleFactory_hpp
