////////////////////////////////////////////////////////////////////////////////
///
/// undoHistoryTests.cpp
/// --------------------
///
///   The two stacks on their own, with no plugin under them. Everything here is
/// about *bookkeeping* -- what is undoable, in what order, and which edits count
/// as one thing -- which is separable from applying a step and is where the
/// arithmetic errors live.
///
/// \note Time is a parameter rather than a reading, so the coalescing window can
/// be crossed without waiting for it. \see issue #101.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "undoHistory.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
//------------------------------------------------------------------------------
namespace
{
using LE::SW::ParameterID;
using LE::SW::UndoHistory;

using Clock = UndoHistory::Clock;

ParameterID parameter(std::uint32_t const binaryValue)
{
    ParameterID id;
    id.binaryValue = binaryValue;
    return id;
}

UndoHistory::ValueDelta delta(std::uint32_t const id, float const value)
{
    return {parameter(id), value};
}

/// A moment \p milliseconds after an arbitrary origin.
Clock::time_point at(int const milliseconds)
{
    return Clock::time_point{} + std::chrono::milliseconds(milliseconds);
}

//------------------------------------------------------------------------------
} // anonymous namespace
//------------------------------------------------------------------------------

TEST_CASE("A fresh history has nothing to undo or redo", "[core][undo]")
{
    UndoHistory history;

    CHECK_FALSE(history.canUndo());
    CHECK_FALSE(history.canRedo());
    CHECK(history.undoName() == nullptr);
    CHECK(history.redoName() == nullptr);
    CHECK(history.peekUndo() == nullptr);
}

TEST_CASE("A recorded step is undoable and carries its name", "[core][undo]")
{
    UndoHistory history;
    history.record("Add module", UndoHistory::StateSnapshot{"before"}, at(0));

    REQUIRE(history.canUndo());
    CHECK_FALSE(history.canRedo());
    CHECK(std::string(history.undoName()) == "Add module");

    auto const *const pStep(history.peekUndo());
    REQUIRE(pStep != nullptr);
    auto const *const pSnapshot(std::get_if<UndoHistory::StateSnapshot>(&pStep->record));
    REQUIRE(pSnapshot != nullptr);
    CHECK(pSnapshot->state == "before");
}

TEST_CASE("Undoing a step makes it redoable, carrying what it replaced", "[core][undo]")
{
    UndoHistory history;
    history.record("Add module", UndoHistory::StateSnapshot{"before"}, at(0));

    // what the caller had to overwrite in order to apply the step
    history.commitUndo(UndoHistory::StateSnapshot{"after"});

    CHECK_FALSE(history.canUndo());
    REQUIRE(history.canRedo());
    CHECK(std::string(history.redoName()) == "Add module");

    auto const *const pStep(history.peekRedo());
    REQUIRE(pStep != nullptr);
    auto const *const pSnapshot(std::get_if<UndoHistory::StateSnapshot>(&pStep->record));
    REQUIRE(pSnapshot != nullptr);
    CHECK(pSnapshot->state == "after");
}

TEST_CASE("Redoing a step puts it back on the undo stack", "[core][undo]")
{
    UndoHistory history;
    history.record("Add module", UndoHistory::StateSnapshot{"before"}, at(0));
    history.commitUndo(UndoHistory::StateSnapshot{"after"});
    history.commitRedo(UndoHistory::StateSnapshot{"before"});

    REQUIRE(history.canUndo());
    CHECK_FALSE(history.canRedo());
    CHECK(std::string(history.undoName()) == "Add module");
}

TEST_CASE("A new step discards anything that was redoable", "[core][undo]")
{
    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \note The ordinary rule, and worth pinning because the alternative is a
    /// redo that reapplies a state the user has since edited away from -- which
    /// is not "forward" in any sense they could predict.
    ///
    ////////////////////////////////////////////////////////////////////////////
    UndoHistory history;
    history.record("Add module", UndoHistory::StateSnapshot{"one"}, at(0));
    history.commitUndo(UndoHistory::StateSnapshot{"two"});
    REQUIRE(history.canRedo());

    history.record("Remove module", UndoHistory::StateSnapshot{"three"}, at(1000));

    CHECK(history.canUndo());
    CHECK_FALSE(history.canRedo());
    CHECK(history.redoName() == nullptr);
}

TEST_CASE("One parameter edited twice inside the window is one step", "[core][undo]")
{
    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \note A mouse wheel is the reason. Each notch is a whole edit and reports
    /// itself as one, so a spun wheel would otherwise be a step per notch and
    /// an undo would move the value by one notch.
    ///
    /// \note The step that survives is the *first*, because what it holds is the
    /// value to go back to -- the one from before the spin started.
    ///
    ////////////////////////////////////////////////////////////////////////////
    UndoHistory history;
    history.record("Gain", delta(7, 0.25f), at(0));
    history.record("Gain", delta(7, 0.50f), at(200));
    history.record("Gain", delta(7, 0.75f), at(400));

    REQUIRE(history.canUndo());

    auto const *const pStep(history.peekUndo());
    REQUIRE(pStep != nullptr);
    auto const *const pDelta(std::get_if<UndoHistory::ValueDelta>(&pStep->record));
    REQUIRE(pDelta != nullptr);
    CHECK(pDelta->value == 0.25f);

    // and it really is one step: undoing it empties the history
    history.commitUndo(delta(7, 0.75f));
    CHECK_FALSE(history.canUndo());
}

TEST_CASE("One parameter edited again after the window is a second step", "[core][undo]")
{
    UndoHistory history;

    auto const justPastTheWindow(at(0) + UndoHistory::coalescingWindow +
                                 std::chrono::milliseconds(1));

    history.record("Gain", delta(7, 0.25f), at(0));
    history.record("Gain", delta(7, 0.50f), justPastTheWindow);

    history.commitUndo(delta(7, 0.75f));
    CHECK(history.canUndo());
}

TEST_CASE("Two different parameters are never one step", "[core][undo]")
{
    UndoHistory history;
    history.record("Gain", delta(7, 0.25f), at(0));
    history.record("Mix", delta(8, 0.5f), at(1));

    history.commitUndo(delta(8, 0.9f));
    REQUIRE(history.canUndo());
    CHECK(std::string(history.undoName()) == "Gain");
}

TEST_CASE("A snapshot never coalesces into the step before it", "[core][undo]")
{
    // two module edits in quick succession are two things the user did
    UndoHistory history;
    history.record("Add module", UndoHistory::StateSnapshot{"one"}, at(0));
    history.record("Add module", UndoHistory::StateSnapshot{"two"}, at(1));

    history.commitUndo(UndoHistory::StateSnapshot{"three"});
    CHECK(history.canUndo());
}

TEST_CASE("The history drops its oldest step once it is full", "[core][undo]")
{
    UndoHistory history;

    // one more than it holds, each far enough apart not to coalesce
    for (std::size_t step(0); step <= UndoHistory::depth; ++step)
        history.record("Gain", delta(static_cast<std::uint32_t>(step), 0.0f),
                       at(static_cast<int>(step) * 1000));

    std::size_t undone(0);
    while (history.canUndo())
    {
        history.commitUndo(delta(0, 0.0f));
        ++undone;
    }
    CHECK(undone == UndoHistory::depth);
}

TEST_CASE("Clearing leaves nothing on either stack", "[core][undo]")
{
    UndoHistory history;
    history.record("Add module", UndoHistory::StateSnapshot{"one"}, at(0));
    history.commitUndo(UndoHistory::StateSnapshot{"two"});
    REQUIRE(history.canRedo());

    history.clear();

    CHECK_FALSE(history.canUndo());
    CHECK_FALSE(history.canRedo());
}
