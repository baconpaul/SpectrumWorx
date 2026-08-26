////////////////////////////////////////////////////////////////////////////////
///
/// \file undoHistory.cpp
/// ---------------------
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "undoHistory.hpp"

#include <utility>
//------------------------------------------------------------------------------
namespace LE::SW
{

namespace
{
/// \brief Whether \p record continues what \p top already remembers, rather than
/// being a new thing the user did.
///
/// \note Deltas only. Two module edits in quick succession are two things, and a
/// snapshot carries no parameter to compare anyway.
bool continues(UndoHistory::Record const &record, UndoHistory::Step const &top,
               UndoHistory::Clock::duration const since)
{
    if (since >= UndoHistory::coalescingWindow)
        return false;

    auto const *const pDelta(std::get_if<UndoHistory::ValueDelta>(&record));
    auto const *const pTop(std::get_if<UndoHistory::ValueDelta>(&top.record));
    return pDelta && pTop && (pDelta->id.binaryValue == pTop->id.binaryValue);
}

/// \note Oldest first, so the end that grows is the end that is read.
void pushDropping(std::deque<UndoHistory::Entry> &stack, UndoHistory::Entry entry)
{
    stack.push_back(std::move(entry));
    if (stack.size() > UndoHistory::depth)
        stack.pop_front();
}
} // namespace

////////////////////////////////////////////////////////////////////////////////
///
/// \note The coalesced edit is dropped rather than merged, and the timestamp of
/// the step it joined is deliberately *not* refreshed. A wheel held down for
/// several seconds therefore becomes one step per window rather than one step
/// for the whole spin, which is the more predictable of the two: how far a
/// single undo goes back stays bounded by something the user can feel.
///
////////////////////////////////////////////////////////////////////////////////

void UndoHistory::record(char const *const name, Record record, Clock::time_point const when)
{
    // whatever was ahead of us is no longer reachable: it describes a state the
    // user has now edited away from
    redo_.clear();

    if (!undo_.empty() && continues(record, undo_.back().step, when - undo_.back().at))
        return;

    pushDropping(undo_, Entry{Step{name, std::move(record)}, when});
}

char const *UndoHistory::undoName() const noexcept
{
    return undo_.empty() ? nullptr : undo_.back().step.name.c_str();
}

char const *UndoHistory::redoName() const noexcept
{
    return redo_.empty() ? nullptr : redo_.back().step.name.c_str();
}

UndoHistory::Step const *UndoHistory::peekUndo() const noexcept
{
    return undo_.empty() ? nullptr : &undo_.back().step;
}

UndoHistory::Step const *UndoHistory::peekRedo() const noexcept
{
    return redo_.empty() ? nullptr : &redo_.back().step;
}

namespace
{
/// \note The name travels with the step and the record is replaced: "Add module"
/// undone is "Add module" to redo, and what changes is only which state it goes
/// to. The timestamp travels too and is never read again -- coalescing is about
/// what the user is doing now, and nothing coalesces into a step that has been
/// through a stack.
void moveTop(std::deque<UndoHistory::Entry> &from, std::deque<UndoHistory::Entry> &to,
             UndoHistory::Record inverse)
{
    if (from.empty())
        return;

    auto entry(std::move(from.back()));
    from.pop_back();
    entry.step.record = std::move(inverse);
    pushDropping(to, std::move(entry));
}
} // namespace

void UndoHistory::commitUndo(Record inverse) { moveTop(undo_, redo_, std::move(inverse)); }

void UndoHistory::commitRedo(Record inverse) { moveTop(redo_, undo_, std::move(inverse)); }

void UndoHistory::clear() noexcept
{
    undo_.clear();
    redo_.clear();
}

//------------------------------------------------------------------------------
} // namespace LE::SW
//------------------------------------------------------------------------------
