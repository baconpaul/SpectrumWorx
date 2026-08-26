////////////////////////////////////////////////////////////////////////////////
///
/// \file undoHistory.hpp
/// ---------------------
///
///   What the user did, in the order they did it, so that it can be taken back.
///
///   Bookkeeping only. Nothing here applies a step or knows how: a step says
/// *what to put back* and the plugin knows how to put it. That split is what
/// lets the arithmetic -- what coalesces, what clears the redo stack, what falls
/// off the end -- be tested with no engine, no editor and no clock.
///
/// \note Three kinds of step, and which one an action gets is decided by the
/// gesture that opened it rather than by anything here. A parameter gesture
/// leaves a `ValueDelta`, which undoes as one parameter write. A chain edit --
/// a slot filled or emptied, a module moved -- leaves a `ChainEdit`, which
/// undoes as the handful of slot changes that put it back. A preset load leaves
/// a `StateSnapshot`, which undoes through the preset loader.
///
///   The snapshot is the general one, and the other two exist because the
/// general one is too expensive for what it would be spent on: it rebuilds the
/// whole module chain, which is right for a preset and wrong for everything
/// else. \see ChainEdit, which carries the reason in full.
///
/// \note Beside the plugin rather than under `core/`, which is the engine and
/// links no JUCE: this is what the *plugin* remembers, not what it computes.
///
/// \note `[main-thread]`, and nothing else. The audio thread never learns this
/// exists. \see doc/tech/threading_model.md, which has nothing to say about it
/// for that reason.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef undoHistory_hpp__0C7E4A19_63B5_4D2F_9E81_A5D3C7B04E62
#define undoHistory_hpp__0C7E4A19_63B5_4D2F_9E81_A5D3C7B04E62
//------------------------------------------------------------------------------
#include "core/parameterID.hpp"

#include <chrono>
#include <cstddef>
#include <deque>
#include <string>
#include <utility>
#include <variant>
#include <vector>
//------------------------------------------------------------------------------
namespace LE::SW
{

class UndoHistory
{
  public:
    using Clock = std::chrono::steady_clock;

    /// \brief How many steps are kept, oldest dropped.
    static constexpr std::size_t depth{64};

    /// \brief How close together two edits of one parameter have to be to count
    /// as one step. \see record().
    static constexpr std::chrono::milliseconds coalescingWindow{500};

    /// \brief One parameter, and the value to put back into it.
    struct ValueDelta
    {
        ParameterID id;
        float value;
    };

    /// \brief The whole program, serialised as a preset. \note A preset rather
    /// than a session: what a session carries and a preset does not is where the
    /// user was in the interface, which is not something undo has an opinion on.
    struct StateSnapshot
    {
        std::string state;
    };

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \brief What one slot held, and the values that went with it.
    ///
    /// \note The parameters are carried only where a module has to come *back*.
    /// Emptying a slot needs none of them; refilling one that was emptied needs
    /// all of them, because the module the chain builds is a new object with
    /// nothing in it.
    ///
    ////////////////////////////////////////////////////////////////////////////

    struct SlotState
    {
        std::uint8_t slot;
        std::int8_t effectIndex; ///< the chain's noModule empties it
        std::vector<std::pair<ParameterID, float>> parameters;
    };

    struct MoveModule
    {
        std::uint8_t from;
        std::uint8_t to;
    };

    using ChainCommand = std::variant<SlotState, MoveModule>;

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \brief What to do to the chain to put it back, in order.
    ///
    /// \note The reason this is not a `StateSnapshot`, which would say the same
    /// thing and be simpler: restoring a state goes through the preset loader,
    /// and a preset load builds a **whole new chain** and swaps it in
    /// (`Loader::publishChain`). Every module is then a new object, so every
    /// strip in the rack is destroyed and rebuilt -- and undoing one added
    /// module played the add-and-remove animation for all of them. A command
    /// touches the slot it is about and leaves the rest of the chain alone,
    /// which is what lets the rack animate the edit rather than the reload.
    ///
    ////////////////////////////////////////////////////////////////////////////

    struct ChainEdit
    {
        std::vector<ChainCommand> commands;
    };

    /// \note A preset load stays a snapshot. It really is a whole new chain, so
    /// there is nothing to be gained by describing it as a list of edits and a
    /// rack that redraws wholesale is the honest picture of what happened.
    using Record = std::variant<ValueDelta, ChainEdit, StateSnapshot>;

    struct Step
    {
        /// \note Owned, and taken at the moment the step was recorded. A chain
        /// edit's name is a literal ("Add module") but a parameter's is whatever
        /// the slot's effect called it, and the slot can hold something else by
        /// the time the step is read.
        std::string name;
        Record record;
    };

    /// \brief A step and when it was recorded. \note Public only so that this
    /// class's own implementation can be written as free functions over the two
    /// stacks; nothing outside builds one.
    struct Entry
    {
        Step step;
        Clock::time_point at;
    };

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \brief Remembers \p record as what \p name is about to change, and drops
    /// anything that was redoable.
    ///
    /// \note \p when is passed rather than read so that the coalescing window is
    /// testable. Two `ValueDelta`s for one parameter inside that window are one
    /// step, and the one that survives is the **first**: what a step holds is
    /// where to go back to, and that is the value from before the first of them.
    /// A mouse wheel is why this exists -- every notch reports itself as a whole
    /// edit, so a spun wheel is otherwise a step per notch.
    ///
    ////////////////////////////////////////////////////////////////////////////
    void record(char const *name, Record, Clock::time_point when);

    bool canUndo() const noexcept { return !undo_.empty(); }
    bool canRedo() const noexcept { return !redo_.empty(); }

    /// \brief What undo would take back, or null. For the control's caption.
    char const *undoName() const noexcept;
    char const *redoName() const noexcept;

    /// \brief The step to apply, or null. Does not remove it. \see commitUndo().
    Step const *peekUndo() const noexcept;
    Step const *peekRedo() const noexcept;

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \brief Moves the peeked step to the other stack, with \p inverse in place
    /// of what it held.
    ///
    /// \note Two calls rather than one because only the caller can say what
    /// \p inverse is: it is whatever applying the step overwrote, which it
    /// cannot know until it has looked at the step. Peek, apply, commit.
    ///
    ////////////////////////////////////////////////////////////////////////////
    void commitUndo(Record inverse);
    void commitRedo(Record inverse);

    void clear() noexcept;

  private:
    std::deque<Entry> undo_;
    std::deque<Entry> redo_;
}; // class UndoHistory

//------------------------------------------------------------------------------
} // namespace LE::SW
//------------------------------------------------------------------------------
#endif // undoHistory_hpp
