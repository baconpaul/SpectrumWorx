////////////////////////////////////////////////////////////////////////////////
///
/// \file messages.hpp
/// ------------------
///
///   What crosses between the main thread and the audio thread, in both
/// directions, as values.
///
///   Every message is trivially copyable and owns nothing: a pointer in one is a
/// *transfer*, and which side is responsible after it lands is written on each
/// case below. That is the whole of the memory management -- there is no shared
/// ownership anywhere in the protocol, and nothing is deleted on the audio thread.
///
/// See doc/tech/correct_the_threading.md §3 and §5.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef messages_hpp__2F86A1D4_7E30_4B9C_A5D2_6C0F31E8B7A5
#define messages_hpp__2F86A1D4_7E30_4B9C_A5D2_6C0F31E8B7A5
//------------------------------------------------------------------------------
#include "core/threading/spscQueue.hpp"

#include <cstdint>
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace SW
{
//------------------------------------------------------------------------------
namespace Threading
{
//------------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
///
/// \struct ToEngine
///
/// \brief Main thread -> audio thread. Drained at the top of process().
///
////////////////////////////////////////////////////////////////////////////////

struct ToEngine
{
    enum struct Kind : std::uint8_t
    {
        None,
        /// A parameter's **base** value, in the parameter's own units, snapped by
        /// the sender -- snapping is a pure function of the static ParameterInfo,
        /// so the interface can do it without waiting for an answer.
        SetBaseParameter,
        /// Which effect a slot holds. The audio thread builds it, which allocates;
        /// a granted concession, recorded in tech_debt.md.
        SetSlot,
        /// A whole Program, built on the main thread. The audio thread swaps it in
        /// and hands the old one back as ToUI::Retire -- it is never destroyed
        /// under the callback.
        SwapProgram,
        /// Likewise a decoded Sample.
        SwapSample
    };

    Kind kind{Kind::None};

    union
    {
        struct
        {
            std::uint32_t parameterID; ///< packed, i.e. SW::ParameterID::binaryValue
            float value;
        } setBaseParameter;

        struct
        {
            std::uint8_t slot;
            std::int8_t effectIndex; ///< -1 empties the slot
        } setSlot;

        /// \note `void *` rather than the real type: this header is the protocol
        /// and has no business including the engine. The two sites that put a
        /// pointer in and take it out are one file apart and both static_cast.
        struct
        {
            void *pProgram;
        } swapProgram;

        struct
        {
            void *pSample;
        } swapSample;
    };
}; // struct ToEngine

////////////////////////////////////////////////////////////////////////////////
///
/// \struct ToUI
///
/// \brief Audio thread -> main thread. Drained on the main thread.
///
/// \note Not the modulated value of a parameter: that is a signal at block rate
/// and goes through the ValueMailbox. What is here is what has *happened*.
///
////////////////////////////////////////////////////////////////////////////////

struct ToUI
{
    enum struct Kind : std::uint8_t
    {
        None,
        /// A base value the engine settled on, after snapping or after a host
        /// automation event. Latchable: the model takes it and the host is told.
        BaseParameterChanged,
        /// A slot's effect changed, so that slot's parameters are now different
        /// ones with different names. The interface rebuilds its strip.
        SlotChanged,
        /// Something the audio thread unlinked and the main thread must delete.
        /// Dropping one of these is a leak, which is why this is a ring and not a
        /// mailbox.
        Retire,
        /// The spectral setup changed, so every Hz-quantised range moved.
        SetupChanged
    };

    /// What a Retire is carrying, since the main thread has to know what to call.
    enum struct Retired : std::uint8_t
    {
        None,
        Program,
        Sample
    };

    Kind kind{Kind::None};

    union
    {
        struct
        {
            std::uint32_t parameterID;
            float value;
        } baseParameterChanged;

        struct
        {
            std::uint8_t slot;
            std::int8_t effectIndex;
        } slotChanged;

        struct
        {
            Retired what;
            void *pObject;
        } retire;
    };
}; // struct ToUI

////////////////////////////////////////////////////////////////////////////////
// The two rings.
//
// \note 1024 each, which is what UIEdits carried and what a block's worth of host
// automation has never come close to. They refuse rather than overwrite when full;
// see spscQueue.hpp for why that is the right way round for commands.
////////////////////////////////////////////////////////////////////////////////

using ToEngineQueue = SPSCQueue<ToEngine, 1024>;
using ToUIQueue = SPSCQueue<ToUI, 1024>;

//------------------------------------------------------------------------------
} // namespace Threading
//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // messages_hpp
