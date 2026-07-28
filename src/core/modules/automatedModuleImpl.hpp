////////////////////////////////////////////////////////////////////////////////
///
/// \file automatedModuleImpl.hpp
/// ----------------------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef automatedModuleImpl_hpp__4462A949_9FE5_44AF_8252_402F9E776690
#define automatedModuleImpl_hpp__4462A949_9FE5_44AF_8252_402F9E776690
//------------------------------------------------------------------------------
#include "automatedModule.hpp"

#include "le/utility/cstdint.hpp"
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace SW
{
//------------------------------------------------------------------------------

template <class InterfaceImpl> class LE_NOVTABLE AutomatedModuleImpl
{
  public: // Parameters
    Plugins::AutomatedParameterValue LE_NOALIAS
    getSharedAutomatedParameter(std::uint8_t parameterIndex, bool normalised) const;
    Plugins::AutomatedParameterValue LE_NOALIAS
    getEffectSpecificAutomatedParameter(std::uint8_t effectSpecificParameterIndex,
                                        bool normalised) const;
    Plugins::AutomatedParameterValue LE_NOALIAS getAutomatedParameter(std::uint8_t parameterIndex,
                                                                      bool normalised) const;

    void setAutomatedParameter(std::uint8_t parameterIndex, Plugins::AutomatedParameterValue,
                               bool normalised);

    LE_NOTHROWNOALIAS char const *
    getParameterValueString(std::uint8_t parameterIndex,
                            LE::Parameters::AutomatedParameterPrinter const &) const;

  private:
    InterfaceImpl &impl() { return *static_cast<InterfaceImpl *LE_RESTRICT>(this); }
    InterfaceImpl const &impl() const { return const_cast<AutomatedModuleImpl &>(*this).impl(); }
}; // class AutomatedModuleImpl

//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // automatedModuleImpl_hpp
