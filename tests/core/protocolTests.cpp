////////////////////////////////////////////////////////////////////////////////
///
/// protocolTests.cpp
/// -----------------
///
///   The two transports the redesign is built on, exercised on their own before
/// anything depends on them.
///
///   Both are lock free and neither can be reasoned about by inspection under
/// contention, so each has a case that actually runs two threads and one that
/// pins the property in the single-threaded case where the answer is exact.
///
/// See doc/tech/correct_the_threading.md §3.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "core/threading/messages.hpp"
#include "core/threading/spscQueue.hpp"
#include "core/threading/valueMailbox.hpp"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>
//------------------------------------------------------------------------------
namespace
{
namespace Threading = LE::SW::Threading;

using SmallQueue = Threading::SPSCQueue<int, 4>;
} // anonymous namespace

////////////////////////////////////////////////////////////////////////////////
// The ring
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("An empty queue pops nothing and leaves the destination alone", "[core][protocol]")
{
    SmallQueue queue;
    CHECK(queue.empty());
    CHECK(queue.size() == 0);

    int destination(-1);
    CHECK(!queue.pop(destination));
    CHECK(destination == -1);
}

TEST_CASE("A full queue refuses rather than overwrites", "[core][protocol]")
{
    // The property the whole design rests on. sst::cpputils::SimpleRingBuffer
    // clobbers the unread tail instead, which is right for a stream of samples
    // and wrong for commands: a dropped Retire is a leak, and SetSlot{2,Gain}
    // followed by ClearSlot{2} does not coalesce to the second.
    SmallQueue queue;
    for (int value(0); value < 4; ++value)
        CHECK(queue.push(value));

    CHECK(queue.size() == SmallQueue::capacity);
    CHECK(!queue.push(99));

    // ...and nothing that was already in it moved.
    for (int expected(0); expected < 4; ++expected)
    {
        int value(-1);
        REQUIRE(queue.pop(value));
        CHECK(value == expected);
    }
    CHECK(queue.empty());
}

TEST_CASE("The queue keeps working past the end of its storage", "[core][protocol]")
{
    // Free-running counters, masked only on the way into the array: a queue that
    // wrapped its counters would confuse full with empty every `capacity` pushes.
    SmallQueue queue;
    for (int round(0); round < 100; ++round)
    {
        REQUIRE(queue.push(round));
        int value(-1);
        REQUIRE(queue.pop(value));
        CHECK(value == round);
    }
    CHECK(queue.empty());
}

TEST_CASE("Everything pushed arrives, once and in order, across two threads",
          "[core][protocol][threads]")
{
    // Small on purpose: a four-slot queue and 100k messages means the producer
    // meets a full queue constantly, which is the interleaving worth testing.
    constexpr int messages{100'000};
    Threading::SPSCQueue<int, 8> queue;

    std::atomic<bool> producerDone{false};
    std::thread producer([&] {
        for (int value(0); value < messages; ++value)
            while (!queue.push(value))
                std::this_thread::yield();
        producerDone.store(true, std::memory_order_release);
    });

    int expected(0);
    while (expected < messages)
    {
        int value(-1);
        if (queue.pop(value))
        {
            REQUIRE(value == expected); // once, and in order
            ++expected;
        }
        else
        {
            std::this_thread::yield();
        }
    }

    producer.join();
    CHECK(producerDone.load());
    CHECK(queue.empty());
}

TEST_CASE("The messages the protocol carries fit through it", "[core][protocol]")
{
    // Trivially copyable is a static_assert on the queue; that the union members
    // survive the round trip is this.
    int module{0};
    Threading::ToEngineQueue toEngine;
    REQUIRE(toEngine.push(Threading::setSlot(3, 17, &module)));

    Threading::ToEngine received{};
    REQUIRE(toEngine.pop(received));
    CHECK(received.kind == Threading::ToEngine::Kind::SetSlot);
    CHECK(received.setSlot.slot == 3);
    CHECK(received.setSlot.effectIndex == 17);
    CHECK(received.setSlot.pModule == &module);

    Threading::ToUIQueue toUI;
    int retirable{0};
    REQUIRE(toUI.push(Threading::retire(Threading::ToUI::Retired::Chain, &retirable)));

    Threading::ToUI back{};
    REQUIRE(toUI.pop(back));
    CHECK(back.kind == Threading::ToUI::Kind::Retire);
    CHECK(back.retire.what == Threading::ToUI::Retired::Chain);
    CHECK(back.retire.pObject == &retirable);
}

////////////////////////////////////////////////////////////////////////////////
// The mailbox
////////////////////////////////////////////////////////////////////////////////

namespace
{
std::vector<std::pair<std::size_t, float>> sweep(Threading::ValueMailbox &mailbox)
{
    std::vector<std::pair<std::size_t, float>> changed;
    mailbox.forEachChanged(
        [&](std::size_t const index, float const value) { changed.emplace_back(index, value); });
    return changed;
}
} // anonymous namespace

TEST_CASE("A mailbox reports only what changed, and only once", "[core][protocol]")
{
    Threading::ValueMailbox mailbox;
    CHECK(sweep(mailbox).empty());

    mailbox.set(0, 1.5f);
    mailbox.set(200, -2.5f);

    auto const changed(sweep(mailbox));
    REQUIRE(changed.size() == 2);
    CHECK(changed[0] == std::pair<std::size_t, float>{0, 1.5f});
    CHECK(changed[1] == std::pair<std::size_t, float>{200, -2.5f});

    // Reported once. A repaint is not owed a second time for the same write.
    CHECK(sweep(mailbox).empty());

    // ...but the value is still readable, which is what makes this a mailbox
    // rather than a queue: a reader that missed the sweep can still ask.
    CHECK(mailbox.value(0) == 1.5f);
    CHECK(mailbox.value(200) == -2.5f);
}

TEST_CASE("A mailbox coalesces: the newest write is the one delivered", "[core][protocol]")
{
    // The reason this is not a queue. preProcess() writes every enabled LFO's
    // target once per host block -- up to 1500 times a second per LFO at a 32
    // sample buffer -- against an interface that draws at 30 Hz.
    Threading::ValueMailbox mailbox;
    for (int write(0); write < 10'000; ++write)
        mailbox.set(7, static_cast<float>(write));

    auto const changed(sweep(mailbox));
    REQUIRE(changed.size() == 1);
    CHECK(changed[0].first == 7);
    CHECK(changed[0].second == 9999.0f);
}

TEST_CASE("A mailbox write out of range is ignored rather than corrupting", "[core][protocol]")
{
    Threading::ValueMailbox mailbox;
    mailbox.set(Threading::ValueMailbox::capacity, 1.0f);
    mailbox.set(Threading::ValueMailbox::capacity + 1000, 1.0f);
    CHECK(sweep(mailbox).empty());
    CHECK(mailbox.value(Threading::ValueMailbox::capacity) == 0.0f);
}

TEST_CASE("A mailbox sweep never loses a write that lands during it", "[core][protocol][threads]")
{
    // The writer runs flat out while the reader sweeps. What must hold is that
    // the reader ends up with the last value written -- a sweep that cleared the
    // dirty bits *after* reading would drop whatever arrived in between.
    Threading::ValueMailbox mailbox;
    constexpr std::size_t index{42};
    constexpr int writes{200'000};

    std::atomic<bool> stop{false};
    std::thread writer([&] {
        for (int write(1); write <= writes; ++write)
            mailbox.set(index, static_cast<float>(write));
        stop.store(true, std::memory_order_release);
    });

    float lastSeen(0);
    while (!stop.load(std::memory_order_acquire))
        mailbox.forEachChanged([&](std::size_t, float const value) { lastSeen = value; });

    writer.join();

    // One last sweep, for whatever landed after the flag.
    mailbox.forEachChanged([&](std::size_t, float const value) { lastSeen = value; });

    CHECK(lastSeen == static_cast<float>(writes));
    CHECK(mailbox.value(index) == static_cast<float>(writes));
}
