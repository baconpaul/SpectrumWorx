////////////////////////////////////////////////////////////////////////////////
///
/// automatedModuleImpl.inl
/// -----------------------
///
/// Copyright (c) 2011 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef automatedModuleImpl_inl__E1CBA39F_51D3_4DF9_A85A_6fA4DD3531DC
#define automatedModuleImpl_inl__E1CBA39F_51D3_4DF9_A85A_6fA4DD3531DC
//------------------------------------------------------------------------------
#include "automatedModuleImpl.hpp"

#include "le/parameters/printer.hpp"
#include "le/spectrumworx/engine/moduleParameters.hpp"

namespace LE::SW
{

template <class Impl>
Plugins::AutomatedParameterValue
AutomatedModuleImpl<Impl>::getSharedAutomatedParameter(std::uint8_t const parameterIndex,
                                                       bool const normalised) const
{
    /// \note The unmodulated value. This is what `clap_plugin_params::get_value`
    /// answers, and a host must not see an LFO sweeping there: the sweep is a
    /// modulation, and CLAP models modulation separately from value. It read the
    /// live parameter, so a host's generic panel polled the sweep.
    ///                                       (02.08.2026.) (SW port)
    float const parameterValue(impl().unmodulatedBaseParameter(parameterIndex));
    return Automation::sharedInternal2AutomatedValue(parameterIndex, parameterValue, normalised);
}

template <class Impl>
Plugins::AutomatedParameterValue AutomatedModuleImpl<Impl>::getEffectSpecificAutomatedParameter(
    std::uint8_t const effectSpecificParameterIndex, bool const normalised) const
{
    /// \note The unmodulated value; see getSharedAutomatedParameter() above.
    float const parameterValue(impl().unmodulatedEffectParameter(effectSpecificParameterIndex));
    return Automation::effectInternal2AutomatedValue(effectSpecificParameterIndex, parameterValue,
                                                     normalised, impl());
}

template <class Impl>
Plugins::AutomatedParameterValue
AutomatedModuleImpl<Impl>::getAutomatedParameter(std::uint8_t const parameterIndex,
                                                 bool const normalised) const
{
    if (parameterIndex < impl().numberOfBaseParameters)
        return this->getSharedAutomatedParameter(parameterIndex, normalised);
    else if (parameterIndex < impl().numberOfParameters())
        return this->getEffectSpecificAutomatedParameter(
            impl().effectSpecificParameterIndex(parameterIndex), normalised);
    else
        return Plugins::AutomatedParameterValue();
}

template <class Impl>
void AutomatedModuleImpl<Impl>::setAutomatedParameter(std::uint8_t const parameterIndex,
                                                      Plugins::AutomatedParameterValue const value,
                                                      bool const normalised)
{
    //...mrmlj...LE_ASSUME( parameterIndex < SW::Constants::maxNumberOfParametersPerModule );

    if (parameterIndex >= impl().numberOfParameters())
        return; // index out of range

    if (parameterIndex != 0 && impl().lfo(parameterIndex - 1).enabled()) //...mrmlj...skip bypass
        return; // skip automation if the parameter's LFO is enabled

    if (parameterIndex < impl().numberOfBaseParameters)
    {
        impl().setBaseParameter(parameterIndex, Automation::sharedAutomated2InternalValue(
                                                    parameterIndex, value, normalised));
        return;
    }

    std::uint8_t const effectSpecificParameterIndex(
        impl().effectSpecificParameterIndex(parameterIndex));
    impl().setEffectParameter(effectSpecificParameterIndex,
                              Automation::effectAutomated2InternalValue(
                                  effectSpecificParameterIndex, value, normalised, impl()));
} // AutomatedModuleImpl<Impl>::setAutomatedParameter()

template <class Impl>
char const *AutomatedModuleImpl<Impl>::getParameterValueString(
    std::uint8_t const index, LE::Parameters::AutomatedParameterPrinter const &printer) const
{
    if (index >= impl().numberOfParameters())
        return nullptr;

    /// \note An effect's parameters are reached by index through a static
    /// invoker, which has the type and no object, so the printer has to be given
    /// a value. Asked about the parameter's own, that value is read here -- this
    /// is the level that knows where a module keeps it.
    if (!printer.forValue)
    {
        printer.forValue = (index < Engine::ModuleParameters::BaseParameters::static_size)
                               ? impl().getBaseParameter(index)
                               : impl().getEffectParameter(impl().effectSpecificParameterIndex(index));
        printer.valueSource = LE::Parameters::AutomatedParameterPrinter::Unchanged;
    }

    return Automation::getParameterValueString(index, printer, impl());
}

} // namespace LE::SW

#endif // automatedModuleImpl_inl
