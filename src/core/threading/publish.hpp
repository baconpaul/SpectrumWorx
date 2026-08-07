////////////////////////////////////////////////////////////////////////////////
///
/// \file publish.hpp
/// -----------------
///
///   Structural change, from the side that asks for it.
///
///   Building a module or a chain allocates, reads files and formats strings, so
/// it happens on the main thread. Installing one is pointer surgery, so it
/// happens wherever the engine is -- here and now if nothing is processing, and
/// at the top of the next block if something is. These four functions are that
/// choice, made once, so that no caller has to know which case it is in.
///
///   The other direction is `ToUI::Retire`: whatever the audio thread displaced
/// comes back for the main thread to destroy. Nothing is ever destroyed under the
/// callback.
///
/// See doc/tech/threading_model.md §5.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef publish_hpp__6B1F0D42_9A57_4E38_B0C6_2D74E15F8A93
#define publish_hpp__6B1F0D42_9A57_4E38_B0C6_2D74E15F8A93
//------------------------------------------------------------------------------
#include "core/threading/messages.hpp"

#include <cstdint>

namespace LE::SW
{

class AutomatedModuleChain;
class Module;
class SpectrumWorxCore;

namespace Threading
{

////////////////////////////////////////////////////////////////////////////////
///
/// \brief Builds a module for \p effectIndex, sized against the current setup.
///
/// \note `[main-thread]`, and it allocates -- which is the whole reason the
/// audio thread is handed the result rather than the request. The plan granted a
/// concession here (create the effect inside the callback under
/// `SST_CPPUTILS_RTSAN_DISABLE`) and it turned out not to be needed: the preset
/// loader already built its modules this way, and a slot change is the same
/// operation with one module instead of five.
///
/// \return one reference, transferred; null when the effect is not in this build.
///
////////////////////////////////////////////////////////////////////////////////

Module *createModuleForSlot(SpectrumWorxCore &, std::int8_t effectIndex, std::uint8_t slot);

/// \brief Puts \p pModule (one reference, transferred; null empties) in \p slot.
///
/// \note Destroys the displaced module itself when it applied the change
/// directly, and hands it to the retire ring when the audio thread did.
void publishSlot(SpectrumWorxCore &, ToEngineQueue &, std::uint8_t slot, std::int8_t effectIndex,
                 Module *pModule);

/// \brief Moves the module in slot \p from to slot \p to.
void publishModuleMove(SpectrumWorxCore &, ToEngineQueue &, std::uint8_t from, std::uint8_t to);

/// \brief Installs \p newChain, whose modules are already built and sized.
///
/// \note Takes it by reference and leaves it empty, rather than by value: a
/// chain is an intrusive list and its nodes point back at their root, so it has
/// to be moved rather than copied and the caller keeps the object.
void publishChain(SpectrumWorxCore &, ToEngineQueue &, AutomatedModuleChain &newChain);

} // namespace Threading

} // namespace LE::SW

#endif // publish_hpp
