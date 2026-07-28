////////////////////////////////////////////////////////////////////////////////
///
/// module.cpp
/// ----------
///
/// Copyright (c) 2011 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "moduleDSPAndGUI.hpp"

#include "automatedModuleImpl.inl"

#include "le/utility/parentFromMember.hpp"
#include "le/spectrumworx/engine/moduleNode.hpp" // for intrusive_ptr_release_deleter
#include "gui/editor/spectrumWorxEditor.hpp"

#include "le/utility/intrusivePtr.hpp"

#include <optional>
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace SW
{
//------------------------------------------------------------------------------

// Implementation note:
//   This is required to prevent Clang from inlining the base destructor into
// each ModuleDSP<> destructor.
//                                            (13.12.2011.) (Domagoj Saric)
LE_NOINLINE LE_COLD Module::~Module() { LE_ASSUME(!ui_.has_value()); }

void Module::createGUI(GUI::SpectrumWorxEditor &editor, std::uint8_t const moduleIndex)
{
    /// \note This used to be a Boost.Optional typed in-place factory, whose
    /// apply() ran the widget construction below before the optional marked
    /// itself as initialised. std::optional::emplace() engages first, so a
    /// re-entrant gui() during doCreateGUI() now sees a half built ModuleUI
    /// where it used to see an empty optional.
    ///                                       (28.07.2026.) (SW port)
    auto const constructModuleUI([&](GUI::ModuleUI &moduleUI) {
        moduleUI.module().doCreateGUI(moduleUI);
        LE_ASSERT_MSG(
            moduleUI.getNumChildComponents() ==
                (GUI::ModuleUI::baseWidgets + moduleUI.module().numberOfEffectSpecificParameters()),
            "Unexpected number of child widgets at end of ModuleUI constructor.");
        moduleUI.updateForEngineSetupChanges(editor.engineSetup());
    });

    if (!GUI::isThisTheGUIThread())
    {
        LE::Utility::IntrusivePtr<Module> pModule(this);
        GUI::postMessage([=, &editor]() { pModule->createGUI(editor, moduleIndex); });
        return;
    }

    try
    {
        LE_ASSERT(GUI::isThisTheGUIThread() ||
                  juce::MessageManager::getInstance()->currentThreadHasLockedMessageManager());
        LE_ASSERT(!ui_);
        constructModuleUI(ui_.emplace());
        LE_ASSERT(gui());

#ifndef NDEBUG
        /// \note Certain debug-only sanity checks may require early access to
        /// the editor (e.g. for access to the Engine::Setup instance).
        ///                                   (28.03.2014.) (Domagoj Saric)
        editor.addChildComponent(&*gui());
#endif // nDEBUG

        /// \note It is assumed that right upon (GUI) creation a module is not
        /// selected and that therefor its shared parameters UI is not active
        /// and does not need to be updated.
        ///                                   (07.02.2014.) (Domagoj Saric)
        LE_ASSERT(!gui()->selected());
        gui()->setBypass(bypass());

        using GUI::ModuleControlBase;
        std::uint8_t const numberOfEffectParameters(numberOfEffectSpecificParameters());
        for (std::uint8_t effectParameterIndex(0); effectParameterIndex < numberOfEffectParameters;
             ++effectParameterIndex)
            gui()->setEffectParameter(effectParameterIndex,
                                      getEffectParameter(effectParameterIndex),
                                      GUI::ModuleUI::AutomationOrPreset);

        gui()->moveToSlot(moduleIndex);
        GUI::addToParentAndShow(editor, *gui());
    }
    catch (...)
    {
    }
}

bool Module::destroyGUI()
{
    // Implementation note:
    //   The current module has to be removed from the processing chain before
    // its GUI is destroyed or added after its GUI is created to prevent the
    // processing thread from accessing the, possibly partially
    // destroyed/created, GUI in the ModuleDSP<>::doPreProcess() member
    // function.
    //                                        (18.11.2011.) (Domagoj Saric)
    if (GUI::isThisTheGUIThread() /*&& referenceCount_ == 1*/)
    {
        LE_ASSERT(juce::MessageManager::getInstance()->currentThreadHasLockedMessageManager());
        LE_ASSERT(
            gui()); //...might not be true if not part of the "active chain"...checked outside for now...
#if !LE_SW_SEPARATED_DSP_GUI
        /// \note Make sure the module is not 'used' from the processing thread
        /// (possibly updating its GUI from active LFOs).
        ///                                       (18.03.2014.) (Domagoj Saric)
        auto const processingLock(gui()->getProcessingLock());
#endif
        gui() = std::nullopt;
        doDestroyGUI(*this);
        return true;
    }
    else if (gui())
    {
        GUI::postMessage(*this, [](GUI::ModuleUI &gui) {
            LE_VERIFY(gui.module()./*...mrmlj...*/ destroyGUI());
            return true;
        });
        return false;
    }
    // Not created or already destroyed
    return true;
}

float Module::setBaseParameter(std::uint8_t const sharedParameterIndex, float const parameterValue)
{
    float const setValue(ModuleDSP::setBaseParameter(sharedParameterIndex, parameterValue));
    if (gui())
        gui()->setBaseParameter(sharedParameterIndex, setValue, GUI::ModuleUI::AutomationOrPreset);
    return setValue;
}

float Module::setEffectParameter(std::uint8_t const effectParameterIndex,
                                 float const parameterValue)
{
    float const setValue(ModuleDSP::setEffectParameter(effectParameterIndex, parameterValue));
    LE_ASSERT(
        (parameterValue == setValue) ||
        (effectSpecificParameterInfo(effectParameterIndex).type != ParameterInfo::FloatingPoint));
    if (gui())
        gui()->setEffectParameter(effectParameterIndex, setValue,
                                  GUI::ModuleUI::AutomationOrPreset);
    return setValue;
}

float Module::setParameterValueFromUI(std::uint8_t const parameterIndex, float const value)
{
    LE_ASSERT_MSG((parameterIndex == 0) || !lfo(parameterIndex - 1).enabled(),
                  "Parameter changed from the GUI while its LFO is enabled?");
    return (parameterIndex < numberOfBaseParameters)
               ? ModuleDSP::setBaseParameter(parameterIndex, value)
               : ModuleDSP::setEffectParameter(effectSpecificParameterIndex(parameterIndex), value);
}

void Module::setBaseParameterFromLFO(std::uint8_t const sharedParameterIndex,
                                     LFO::value_type const lfoValue)
{
    auto const parameterValue(
        ModuleParameters::setBaseParameterFromLFOAux(sharedParameterIndex, lfoValue));
    if (gui())
        gui()->setBaseParameter(sharedParameterIndex, parameterValue, GUI::ModuleUI::LFOValue);
}

void Module::setEffectParameterFromLFO(std::uint8_t const effectParameterIndex,
                                       LFO::value_type const lfoValue)
{
    auto const parameterValue(
        ModuleParameters::setEffectParameterFromLFOAux(effectParameterIndex, lfoValue));
    if (gui())
        gui()->setEffectParameter(effectParameterIndex, parameterValue, GUI::ModuleUI::LFOValue);
}

void LE_COLD Module::updateLFOGUI(std::uint8_t const parameterIndex,
                                  std::uint8_t const lfoParameterIndex,
                                  Plugins::AutomatedParameterValue const value)
{
    GUI::postMessage(*this, [=](GUI::ModuleUI &gui) {
        gui.updateLFOParameter(parameterIndex, lfoParameterIndex, value);
        return true;
    });
}

Module &Module::fromGUI(GUI::ModuleUI &gui)
{
    return Utility::ParentFromOptionalMember<Module, GUI::ModuleUI, &Module::ui_, false>()(gui);
}

namespace Engine
{
void LE_NOINLINE intrusive_ptr_release_deleter(ModuleNode const *LE_RESTRICT const pModuleNode)
{
    auto const &module(actualModule<Module>(*pModuleNode));
    if (!module.gui() || const_cast<Module &>(module).destroyGUI())
    {
        LE_ASSUME(module.referenceCount_ == 0);
        delete &module;
    }
    else
    {
        LE_ASSUME(module.referenceCount_ == 1);
    }
}
} // namespace Engine

//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
