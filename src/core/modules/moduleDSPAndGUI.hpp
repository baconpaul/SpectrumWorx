////////////////////////////////////////////////////////////////////////////////
///
/// \file moduleDSPAndGUI.hpp
///
///    SW plugin module interface and implementation.
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef moduleDSPAndGUI_hpp__C1E04A83_2733_44D6_A484_9F03A08AD6CD
#define moduleDSPAndGUI_hpp__C1E04A83_2733_44D6_A484_9F03A08AD6CD
//------------------------------------------------------------------------------
#include "automatedModuleImpl.hpp"

#include "gui/modules/moduleUI.hpp"

#include "le/spectrumworx/engine/module.hpp"
#include "le/utility/cstdint.hpp"
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace SW
{
//------------------------------------------------------------------------------

namespace GUI
{
class SpectrumWorxEditor;
}

////////////////////////////////////////////////////////////////////////////////
///
/// \class Module
///
////////////////////////////////////////////////////////////////////////////////

class LE_NOVTABLE Module : public Engine::ModuleDSP,
                           public AutomatedModuleImpl<Module>,
                           private GUI::ParameterWidgetsVTable<Module>
{
  public: // GUI
    LE_NOTHROW void createGUI(GUI::SpectrumWorxEditor &, std::uint8_t moduleIndex);
    LE_NOTHROW bool destroyGUI();

  public: // Automation
    template <class AutomatedParameter>
    boost::optional<std::pair<std::uint8_t, LFO::value_type>> LE_NOTHROW
    setAutomatedLFOParameter(std::uint8_t const parameterIndex,
                             std::uint8_t const lfoParameterIndex,
                             Plugins::AutomatedParameterValue const value)
    {
        auto const result(Automation::setAutomatedLFOParameter<AutomatedParameter>(
            parameterIndex, lfoParameterIndex, value, *this));
        updateLFOGUI(parameterIndex, lfoParameterIndex, value);
        return result;
    }

  public:
    using OptionalUI = boost::optional<GUI::ModuleUI>;

    OptionalUI &gui() { return ui_; }
    OptionalUI const &gui() const { return ui_; }

    float setParameterValueFromUI(std::uint8_t parameterIndex, float value);

    static Module &fromGUI(GUI::ModuleUI &);

  public:
    template <class Effect> class Impl;

  protected:
    template <class Effect, typename... T>
    Module(Impl<Effect> *const pImpl, T &&...args)
        : ModuleDSP(std::forward<T>(args)...), GUI::ParameterWidgetsVTable<Module>(*pImpl)
    {
    }

  public: //...mrmlj...(delete pModule)...
    ~Module();

  private:
    friend class AutomatedModuleImpl<Module>;
    float setBaseParameter(std::uint8_t sharedParameterIndex, float parameterValue) override final;
    float setEffectParameter(std::uint8_t effectParameterIndex,
                             float parameterValue) override final;

  private:
    void setBaseParameterFromLFO(std::uint8_t parameterIndex, LFO::value_type) override final;
    void setEffectParameterFromLFO(std::uint8_t parameterIndex, LFO::value_type) override final;

    void updateLFOGUI(std::uint8_t parameterIndex, std::uint8_t lfoParameterIndex,
                      Plugins::AutomatedParameterValue);

  private:
    OptionalUI ui_;
}; // class Module

//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // moduleDSPAndGUI_hpp
