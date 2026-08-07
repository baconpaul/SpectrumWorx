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
#include "core/modules/factory.hpp"
#include "le/spectrumworx/engine/moduleNode.hpp" // for intrusive_ptr_release_deleter

#include "le/utility/intrusivePtr.hpp"

#include <optional>

namespace LE::SW
{

// Implementation note:
//   This is required to prevent Clang from inlining the base destructor into
// each ModuleDSP<> destructor.
//                                            (13.12.2011.) (Domagoj Saric)
LE_NOINLINE LE_COLD Module::~Module() {}

/// \note `createGUI()` and `destroyGUI()` stood here, ~100 lines of building a
/// module's editor region into a member of the module and taking it down again.
/// Both had to cope with being called from the wrong thread -- `createGUI` posted
/// itself to the message thread when it was not on it, and `destroyGUI` took the
/// *processing lock* on the message thread so that the audio thread could not be
/// halfway through the module while its widgets were freed.
///
///   Neither is needed. The editor owns the region and builds it in the region's
/// own constructor, on the thread that owns widgets, and the region holds a
/// counted reference to the module so the module cannot go while it is drawn.
///                                           (02.08.2026.) (SW port)

float Module::setParameterValueFromUI(std::uint8_t const parameterIndex, float const value)
{
    LE_ASSERT_MSG((parameterIndex == 0) || !lfo(parameterIndex - 1).enabled(),
                  "Parameter changed from the GUI while its LFO is enabled?");
    return (parameterIndex < numberOfBaseParameters)
               ? ModuleDSP::setBaseParameter(parameterIndex, value)
               : ModuleDSP::setEffectParameter(effectSpecificParameterIndex(parameterIndex), value);
}

namespace Engine
{
/// \note This used to ask the module whether it still had an editor region, and
/// destroy it first -- which, when the reference that reached zero was the *audio
/// thread's*, meant `destroyGUI()` allocating a message and posting it to the
/// message queue from inside the audio callback. The chain deliberately holds a
/// reference per node while it walks it (moduleChainImpl.hpp), so that is not a
/// hypothetical: removing a module mid-block is exactly when it happened.
///
///   A region holds a counted reference of its own now, so a module with one
/// drawn simply does not reach zero, and by the time it does there is nothing to
/// take down.
///                                           (02.08.2026.) (SW port)
void LE_NOINLINE intrusive_ptr_release_deleter(ModuleNode const *LE_RESTRICT const pModuleNode)
{
    auto const &module(actualModule<Module>(*pModuleNode));
    LE_ASSERT(module.referenceCount_ == 0);
    ModuleFactory::destroy(module);
}
} // namespace Engine

} // namespace LE::SW
