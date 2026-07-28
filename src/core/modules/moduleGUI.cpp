////////////////////////////////////////////////////////////////////////////////
///
/// moduleGUI.cpp
/// -------------
///
/// Copyright (c) 2011 - 2015. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "moduleGUI.hpp"
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace SW
{
//------------------------------------------------------------------------------

LE_COLD ModuleGUI::~ModuleGUI() {}

float ModuleGUI::getSharedParameter(std::uint_fast8_t const sharedParameterIndex) const
{
    auto const result(ModuleParameters::getSharedParameter(sharedParameterIndex));
    LE_ASSERT(result == GUI::ModuleUI::getSharedParameter(sharedParameterIndex));
    return result;
}

void ModuleGUI::setSharedParameter(std::uint_fast8_t const sharedParameterIndex,
                                   float const parameterValue)
{
    ModuleParameters::setSharedParameter(sharedParameterIndex, parameterValue);
    GUI::ModuleUI ::setSharedParameter(sharedParameterIndex, parameterValue,
                                       GUI::ModuleUI::AutomationOrPreset);
}

float ModuleGUI::getEffectParameter(std::uint_fast8_t const effectParameterIndex) const
{
    return GUI::ModuleUI::getEffectParameter(effectParameterIndex);
}

float ModuleGUI::setEffectParameter(std::uint_fast8_t const effectParameterIndex,
                                    float const parameterValue)
{
    GUI::ModuleUI::setEffectParameter(effectParameterIndex, parameterValue,
                                      GUI::ModuleUI::AutomationOrPreset);
    return parameterValue; //...mrmlj...no snapping/quantization? reinvestigate...
}

void ModuleGUI::setSharedParameterFromLFO(std::uint_fast8_t const sharedParameterIndex,
                                          LFO::value_type const lfoValue)
{
    //...mrmlj...AutomatedModuleImpl duplication
    auto const parameterValue(ModuleParameters::normalisedToParameterValue(
        lfoValue, parameterInfos()[sharedParameterIndex]));
    GUI::ModuleUI::setSharedParameter(sharedParameterIndex, parameterValue,
                                      GUI::ModuleUI::LFOValue);
}

void ModuleGUI::setEffectParameterFromLFO(std::uint_fast8_t const effectParameterIndex,
                                          LFO::value_type const lfoValue)
{
    //...mrmlj...AutomatedModuleImpl duplication
    auto const parameterValue(ModuleParameters::normalisedToParameterValue(
        lfoValue, effectSpecificParameterInfo(effectParameterIndex)));
    GUI::ModuleUI::setEffectParameter(effectParameterIndex, parameterValue,
                                      GUI::ModuleUI::LFOValue);
}

//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
