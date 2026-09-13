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
// getParameter(id, Program) instantiates both; the plugin's TU may inline its copies away
#include "core/host_interop/plugin2HostImpl.inl"
#include "core/modules/automatedModuleImpl.inl"
#include "external_audio/sample.hpp"
#include "gui/editor/spectrumWorxEditor.hpp"
#include "gui/modules/moduleUI.hpp"

#include "le/spectrumworx/presets.hpp"
#include "le/utility/intrusivePtr.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <random>
#include <string>
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
void runOneBlock(ActivePlugin &plugin, std::vector<TimedParameterEvents::At> const &events = {})
{
    TimedParameterEvents const list(events);
    std::vector<float> leftIn(blockSize, 0), rightIn(blockSize, 0);
    std::vector<float> leftOut(blockSize, 0), rightOut(blockSize, 0);
    plugin.process(leftIn, rightIn, leftOut, rightOut, nullptr, &*list);
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
///   The counter is not a repair. The echo ring is the one with a repair behind
/// it, so its cases assert the two copies agree afterwards. \see issue #198.
///
////////////////////////////////////////////////////////////////////////////////

namespace
{
/// One more than the ring in question holds. Two constants because the two are
/// no longer the same depth: the echo ring is the one a host automating hard
/// fills, so it is the deeper of them.
constexpr unsigned overflowingCommands{LE::SW::Threading::ToEngineQueue::capacity + 1};
constexpr unsigned overflowingEchoes{LE::SW::Threading::ToUIQueue::capacity + 1};

LE::SW::ParameterID globalParameterID(unsigned const index)
{
    return LE::SW::ParameterID{LE::Plugins::ParameterID{parameterID(globalType, index)}};
}

/// Every parameter on which the engine's Program and the main thread's differ.
std::vector<std::string> disagreements(clap_plugin const &plugin)
{
    auto &implementation(implementationOf(plugin));
    auto &host(editorHostOf(plugin));

    std::vector<std::string> found;
    for (auto const &info : allParameterInfo(plugin, parameters(plugin)))
    {
        LE::SW::ParameterID const id{LE::Plugins::ParameterID{info.id}};
        auto const engine(implementation.getParameter(id, implementation.program()));
        auto const main(implementation.getParameter(id, host.programMain()));
        if (engine != main)
            found.push_back(std::string(info.module) + " / " + info.name + ": engine " +
                            std::to_string(engine) + ", main " + std::to_string(main));
    }
    return found;
}

/// What stateSave would write, from either copy.
std::string asSaved(LE::SW::Program const &program)
{
    return LE::SW::savePreset({}, LE::SW::defaultSideChainSource, {}, {}, program);
}

void checkLevel(clap_plugin const &plugin)
{
    auto const differing(disagreements(plugin));
    for (auto const &line : differing)
        UNSCOPED_INFO(line);
    CHECK(differing.empty());

    CHECK(asSaved(editorHostOf(plugin).programMain()) ==
          asSaved(implementationOf(plugin).program()));
}

/// A ring and one more of one parameter's automation, each value new, in one block.
void overflowTheEchoRing(ActivePlugin &plugin)
{
    std::vector<TimedParameterEvents::At> ramp;
    for (unsigned echo(0); echo < overflowingEchoes; ++echo)
        ramp.push_back({0, parameterID(globalType, 0), 0.25 + (0.5 * echo) / overflowingEchoes});
    runOneBlock(plugin, ramp);
    REQUIRE(implementationOf(*plugin).droppedMessages() > 0);
}

/// Each automatable parameter somewhere in its range, as a host's fuzzer writes it.
std::vector<TimedParameterEvents::At>
everyParameterAtRandom(std::vector<clap_param_info> const &infos, std::mt19937 &random,
                       unsigned const firstMovingSlot)
{
    std::vector<TimedParameterEvents::At> events;
    for (auto const &info : infos)
    {
        // the spectral three restart the plugin, which is not this case's subject
        if (!(info.flags & CLAP_PARAM_IS_AUTOMATABLE))
            continue;
        if (((info.id >> 24) == moduleChainType) && (((info.id >> 16) & 0xff) < firstMovingSlot))
            continue;
        // the ends a third of the time each: a boolean on a 0..1 edge is off at 0 alone
        std::uniform_real_distribution<double> range(info.min_value, info.max_value);
        auto value(range(random));
        switch (random() % 3)
        {
        case 0:
            value = info.min_value;
            break;
        case 1:
            value = info.max_value;
            break;
        }
        if (info.flags & CLAP_PARAM_IS_STEPPED)
            value = std::round(value);
        events.push_back({0, info.id, value});
    }
    return events;
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
    for (unsigned edit(0); edit < overflowingCommands; ++edit)
        host.editParameter(globalParameterID(0), 0.5f);

    CHECK(implementation.droppedMessages() > 0);
}

TEST_CASE("A full echo ring leaves the main thread's Program behind until it resyncs",
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
    for (unsigned block(0); block < overflowingEchoes; ++block)
    {
        OneParameterEvent const event(id, 0.25 + (0.5 * block) / overflowingEchoes);
        plugin.process(leftIn, rightIn, leftOut, rightOut, nullptr, &*event);
    }

    REQUIRE(implementation.droppedMessages() > 0);

    auto const asTheEngineHasIt(implementation.program()
                                    .parameters()
                                    .get<LE::SW::GlobalParameters::InputGain>()
                                    .getValue());
    auto const hostReads([&] {
        double value{-1};
        REQUIRE(params.get_value(&*plugin, id, &value));
        return value;
    });

    // the drain takes what fit, which is stale, and asks for the rest
    plugin.pumpMainThread();
    CHECK(hostReads() != Catch::Approx(asTheEngineHasIt));

    // the engine answers on its next block and the one after drains it
    runOneBlock(plugin);
    plugin.pumpMainThread();
    CHECK(hostReads() == Catch::Approx(asTheEngineHasIt));
    checkLevel(*plugin);
}

////////////////////////////////////////////////////////////////////////////////
///
/// \note clap-validator's param-fuzz: every parameter, every block, callback
/// starved. The republish is state rather than writes, so order matters here.
///
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("A parameter storm that overflows the echo ring ends with both Programs level",
          "[clap][protocol][hostile]")
{
    Entry const entry;

    ActivePlugin plugin(sampleRate, blockSize);
    auto &host(editorHostOf(*plugin));
    auto &implementation(implementationOf(*plugin));

    for (std::uint8_t slot(0); slot < LE::SW::Constants::maxNumberOfModules; ++slot)
        REQUIRE(host.editSlot(slot, static_cast<std::int8_t>(slot + 2)));
    runOneBlock(plugin);
    plugin.pumpMainThread();
    checkLevel(*plugin);

    auto const infos(allParameterInfo(*plugin, parameters(*plugin)));
    std::mt19937 random(198);

    // only what an effect owns is echoed, so count drops rather than blocks
    //
    // three slots never move: a fresh module has every LFO off, hiding what a running one does
    constexpr unsigned firstMovingSlot{3};
    unsigned blocks(0);
    while (implementation.droppedMessages() < overflowingEchoes)
    {
        REQUIRE(++blocks < 1000);
        runOneBlock(plugin, everyParameterAtRandom(infos, random, firstMovingSlot));
    }

    plugin.pumpMainThread();
    runOneBlock(plugin);
    plugin.pumpMainThread();

    checkLevel(*plugin);
}

////////////////////////////////////////////////////////////////////////////////
///
/// \note A module parameter's write is skipped while its LFO runs, so a replay
/// of state through that setter cannot move a base with the LFO on in both copies.
///
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("A resync moves the base of a parameter whose LFO is running",
          "[clap][protocol][hostile]")
{
    Entry const entry;

    ActivePlugin plugin(sampleRate, blockSize);
    auto &host(editorHostOf(*plugin));

    REQUIRE(host.editSlot(0, secondEffect));
    runOneBlock(plugin);

    auto const gain(parameterID(moduleType, 0, 1 << 8));
    auto const gainLFOEnabled(parameterID(lfoType, 0, 0));

    runOneBlock(plugin, {{0, gain, 0.2}, {0, gainLFOEnabled, 1}});
    plugin.pumpMainThread();
    checkLevel(*plugin);

    overflowTheEchoRing(plugin);
    runOneBlock(plugin, {{0, gainLFOEnabled, 0}, {0, gain, 0.8}, {0, gainLFOEnabled, 1}});

    plugin.pumpMainThread();
    runOneBlock(plugin);
    plugin.pumpMainThread();

    checkLevel(*plugin);
}

////////////////////////////////////////////////////////////////////////////////
///
/// \note A period snaps to the grid of the sync type its LFO has when it is
/// written, and the period comes before the sync type in the parameter list.
///
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("A resync lands a synced period on the grid the engine has it on",
          "[clap][protocol][hostile]")
{
    Entry const entry;

    ActivePlugin plugin(sampleRate, blockSize);
    auto &host(editorHostOf(*plugin));

    REQUIRE(host.editSlot(0, secondEffect));
    runOneBlock(plugin);

    auto const lfoParameter([](unsigned const lfo, unsigned const which) {
        return parameterID(lfoType, 0, (lfo << 8) | which);
    });
    constexpr unsigned period{1}, syncTypes{5};
    constexpr double note{1}, triplet{2};
    constexpr unsigned lfos{5};

    std::vector<TimedParameterEvents::At> onTriplets, onNotes;
    for (unsigned lfo(0); lfo < lfos; ++lfo)
    {
        onTriplets.push_back({0, lfoParameter(lfo, syncTypes), triplet});
        onNotes.push_back({0, lfoParameter(lfo, syncTypes), note});
        onNotes.push_back({0, lfoParameter(lfo, period), 0.15 + 0.17 * lfo});
    }

    runOneBlock(plugin, onTriplets);
    plugin.pumpMainThread();
    checkLevel(*plugin);

    overflowTheEchoRing(plugin);
    runOneBlock(plugin, onNotes);

    plugin.pumpMainThread();
    runOneBlock(plugin);
    plugin.pumpMainThread();

    checkLevel(*plugin);
}

////////////////////////////////////////////////////////////////////////////////
///
/// \note A lost slot selector means the rack resync and the host's rescan ran
/// against the stale copy, so the republish that moves the slot has to say so.
///
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("A slot the resync moves is redrawn and announced to the host",
          "[clap][protocol][hostile][gui]")
{
    using Editor = LE::SW::GUI::SpectrumWorxEditor;

    Entry const entry;
    juce::ScopedJuceInitialiser_GUI const juceIsUp;

    TestHost testHost{{.params = true}};
    ActivePlugin plugin(sampleRate, blockSize, testHost);
    auto &host(editorHostOf(*plugin));
    auto &implementation(implementationOf(*plugin));
    auto const &params(parameters(*plugin));

    auto editor(std::make_unique<Editor>(host, Editor::PanelPlacement::overlay));

    REQUIRE(host.editSlot(0, secondEffect));
    runOneBlock(plugin);
    plugin.pumpMainThread();
    editor->resyncModuleRack();
    REQUIRE(editor->regionInSlot(0) != nullptr);

    auto const selector(slotSelector(*plugin, params, 0));

    overflowTheEchoRing(plugin);
    runOneBlock(plugin, {{0, selector.id, aDifferentValue(*plugin, params, selector)}});

    auto const engineEffect(
        implementation.program().moduleChain().moduleAs<LE::SW::Module>(0)->effectTypeIndex());
    REQUIRE(engineEffect != secondEffect);

    // the callback the slot change asked for, run against the stale copy
    plugin.pumpMainThread();
    auto const rescansBefore(testHost.rescanCalls.load());
    testHost.rescanFlags = 0;

    runOneBlock(plugin);
    plugin.pumpMainThread();

    checkLevel(*plugin);
    CHECK(testHost.rescanCalls.load() > rescansBefore);
    CHECK((testHost.rescanFlags.load() & CLAP_PARAM_RESCAN_INFO) != 0);
    REQUIRE(editor->regionInSlot(0) != nullptr);
    CHECK(editor->regionInSlot(0)->module().effectTypeIndex() == engineEffect);

    editor.reset();
}

////////////////////////////////////////////////////////////////////////////////
///
/// \note A host need not run the callback before deactivating, and a stopped
/// engine drains no commands, so a queued request would leave stateSave stale.
///
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("A plugin deactivated straight after a storm is level before it saves",
          "[clap][protocol][hostile]")
{
    Entry const entry;

    ActivePlugin plugin(sampleRate, blockSize);

    overflowTheEchoRing(plugin);

    plugin.whileDeactivated([](clap_plugin const &deactivated) { checkLevel(deactivated); });
}

////////////////////////////////////////////////////////////////////////////////
///
/// \note The request is itself a command, and the command ring can be full when
/// it is made; the news that echoes were lost has to outlive that.
///
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("A resync asked for on a full command ring is asked for again",
          "[clap][protocol][hostile]")
{
    Entry const entry;

    ActivePlugin plugin(sampleRate, blockSize);
    auto &host(editorHostOf(*plugin));
    auto &implementation(implementationOf(*plugin));

    overflowTheEchoRing(plugin);

    // another parameter, at the value it has, so the edits that do not fit cost nothing
    auto const edited(globalParameterID(1));
    auto const editedTo(implementation.getParameter(edited, implementation.program()));
    auto const echoesDropped(implementation.droppedMessages());
    for (unsigned edit(0); edit < overflowingCommands; ++edit)
        host.editParameter(edited, editedTo);
    REQUIRE(implementation.droppedMessages() > echoesDropped);

    // the request cannot be queued, and the block drains only the edits
    auto const beforeRequest(implementation.droppedMessages());
    plugin.pumpMainThread();
    CHECK(implementation.droppedMessages() > beforeRequest);
    runOneBlock(plugin);
    plugin.pumpMainThread();
    runOneBlock(plugin);
    plugin.pumpMainThread();

    checkLevel(*plugin);
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

    REQUIRE(host.currentSampleFile().empty());

    // Fill the ring so that the sample's own push cannot land.
    for (unsigned edit(0); edit < overflowingCommands; ++edit)
        host.editParameter(globalParameterID(0), 0.5f);
    REQUIRE(implementation.droppedMessages() > 0);

    auto const droppedBefore(implementation.droppedMessages());
    host.setNewSample(samples.front());

    // The load was dropped...
    CHECK(implementation.droppedMessages() > droppedBefore);
    // ...so the plugin must not claim to be playing it.
    CHECK(host.currentSampleFile().empty());
}
