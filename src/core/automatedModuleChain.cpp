////////////////////////////////////////////////////////////////////////////////
///
/// automatedModuleChain.cpp
/// ------------------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "automatedModuleChain.hpp"

#include "le/spectrumworx/engine/module.hpp"
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace SW
{
//------------------------------------------------------------------------------

ModuleChainParameter
AutomatedModuleChain::getParameterForIndex(std::uint8_t const moduleIndex) const
{
    auto const pModule(ModuleChainImpl::module(moduleIndex));
    return ModuleChainParameter(
        !isEnd(pModule) ? static_cast<std::int8_t>(
                              Engine::actualModule<Module const>(*pModule).effectTypeIndex())
                        : noModule);
}

AutomatedModuleChain::ModulePtr AutomatedModuleChain::module(std::uint8_t const index)
{
    return moduleAs<Module>(index);
}
AutomatedModuleChain::ModuleCPtr AutomatedModuleChain::module(std::uint8_t const index) const
{
    return moduleAs<Module const>(index);
}

//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
