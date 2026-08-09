////////////////////////////////////////////////////////////////////////////////
///
/// \file publishProtocolTests.cpp
/// ------------------------------
///
///   That the two halves of publish-and-retire actually happen -- in every build
/// configuration, which is the part nothing checked.
///
///   `LE_ASSERT_MSG` expands to `static_cast<void>(0)` under NDEBUG
/// (`assert.hpp:54-57`), so an expression written inside one is not merely
/// unchecked in a release build, it is absent. Two ring pushes were written that
/// way -- `SpectrumWorxCLAP::retire()` and `Threading::publishModuleMove()` --
/// and each is the whole of what its function does. Every shipped binary
/// therefore leaked what the audio thread handed back and dropped every module
/// move on the floor, while the checked build the suite runs in did neither.
///
///   Which is why both cases here are written against observable state rather
/// than against the queues: a case that asserted `toEngine.pop()` would pin the
/// mechanism, and what went wrong is that the mechanism was not reached at all.
/// The engine's chain order and a module's reference count are what a user and a
/// leak detector respectively would see.
///
/// \note These fail in Release and pass in Debug before the fix, which is the
/// signature of this whole class of bug and the reason `checkNoAssertSideEffects`
/// exists beside them.
///
/// See doc/tech/threading_model.md §3 and §5.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "clap/testHost.hpp"

#include "core/modules/moduleDSPAndGUI.hpp"
#include "external_audio/sample.hpp"

#include "le/utility/intrusivePtr.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>
//------------------------------------------------------------------------------
namespace
{
//------------------------------------------------------------------------------

using namespace SWTest;

constexpr double sampleRate{48000};
constexpr std::uint32_t blockSize{512};

/// Two effects that are not each other, so that chain *order* is observable.
constexpr std::int8_t firstEffect{0};
constexpr std::int8_t secondEffect{1};

LE::SW::SpectrumWorxCLAP &implementationOf(clap_plugin const &plugin)
{
    auto *const pHelper(static_cast<LE::SW::PluginHelper *>(plugin.plugin_data));
    REQUIRE(pHelper != nullptr);
    return *static_cast<LE::SW::SpectrumWorxCLAP *>(pHelper);
}

/// \brief One block of silence, which is all these cases want from `process()`:
/// it is the only thing that drains the command ring.
void runOneBlock(ActivePlugin &plugin)
{
    std::vector<float> leftIn(blockSize, 0), rightIn(blockSize, 0);
    std::vector<float> leftOut(blockSize, 0), rightOut(blockSize, 0);
    plugin.process(leftIn, rightIn, leftOut, rightOut);
}

/// The effect in each slot of \p chain, in chain order.
template <class ModuleChain> std::vector<int> effectOrder(ModuleChain const &chain)
{
    std::vector<int> effects;
    for (std::uint8_t slot(0); slot < chain.size(); ++slot)
    {
        auto const pModule(chain.template moduleAs<LE::SW::Module>(slot));
        REQUIRE(pModule);
        effects.push_back(pModule->effectTypeIndex());
    }
    return effects;
}

//------------------------------------------------------------------------------
} // anonymous namespace
//------------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
///
/// \note The desync half. `editModuleMove()` does two things -- reorders
/// `programMain_`, which is what the rack draws and what `stateSave` writes, and
/// queues the same move for the engine. Only the first survived NDEBUG, so a
/// user dragging a module saw the rack reorder, saved a session that agreed with
/// the rack, and heard the old order for the life of the instance.
///
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Moving a module reaches the engine and not only the interface", "[clap][protocol]")
{
    Entry const entry;

    ActivePlugin plugin(sampleRate, blockSize);
    auto &host(editorHostOf(*plugin));
    auto &implementation(implementationOf(*plugin));

    REQUIRE(host.editSlot(0, firstEffect));
    REQUIRE(host.editSlot(1, secondEffect));

    // The slot changes are commands while the plugin is active, so nothing has
    // happened to the engine until a block does.
    runOneBlock(plugin);

    std::vector<int> const asBuilt{firstEffect, secondEffect};
    REQUIRE(effectOrder(host.programMain().moduleChain()) == asBuilt);
    REQUIRE(effectOrder(implementation.program().moduleChain()) == asBuilt);

    host.editModuleMove(0, 1);
    runOneBlock(plugin);

    std::vector<int> const asMoved{secondEffect, firstEffect};

    // The rack and the session state follow the drag...
    CHECK(effectOrder(host.programMain().moduleChain()) == asMoved);
    // ...and so does what is being heard.
    CHECK(effectOrder(implementation.program().moduleChain()) == asMoved);
}

////////////////////////////////////////////////////////////////////////////////
///
/// \note The leak half, and the reason it is measured on the reference count
/// rather than on the ring: `retire()`'s contract is that the audio thread hands
/// the displaced module back and the *main* thread frees it. A push that never
/// happens keeps the count at the value the handover left it, forever -- one
/// module per slot change, one whole chain per preset load, invisible to
/// everything except a leak detector nothing was pointing here.
///
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("A module the engine displaces is released rather than leaked", "[clap][protocol]")
{
    Entry const entry;

    ActivePlugin plugin(sampleRate, blockSize);
    auto &host(editorHostOf(*plugin));
    auto &implementation(implementationOf(*plugin));

    REQUIRE(host.editSlot(0, firstEffect));
    runOneBlock(plugin);
    REQUIRE(implementation.program().moduleChain().size() == 1);

    /// \note A reference of our own on the engine's module, so that the count is
    /// still readable after everything else has let go of it -- and so that the
    /// failure is a wrong count rather than a read of freed memory.
    LE::Utility::IntrusivePtr<LE::SW::Module> const pDisplaced(
        implementation.program().moduleChain().moduleAs<LE::SW::Module>(0));
    REQUIRE(pDisplaced);
    auto const &node(LE::SW::Engine::node(*pDisplaced));

    // Something else out of the same slot: the audio thread unlinks the module
    // above and hands it to the retire ring.
    REQUIRE(host.editSlot(0, secondEffect));
    runOneBlock(plugin);

    REQUIRE(implementation.program().moduleChain().size() == 1);
    REQUIRE(implementation.program().moduleChain().moduleAs<LE::SW::Module>(0).get() !=
            pDisplaced.get());
    REQUIRE(node.referenceCount_.load() > 1); // the handover's, and ours

    // The main thread's half of the protocol, which a host runs when the plugin
    // asks through `request_callback`.
    plugin.pumpMainThread();

    // Ours alone: the engine has genuinely let go of it.
    CHECK(node.referenceCount_.load() == 1);
}

////////////////////////////////////////////////////////////////////////////////
//
// What a full ring costs
// ----------------------
//
////////////////////////////////////////////////////////////////////////////////
///
/// \note Six of the seven pushes did not look at their result, so the answer to
/// "what happens when a ring fills" was "nobody knows and nothing says". These
/// cases make it an answer: the message is gone, the two copies have diverged,
/// and `droppedMessages()` is the plugin admitting it.
///
///   They deliberately assert the *divergence* as well as the count. The counter
/// is not a repair and pretending otherwise would be the same mistake in a
/// different place -- what is lost is lost, because the ring is where the
/// information to put it back would have been. A lossless echo would make this
/// unreachable, and is a bigger change than a bug fix.
///
////////////////////////////////////////////////////////////////////////////////

namespace
{
/// One more than either ring holds.
constexpr unsigned overflowing{LE::SW::Threading::ToEngineQueue::capacity + 1};

LE::SW::ParameterID globalParameterID(unsigned const index)
{
    return LE::SW::ParameterID{LE::Plugins::ParameterID{parameterID(globalType, index)}};
}
} // anonymous namespace

TEST_CASE("A full command ring is counted rather than ignored", "[clap][protocol][hostile]")
{
    Entry const entry;

    ActivePlugin plugin(sampleRate, blockSize);
    auto &host(editorHostOf(*plugin));
    auto &implementation(implementationOf(*plugin));

    REQUIRE(implementation.droppedMessages() == 0);

    /// \note No `process()` anywhere in here, which is what makes the ring fill:
    /// draining it is what a block does. An editor with a stuck audio thread in
    /// front of it is the real shape of this.
    for (unsigned edit(0); edit < overflowing; ++edit)
        host.editParameter(globalParameterID(0), 0.5f);

    CHECK(implementation.droppedMessages() > 0);
}

TEST_CASE("A full echo ring leaves the main thread's Program behind, and says so",
          "[clap][protocol][hostile]")
{
    Entry const entry;

    ActivePlugin plugin(sampleRate, blockSize);
    auto &implementation(implementationOf(*plugin));
    auto const &params(parameters(*plugin));

    auto const id(parameterID(globalType, 0));

    std::vector<float> leftIn(blockSize, 0), rightIn(blockSize, 0);
    std::vector<float> leftOut(blockSize, 0), rightOut(blockSize, 0);

    /// \note One echo per block, and nothing pumping the main thread -- which is
    /// a host that is slow to run the callback it was asked for, with automation
    /// moving. Every one of these is applied to the engine.
    for (unsigned block(0); block < overflowing; ++block)
    {
        OneParameterEvent const event(id, 0.25 + (0.5 * block) / overflowing);
        plugin.process(leftIn, rightIn, leftOut, rightOut, nullptr, &*event);
    }

    REQUIRE(implementation.droppedMessages() > 0);

    // Now let the main thread catch up with everything that did fit.
    plugin.pumpMainThread();

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \note And this is the cost, stated rather than implied: what the host
    /// reads back is not what the engine is running, and no further drain will
    /// fix it. Before this change the two simply differed with nothing recorded.
    ///
    ////////////////////////////////////////////////////////////////////////////
    double asTheHostSeesIt{-1};
    REQUIRE(params.get_value(&*plugin, id, &asTheHostSeesIt));

    auto const asTheEngineHasIt(implementation.program()
                                    .parameters()
                                    .get<LE::SW::GlobalParameters::InputGain>()
                                    .getValue());

    UNSCOPED_INFO("host reads " << asTheHostSeesIt << ", engine holds " << asTheEngineHasIt);
    CHECK(asTheHostSeesIt != Catch::Approx(asTheEngineHasIt));
}

TEST_CASE("A sample the engine never received is not recorded as loaded",
          "[clap][protocol][hostile]")
{
    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \note `sampleFile_` is the main thread's record of what the engine is
    /// playing and `stateSave` writes it. It was set at the top of
    /// `publishSample`, before the push that can fail -- so a dropped sample load
    /// left the session naming a file the engine never got, and reopening that
    /// session loaded it as though it had always been there.
    ///
    ////////////////////////////////////////////////////////////////////////////
    Entry const entry;

    ActivePlugin plugin(sampleRate, blockSize);
    auto &host(editorHostOf(*plugin));
    auto &implementation(implementationOf(*plugin));

    auto const samples(LE::Sample::factorySamples());
    REQUIRE(!samples.empty());

    REQUIRE(host.currentSampleFile() == juce::File());

    // Fill the ring so that the sample's own push cannot land.
    for (unsigned edit(0); edit < overflowing; ++edit)
        host.editParameter(globalParameterID(0), 0.5f);
    REQUIRE(implementation.droppedMessages() > 0);

    auto const droppedBefore(implementation.droppedMessages());
    host.setNewSample(samples.front());

    // The load was dropped...
    CHECK(implementation.droppedMessages() > droppedBefore);
    // ...so the plugin must not claim to be playing it.
    CHECK(host.currentSampleFile() == juce::File());
}
