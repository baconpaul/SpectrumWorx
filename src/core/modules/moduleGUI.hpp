////////////////////////////////////////////////////////////////////////////////
///
/// \file moduleGUI.hpp
///
///    SW plugin module interface and implementation.
///
/// Copyright (c) 2009 - 2015. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef moduleGUI_hpp__E42344C7_8515_44B3_9C24_6F88CC5840FA
#define moduleGUI_hpp__E42344C7_8515_44B3_9C24_6F88CC5840FA
//------------------------------------------------------------------------------
#include "core/modules/automatedModuleImpl.hpp"

#include "gui/modules/moduleUI.hpp"

#include "le/spectrumworx/engine/moduleParameters.hpp"
#include "le/utility/cstdint.hpp"
#include <optional>
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace SW
{
//------------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
///
/// \class Module
///
////////////////////////////////////////////////////////////////////////////////

class LE_NOVTABLE ModuleGUI : public GUI::ModuleUI,
                              public Engine::ModuleParameters,
                              public AutomatedModuleImpl<ModuleGUI>
{
  public: // Automation
    template <class AutomatedParameter>
    std::optional<std::pair<std::uint_fast8_t, LFO::value_type>>
    setAutomatedLFOParameter(std::uint_fast8_t const parameterIndex,
                             std::uint_fast8_t const lfoParameterIndex,
                             Plugins::AutomatedParameterValue const value)
    {
        auto const result(Automation::setAutomatedLFOParameter<AutomatedParameter>(
            parameterIndex, lfoParameterIndex, value, *this));
        GUI::ModuleUI::updateLFOParameter(parameterIndex, lfoParameterIndex, value);
        return result;
    }

    float getSharedParameter(std::uint_fast8_t sharedParameterIndex) const;
    void setSharedParameter(std::uint_fast8_t sharedParameterIndex, float parameterValue);

    float getEffectParameter(std::uint_fast8_t effectParameterIndex) const override final;
    float setEffectParameter(std::uint_fast8_t effectParameterIndex,
                             float parameterValue) override final;

  public:
    /// \note The `LE_ASSUME( &moduleUI )` this opened with is gone for the
    /// reason given in moduleControl.cpp: the address of a reference cannot be
    /// null, so it was an assumption about nothing -- and one that would warn,
    /// fatally under -Werror, the day this inline function got called.
    ///                                       (05.08.2026.) (SW port)
    static ModuleGUI &fromGUI(GUI::ModuleUI &moduleUI)
    {
        return static_cast<ModuleGUI &>(moduleUI);
    }

    GUI::ModuleUI *gui() { return this; }

  public:
    template <class Effect> class Impl;

  protected:
    template <typename... T>
    ModuleGUI(T &&...args) : Engine::ModuleParameters(std::forward<T>(args)...)
    {
    }
    ~ModuleGUI();

  private:
    friend class AutomatedModuleImpl<ModuleGUI>;
    void setSharedParameterFromLFO(std::uint_fast8_t parameterIndex, LFO::value_type);
    void setEffectParameterFromLFO(std::uint_fast8_t parameterIndex, LFO::value_type);
}; // class ModuleGUI

//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // moduleGUI_hpp
