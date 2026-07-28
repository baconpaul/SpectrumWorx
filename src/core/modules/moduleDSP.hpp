////////////////////////////////////////////////////////////////////////////////
///
/// \file moduleDSP.hpp
///
///    SW plugin module DSP interface and implementation.
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef moduleDSP_hpp__69C2A250_9301_4C9F_8EED_BC1DBEB7F8A8
#define moduleDSP_hpp__69C2A250_9301_4C9F_8EED_BC1DBEB7F8A8
//------------------------------------------------------------------------------
#include "automatedModuleImpl.hpp"

#include "le/spectrumworx/engine/module.hpp"
#include "le/utility/platformSpecifics.hpp"
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace SW
{
//------------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
///
/// \class ModuleDSP
///
////////////////////////////////////////////////////////////////////////////////

class LE_NOVTABLE ModuleDSP : public Engine::ModuleDSP, public AutomatedModuleImpl<ModuleDSP>
{
  public:
    template <class Effect> class Impl;

  protected:
#ifdef _MSC_VER
    template <typename... T>
    LE_COLD ModuleDSP(T &&...args) : Engine::ModuleDSP(std::forward<T>(args)...)
    {
    }
#else
    using Engine::ModuleDSP::ModuleDSP;
#endif // _MSC_ver
}; // class _MSC_VER

//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // moduleDSP_hpp
