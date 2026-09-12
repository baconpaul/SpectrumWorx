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

namespace LE::SW
{

/// \brief What a write does to a parameter whose LFO is running.
enum struct WhileModulated : bool
{
    ignored, ///< automation: the LFO has the parameter
    stored   ///< another Program's state being copied \see issue #198
};

template <class InterfaceImpl> class AutomatedModuleImpl
{
  public: // Parameters
    Plugins::AutomatedParameterValue getSharedAutomatedParameter(std::uint8_t parameterIndex,
                                                                 bool normalised) const;
    Plugins::AutomatedParameterValue
    getEffectSpecificAutomatedParameter(std::uint8_t effectSpecificParameterIndex,
                                        bool normalised) const;
    Plugins::AutomatedParameterValue getAutomatedParameter(std::uint8_t parameterIndex,
                                                           bool normalised) const;

    void setAutomatedParameter(std::uint8_t parameterIndex, Plugins::AutomatedParameterValue,
                               bool normalised, WhileModulated = WhileModulated::ignored);

    char const *getParameterValueString(std::uint8_t parameterIndex,
                                        LE::Parameters::AutomatedParameterPrinter const &) const;

  private:
    InterfaceImpl &impl() { return *static_cast<InterfaceImpl *LE_RESTRICT>(this); }
    InterfaceImpl const &impl() const { return const_cast<AutomatedModuleImpl &>(*this).impl(); }
}; // class AutomatedModuleImpl

} // namespace LE::SW

#endif // automatedModuleImpl_hpp
