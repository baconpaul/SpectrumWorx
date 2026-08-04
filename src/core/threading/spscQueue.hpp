////////////////////////////////////////////////////////////////////////////////
///
/// \file spscQueue.hpp
/// -------------------
///
///   One producer, one consumer, no allocation, no lock.
///
/// \note Generalised from `SpectrumWorxCLAP::UIEdits`, which was already this and
/// already correct -- inline storage, relaxed/acquire/release written properly --
/// and which is now one instantiation of it.
///
///   Not `sst::cpputils::SimpleRingBuffer`, whose `push` overwrites the unread
/// tail when full. That is the right policy for a stream of samples and the wrong
/// one for commands: `SetSlot{2, Gain}` followed by `ClearSlot{2}` does not
/// coalesce to the second, and a dropped `Retire{ptr}` is a leak. This refuses
/// instead, and says so, so a caller can decide.
///
/// See doc/tech/threading_model.md.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef spscQueue_hpp__4A7C0E62_1B95_4D38_A0F6_82C3B7E5D419
#define spscQueue_hpp__4A7C0E62_1B95_4D38_A0F6_82C3B7E5D419
//------------------------------------------------------------------------------
#include <array>
#include <atomic>
#include <cstddef>
#include <type_traits>
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
/// \class SPSCQueue
///
/// \brief A bounded single-producer single-consumer queue.
///
/// \tparam Capacity a power of two, so the mask is an and.
///
/// \note The counters are free-running rather than masked, which is what makes
/// "full" and "empty" distinguishable without wasting a slot, and what makes the
/// unsigned wrap at 2^64 harmless -- the difference is still right across it.
///
////////////////////////////////////////////////////////////////////////////////

template <class Message, std::size_t Capacity> class SPSCQueue
{
    static_assert(Capacity && ((Capacity & (Capacity - 1)) == 0),
                  "Capacity must be a power of two");
    static_assert(std::is_trivially_copyable_v<Message>,
                  "A message crosses threads by value; give it no ownership.");

  public:
    using value_type = Message;

    static constexpr std::size_t capacity{Capacity};

    /// \brief Producer side. False when the queue is full and \p message was
    /// **not** taken.
    bool push(Message const &message)
    {
        auto const written(written_.load(std::memory_order_relaxed));
        /// \note The consumer only ever advances read_, so a stale value here
        /// makes the queue look fuller than it is -- never emptier.
        if ((written - read_.load(std::memory_order_acquire)) >= Capacity)
            return false;
        messages_[written & mask] = message;
        written_.store(written + 1, std::memory_order_release);
        return true;
    }

    /// \brief Consumer side. False when there was nothing, leaving \p message
    /// untouched.
    bool pop(Message &message)
    {
        auto const read(read_.load(std::memory_order_relaxed));
        if (read == written_.load(std::memory_order_acquire))
            return false;
        message = messages_[read & mask];
        read_.store(read + 1, std::memory_order_release);
        return true;
    }

    /// \brief How many messages are waiting. Exact on the consumer, an
    /// underestimate on the producer, which is the safe direction for both.
    std::size_t size() const
    {
        return written_.load(std::memory_order_acquire) - read_.load(std::memory_order_acquire);
    }

    bool empty() const { return size() == 0; }

  private:
    static constexpr std::size_t mask{Capacity - 1};

    static_assert(std::atomic<std::size_t>::is_always_lock_free,
                  "The audio thread is one end of this.");

    std::array<Message, Capacity> messages_{};
    std::atomic<std::size_t> written_{0};
    std::atomic<std::size_t> read_{0};
}; // class SPSCQueue

//------------------------------------------------------------------------------
} // namespace Threading
//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // spscQueue_hpp
