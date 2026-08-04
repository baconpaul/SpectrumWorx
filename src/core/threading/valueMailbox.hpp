////////////////////////////////////////////////////////////////////////////////
///
/// \file valueMailbox.hpp
/// ----------------------
///
///   The other half of the protocol: a slot per parameter that the newest write
/// overwrites, for values that are sampled rather than latched.
///
///   `Processor::preProcess()` runs once per host block and writes every enabled
/// LFO's target, so at 48 kHz with a 32 sample buffer that is 1500 updates a
/// second per enabled LFO -- up to 120,000 for a full rack -- against an interface
/// that draws at 30 Hz. Through a queue, 999 of every 1000 of those would be
/// delivered to be discarded, and a queue deep enough not to drop them would be
/// deep enough to hand the interface a second of stale sweep. A mailbox cannot
/// overflow and coalesces by construction.
///
///   Commands do not go here. `SetSlot{2, Gain}` then `ClearSlot{2}` does not
/// coalesce to the second one, and a dropped `Retire{ptr}` is a leak; those are
/// the ring's, in spscQueue.hpp.
///
/// See doc/tech/threading_model.md §3.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef valueMailbox_hpp__9E3D5B08_6C41_47A2_B85F_10D9A7362E4C
#define valueMailbox_hpp__9E3D5B08_6C41_47A2_B85F_10D9A7362E4C
//------------------------------------------------------------------------------
#include "core/host_interop/parameters.hpp"

#include <array>
#include <atomic>
#include <cstddef>
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
/// \class ValueMailbox
///
/// \brief The modulated value of every parameter, for painting.
///
///   Written by the audio thread, read by whoever draws. Indexed by the dense
/// parameter index -- `parameterIndexFromBinaryID()` -- rather than by the packed
/// ParameterID, which is not dense.
///
/// \note The **modulated** value, and never the base one. A base value is an event
/// with an order and a host that has to hear about it; it goes through the ring.
/// What is here is what the DSP is doing right now, which is a signal.
///
////////////////////////////////////////////////////////////////////////////////

class ValueMailbox
{
  public:
    static constexpr std::size_t capacity{ParameterCounts::maxNumberOfParameters};

    /// \brief Audio thread. Two relaxed stores and one read-modify-write.
    void set(std::size_t const index, float const value)
    {
        if (index >= capacity) [[unlikely]]
            return;
        values_[index].store(value, std::memory_order_relaxed);
        /// \note Release, and acquire on the reading side, so that a reader which
        /// sees the bit also sees the value that goes with it.
        dirty_[index / wordBits].fetch_or(bit(index), std::memory_order_release);
    }

    /// \brief Any thread. The value as of the last write, whatever else is going on.
    float value(std::size_t const index) const
    {
        if (index >= capacity) [[unlikely]]
            return 0;
        return values_[index].load(std::memory_order_relaxed);
    }

    /// \brief Reader. Calls \p f(index, value) for every parameter written since
    /// the last sweep, and clears what it reports.
    ///
    /// \note Clears by exchange rather than by clearing after reading, so a write
    /// that lands mid-sweep is carried into the next one instead of being lost.
    ///
    /// \note `const`, with the dirty words `mutable`. The reader is handed a
    /// `const &` on purpose -- it must not be able to write a *value* -- and
    /// taking one's own notification is not a change to what the mailbox says.
    template <class Functor> void forEachChanged(Functor &&f) const
    {
        for (std::size_t word(0); word < words; ++word)
        {
            auto bits(dirty_[word].exchange(0, std::memory_order_acquire));
            while (bits)
            {
                auto const offset(static_cast<std::size_t>(countTrailingZeros(bits)));
                bits &= bits - 1;
                auto const index((word * wordBits) + offset);
                if (index < capacity)
                    f(index, values_[index].load(std::memory_order_relaxed));
            }
        }
    }

    /// \brief Drops whatever has not been swept. For a reader that has just
    /// resynchronised from the model and does not want the backlog.
    void discardChanges() const
    {
        for (auto &word : dirty_)
            word.store(0, std::memory_order_relaxed);
    }

  private:
    using Word = std::uint64_t;

    static constexpr std::size_t wordBits{64};
    static constexpr std::size_t words{(capacity + wordBits - 1) / wordBits};

    static constexpr Word bit(std::size_t const index) { return Word{1} << (index % wordBits); }

    /// \note Not `std::countr_zero`: <bit> is a heavier include than this needs
    /// and the loop above is the only caller.
    static constexpr int countTrailingZeros(Word bits)
    {
        int count(0);
        while ((bits & 1) == 0)
        {
            bits >>= 1;
            ++count;
        }
        return count;
    }

    static_assert(std::atomic<float>::is_always_lock_free,
                  "The audio thread writes these every block.");
    static_assert(std::atomic<Word>::is_always_lock_free, "...and the dirty words with them.");

    std::array<std::atomic<float>, capacity> values_{};
    /// Mutable: see forEachChanged().
    mutable std::array<std::atomic<Word>, words> dirty_{};
}; // class ValueMailbox

//------------------------------------------------------------------------------
} // namespace Threading
//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // valueMailbox_hpp
