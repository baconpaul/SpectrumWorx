////////////////////////////////////////////////////////////////////////////////
///
/// publish.cpp
/// -----------
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "publish.hpp"

#include "core/automatedModuleChain.hpp"
#include "core/modules/moduleDSPAndGUI.hpp"
#include "core/spectrumWorxCore.hpp"

#include "le/utility/assert.hpp"

namespace LE::SW::Threading
{
namespace
{
void releaseModule(Module *const pModule)
{
    if (pModule)
        intrusive_ptr_release(&Engine::node(*pModule));
}
} // anonymous namespace

Module *createModuleForSlot(SpectrumWorxCore &core, std::int8_t const effectIndex,
                            std::uint8_t const slot)
{
    if (effectIndex == noModule)
        return nullptr;

    auto pModule(ModuleFactory::create<Module>(effectIndex));
    if (!pModule)
        return nullptr;

    /// \note The DSP initialiser and only that. A module has no interface of its
    /// own any more -- stage 5 -- so there is nothing else to give it, and the
    /// editor builds its strip when the chain reports the change.
    if (!core.moduleInitialiser()(*pModule, slot))
        return nullptr;

    return pModule.detach();
}

void publishSlot(SpectrumWorxCore &core, ToEngineQueue &toEngine, std::uint8_t const slot,
                 std::int8_t const effectIndex, Module *const pModule)
{
    if (!core.engineIsRunning())
    {
        releaseModule(core.installModuleInSlot(slot, pModule));
        return;
    }

    if (toEngine.push(setSlot(slot, effectIndex, pModule)))
        return;

    /// \note A full command ring drops the request, and the module built for it
    /// has to go with it -- the alternative is a leak per dropped slot change.
    /// 1024 deep against a handful of clicks, so this is a "something is very
    /// wrong" path rather than a rate limit.
    LE_ASSERT_MSG(false, "The command queue is full; a slot change was dropped.");
    releaseModule(pModule);
}

void publishModuleMove(SpectrumWorxCore &core, ToEngineQueue &toEngine, std::uint8_t const from,
                       std::uint8_t const to)
{
    if (!core.engineIsRunning())
    {
        core.moveModule(from, to);
        return;
    }
    LE_ASSERT_MSG(toEngine.push(moveModule(from, to)),
                  "The command queue is full; a module move was dropped.");
}

void publishChain(SpectrumWorxCore &core, ToEngineQueue &toEngine, AutomatedModuleChain &newChain)
{
    if (!core.engineIsRunning())
    {
        /// \note The same exchange the audio thread would do, and then the
        /// caller's own chain holds what used to be live -- which its destructor
        /// releases. One code path, two threads, no special case.
        core.swapModuleChain(newChain);
        return;
    }

    /// \note Raw, and deliberately: the message *is* the owner from the push
    /// until the retire, and dressing that up as a smart pointer on one side of a
    /// ring that carries `void *` would only hide where the handover is.
    auto *const pOutgoing(new AutomatedModuleChain);
    pOutgoing->swap(newChain);

    if (toEngine.push(swapChain(pOutgoing)))
        return;

    LE_ASSERT_MSG(false, "The command queue is full; a chain change was dropped.");
    /// \note And the new chain goes back where it came from, so that a caller
    /// that meant to install it still owns what it built.
    pOutgoing->swap(newChain);
    delete pOutgoing;
}

} // namespace LE::SW::Threading
