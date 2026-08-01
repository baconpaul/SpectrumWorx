////////////////////////////////////////////////////////////////////////////////
///
/// \file pluginTests.cpp
/// ---------------------
///
///   Drives the plugin the way a host does: through clap_plugin_entry's
/// factory and the clap_plugin vtable, not through SpectrumWorxCLAP's C++
/// interface. That is deliberate -- the lifecycle order a DAW uses (init,
/// activate, start_processing, process, ...) is the thing most likely to be
/// wrong, and calling the C++ members directly would not exercise it.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "swClapEntryImpl.hpp"

#include "core/host_interop/parameters.hpp" // the fixed parameter count

#include <clap/clap.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <numbers>
#include <functional>
#include <set>
#include <vector>
//------------------------------------------------------------------------------
namespace
{
//------------------------------------------------------------------------------

/// The smallest host that answers everything clap::helpers::Plugin asks at
/// construction. Every extension is absent, which is legal and is the
/// interesting case: it means the plugin may not assume any of them.
clap_host const &nullHost()
{
    static clap_host host{CLAP_VERSION,
                          nullptr,
                          "sw-tests",
                          "SpectrumWorx",
                          "",
                          "0",
                          [](clap_host const *, char const *) -> void const * { return nullptr; },
                          [](clap_host const *) {},
                          [](clap_host const *) {},
                          [](clap_host const *) {}};
    return host;
}

/// \brief A host that supports clap_host_params and counts what it is asked.
///
/// \note nullHost() above deliberately has no extensions, which makes it useless
/// for the one thing a slot change is supposed to cause. Rescans are deferred to
/// the main thread, so a test has to pump on_main_thread() before looking.
class RecordingHost
{
  public:
    RecordingHost()
        : params_{[](clap_host const *, clap_param_rescan_flags const flags) {
                      instance().rescanFlags |= flags;
                  },
                  [](clap_host const *, clap_id, clap_param_clear_flags) {},
                  [](clap_host const *) { ++instance().flushRequests; }},
          host_{CLAP_VERSION, nullptr, "sw-tests", "SpectrumWorx", "", "0",
                [](clap_host const *, char const *const id) -> void const * {
                    return (std::strcmp(id, CLAP_EXT_PARAMS) == 0) ? &instance().params_ : nullptr;
                },
                // request_restart, request_process, request_callback -- in that
                // order; the last is the one a deferred rescan asks for.
                [](clap_host const *) {}, [](clap_host const *) {},
                [](clap_host const *) { ++instance().mainThreadCallbacks; }}
    {
        rescanFlags = 0;
        flushRequests = 0;
        mainThreadCallbacks = 0;
    }

    RecordingHost(RecordingHost const &) = delete; // the callbacks reach the singleton
    RecordingHost &operator=(RecordingHost const &) = delete;

    clap_host const &operator*() const { return host_; }

    clap_param_rescan_flags rescanFlags{0};
    unsigned flushRequests{0};
    unsigned mainThreadCallbacks{0};

    /// \note The C callbacks carry no context of their own -- clap_host::host_data
    /// is the plugin's, not ours -- so there is one of these at a time.
    static RecordingHost &instance()
    {
        REQUIRE(pInstance != nullptr);
        return *pInstance;
    }
    static RecordingHost *pInstance;

  private:
    clap_host_params params_;
    clap_host host_;
}; // class RecordingHost

RecordingHost *RecordingHost::pInstance{nullptr};

/// Scopes RecordingHost::instance() to one test.
class CurrentRecordingHost
{
  public:
    CurrentRecordingHost() { RecordingHost::pInstance = &host_; }
    ~CurrentRecordingHost() { RecordingHost::pInstance = nullptr; }

    CurrentRecordingHost(CurrentRecordingHost const &) = delete;
    CurrentRecordingHost &operator=(CurrentRecordingHost const &) = delete;

    RecordingHost &operator*() { return host_; }
    RecordingHost *operator->() { return &host_; }

  private:
    RecordingHost host_;
}; // class CurrentRecordingHost

////////////////////////////////////////////////////////////////////////////////
///
/// \brief A host that provides `clap.state` and, deliberately, no
/// `clap.thread-check`.
///
///   RecordingHost above offers `clap.params` and nothing else, which is why
/// week_two.md §2.1a was invisible: `markCurrentProgramAsModified()` returns at
/// its `canUseState()` check before it can ask a thread-check that is not there.
/// Every real DAW provides state. This is the shape that reaches the bug --
/// state present, thread-check absent -- and CLAP explicitly allows it:
/// `clap.thread-check` is optional and a plugin may not assume it.
///
/// \note The interesting assertion is that nothing crashes. What it also pins
/// is the *consequence* of not being able to ask: the dirty mark has to go
/// through on_main_thread(), because "am I already on it" is exactly the
/// question this host refuses to answer.
///
////////////////////////////////////////////////////////////////////////////////

class StatefulHost
{
  public:
    StatefulHost()
        : state_{[](clap_host const *) { ++instance().dirtyMarks; }},
          host_{CLAP_VERSION,
                nullptr,
                "sw-tests",
                "SpectrumWorx",
                "",
                "0",
                [](clap_host const *, char const *const id) -> void const * {
                    return (std::strcmp(id, CLAP_EXT_STATE) == 0) ? &instance().state_ : nullptr;
                },
                [](clap_host const *) {},
                [](clap_host const *) {},
                [](clap_host const *) { ++instance().mainThreadCallbacks; }}
    {
        dirtyMarks = 0;
        mainThreadCallbacks = 0;
    }

    StatefulHost(StatefulHost const &) = delete; // the callbacks reach the singleton
    StatefulHost &operator=(StatefulHost const &) = delete;

    clap_host const &operator*() const { return host_; }

    unsigned dirtyMarks{0};
    unsigned mainThreadCallbacks{0};

    static StatefulHost &instance()
    {
        REQUIRE(pInstance != nullptr);
        return *pInstance;
    }
    static StatefulHost *pInstance;

  private:
    clap_host_state state_;
    clap_host host_;
}; // class StatefulHost

StatefulHost *StatefulHost::pInstance{nullptr};

/// Scopes StatefulHost::instance() to one test. \see CurrentRecordingHost
class CurrentStatefulHost
{
  public:
    CurrentStatefulHost() { StatefulHost::pInstance = &host_; }
    ~CurrentStatefulHost() { StatefulHost::pInstance = nullptr; }

    CurrentStatefulHost(CurrentStatefulHost const &) = delete;
    CurrentStatefulHost &operator=(CurrentStatefulHost const &) = delete;

    StatefulHost &operator*() { return host_; }
    StatefulHost *operator->() { return &host_; }

  private:
    StatefulHost host_;
}; // class CurrentStatefulHost

clap_input_events const &noInputEvents()
{
    static clap_input_events events{
        nullptr, [](clap_input_events const *) -> std::uint32_t { return 0; },
        [](clap_input_events const *, std::uint32_t) -> clap_event_header const * {
            return nullptr;
        }};
    return events;
}

clap_output_events const &discardedOutputEvents()
{
    static clap_output_events events{
        nullptr, [](clap_output_events const *, clap_event_header const *) { return true; }};
    return events;
}

clap_plugin_factory const &factory()
{
    auto const *const pFactory(static_cast<clap_plugin_factory const *>(
        LE::SW::ClapFirst::getFactory(CLAP_PLUGIN_FACTORY_ID)));
    REQUIRE(pFactory != nullptr);
    return *pFactory;
}

/// \brief RAII around the entry point, whose init/deinit are refcounted and
/// must bracket every factory call.
class Entry
{
  public:
    Entry() { REQUIRE(LE::SW::ClapFirst::clapInit("sw-tests")); }
    ~Entry() { LE::SW::ClapFirst::clapDeinit(); }

    Entry(Entry const &) = delete; // makes non-copyable
    Entry &operator=(Entry const &) = delete;
}; // class Entry

/// \brief A plugin taken all the way to "processing", and torn down in order.
class ActivePlugin
{
  public:
    /// \param beforeActivate run against the initialised-but-inactive plugin,
    /// which is when a host restores state and when params.flush() is legal but
    /// the engine has no sample rate yet.
    ActivePlugin(double const sampleRate, std::uint32_t const blockSize,
                 clap_host const &host = nullHost(),
                 std::function<void(clap_plugin const &)> const &beforeActivate = {})
        : blockSize_(blockSize)
    {
        pPlugin_ = factory().create_plugin(&factory(), &host, descriptorID());
        REQUIRE(pPlugin_ != nullptr);
        REQUIRE(pPlugin_->init(pPlugin_));
        if (beforeActivate)
            beforeActivate(*pPlugin_);
        REQUIRE(pPlugin_->activate(pPlugin_, sampleRate, 1, blockSize));
        REQUIRE(pPlugin_->start_processing(pPlugin_));
    }

    ~ActivePlugin()
    {
        pPlugin_->stop_processing(pPlugin_);
        pPlugin_->deactivate(pPlugin_);
        pPlugin_->destroy(pPlugin_);
    }

    ActivePlugin(ActivePlugin const &) = delete; // makes non-copyable
    ActivePlugin &operator=(ActivePlugin const &) = delete;

    clap_plugin const &operator*() const { return *pPlugin_; }
    clap_plugin const *operator->() const { return pPlugin_; }

    /// Runs one block of stereo audio through, in place of a host's callback.
    ///
    /// \param transport what the host reports, nullptr being a host that reports
    /// nothing -- which is the default because most of these cases do not care,
    /// and because it is the harder half of the LFO timing contract.
    void process(std::vector<float> &leftIn, std::vector<float> &rightIn,
                 std::vector<float> &leftOut, std::vector<float> &rightOut,
                 clap_event_transport const *const transport = nullptr)
    {
        float *inputChannels[]{leftIn.data(), rightIn.data()};
        float *outputChannels[]{leftOut.data(), rightOut.data()};

        clap_audio_buffer input{&inputChannels[0], nullptr, 2, 0, 0};
        clap_audio_buffer output{&outputChannels[0], nullptr, 2, 0, 0};

        clap_process process{};
        process.steady_time = -1;
        process.frames_count = blockSize_;
        process.transport = transport;
        process.audio_inputs = &input;
        process.audio_inputs_count = 1;
        process.audio_outputs = &output;
        process.audio_outputs_count = 1;
        process.in_events = &noInputEvents();
        process.out_events = &discardedOutputEvents();

        REQUIRE(pPlugin_->process(pPlugin_, &process) != CLAP_PROCESS_ERROR);
    }

    static char const *descriptorID()
    {
        auto const *const pDescriptor(factory().get_plugin_descriptor(&factory(), 0));
        REQUIRE(pDescriptor != nullptr);
        return pDescriptor->id;
    }

  private:
    clap_plugin const *pPlugin_;
    std::uint32_t blockSize_;
}; // class ActivePlugin

void fillWithSine(std::vector<float> &buffer, float const sampleRate, float const frequency,
                  std::uint32_t const startFrame)
{
    for (std::size_t frame(0); frame < buffer.size(); ++frame)
        buffer[frame] = 0.5f * std::sin(2 * std::numbers::pi_v<float> * frequency *
                                        static_cast<float>(startFrame + frame) / sampleRate);
}

bool allFinite(std::vector<float> const &buffer)
{
    return std::all_of(buffer.begin(), buffer.end(),
                       [](float const sample) { return std::isfinite(sample); });
}

float peak(std::vector<float> const &buffer)
{
    float largest{0};
    for (auto const sample : buffer)
        largest = std::max(largest, std::abs(sample));
    return largest;
}

/// \note ParameterID's members are laid out in reverse so that the hex reads
/// naturally on a little-endian machine: the type is the top byte and the module
/// index the one below it. See core/parameterID.hpp.
enum ParameterType : clap_id
{
    globalType = 0,
    moduleChainType = 1,
    moduleType = 2,
    lfoType = 3
};

clap_id parameterID(ParameterType const type, unsigned const moduleIndex = 0,
                    unsigned const parameterIndex = 0)
{
    return (static_cast<clap_id>(type) << 24) | (moduleIndex << 16) | parameterIndex;
}

/// An input event list holding exactly one parameter value, which is how a host
/// delivers an edit to flush() or process().
class OneParameterEvent
{
  public:
    OneParameterEvent(clap_id const id, double const value) : list_{this, size, get}, event_{}
    {
        event_.header.size = sizeof(event_);
        event_.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        event_.header.type = CLAP_EVENT_PARAM_VALUE;
        event_.param_id = id;
        event_.note_id = event_.port_index = event_.channel = event_.key = -1;
        event_.value = value;
    }

    OneParameterEvent(OneParameterEvent const &) = delete; // self-referential ctx
    OneParameterEvent &operator=(OneParameterEvent const &) = delete;

    clap_input_events const &operator*() const { return list_; }

  private:
    static std::uint32_t size(clap_input_events const *) { return 1; }
    static clap_event_header const *get(clap_input_events const *self, std::uint32_t)
    {
        return &static_cast<OneParameterEvent const *>(self->ctx)->event_.header;
    }

    clap_input_events list_;
    clap_event_param_value event_;
}; // class OneParameterEvent

/// Every parameter's description, indexed the way the host reads them.
std::vector<clap_param_info> allParameterInfo(clap_plugin const &plugin,
                                              clap_plugin_params const &params)
{
    std::vector<clap_param_info> infos(params.count(&plugin));
    for (std::uint32_t index(0); index < infos.size(); ++index)
        REQUIRE(params.get_info(&plugin, index, &infos[index]));
    return infos;
}

clap_plugin_params const &parameters(clap_plugin const &plugin)
{
    auto const *const params(
        static_cast<clap_plugin_params const *>(plugin.get_extension(&plugin, CLAP_EXT_PARAMS)));
    REQUIRE(params != nullptr);
    return *params;
}

bool isNormalisedType(clap_id const id)
{
    auto const type(id >> 24);
    return (type == moduleType) || (type == lfoType);
}

/// An LFO's own parameters, in the order lfoImpl.hpp declares them.
enum LFOParameter : unsigned
{
    lfoEnabled = 0,
    lfoPeriodScale = 1,
    lfoPhase = 2,
    lfoLowerBound = 3,
    lfoUpperBound = 4,
    lfoSyncTypes = 5,
    lfoWaveform = 6
};

/// \note A module's LFOs are indexed by *LFO-able* parameter, which is its
/// parameter index less the one base parameter that has no LFO (Bypass, first).
/// So LFO 0 drives module parameter 1, which is Gain -- a base parameter, and so
/// one every effect has whatever is in the slot.
clap_id lfoParameterID(unsigned const moduleIndex, unsigned const lfoIndex,
                       LFOParameter const which)
{
    return parameterID(lfoType, moduleIndex, (lfoIndex << 8) | which);
}

/// The module parameter \p lfoIndex drives. \see lfoParameterID
clap_id modulatedParameterID(unsigned const moduleIndex, unsigned const lfoIndex)
{
    return parameterID(moduleType, moduleIndex, (lfoIndex + 1) << 8);
}

/// \brief Fills slot 0, turns LFO 0 on, and reports how many distinct values its
/// target takes over \p blocks blocks of audio.
///
/// One is the failure: it means the LFO is enabled, is being asked for a value
/// every block, and is answering with the same one every time.
std::size_t distinctModulatedValues(ActivePlugin &plugin, clap_plugin_params const &params,
                                    float const sampleRate, std::uint32_t const blockSize,
                                    unsigned const blocks,
                                    clap_event_transport const *const transport)
{
    OneParameterEvent const fillSlotOne(parameterID(moduleChainType, 0),
                                        0 /*the first effect in the list*/);
    params.flush(&*plugin, &*fillSlotOne, &discardedOutputEvents());

    OneParameterEvent const enable(lfoParameterID(0, 0, lfoEnabled), 1);
    params.flush(&*plugin, &*enable, &discardedOutputEvents());

    auto const target(modulatedParameterID(0, 0));

    std::vector<float> leftIn(blockSize), rightIn(blockSize);
    std::vector<float> leftOut(blockSize), rightOut(blockSize);
    std::set<double> seen;

    for (unsigned block(0); block < blocks; ++block)
    {
        fillWithSine(leftIn, sampleRate, 440.0f, block * blockSize);
        rightIn = leftIn;
        plugin.process(leftIn, rightIn, leftOut, rightOut, transport);

        double value{0};
        REQUIRE(params.get_value(&*plugin, target, &value));
        seen.insert(value);
    }
    return seen.size();
}

/// A host's transport, at \p tempo in 4/4, parked at \p positionInBeats.
clap_event_transport transportAt(double const tempo, double const positionInBeats,
                                 std::uint32_t const extraFlags)
{
    clap_event_transport transport{};
    transport.header.size = sizeof(transport);
    transport.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    transport.header.type = CLAP_EVENT_TRANSPORT;
    transport.flags = CLAP_TRANSPORT_HAS_TEMPO | CLAP_TRANSPORT_HAS_TIME_SIGNATURE |
                      CLAP_TRANSPORT_HAS_BEATS_TIMELINE | extraFlags;
    transport.tempo = tempo;
    transport.tsig_num = 4;
    transport.tsig_denom = 4;
    transport.song_pos_beats = static_cast<clap_beattime>(positionInBeats * CLAP_BEATTIME_FACTOR);
    return transport;
}

//------------------------------------------------------------------------------
} // anonymous namespace
//------------------------------------------------------------------------------

TEST_CASE("The factory offers exactly one plugin, and it describes itself", "[clap]")
{
    Entry const entry;

    REQUIRE(factory().get_plugin_count(&factory()) == 1);

    auto const *const pDescriptor(factory().get_plugin_descriptor(&factory(), 0));
    REQUIRE(pDescriptor != nullptr);
    CHECK(std::strlen(pDescriptor->id) > 0);
    CHECK(std::strcmp(pDescriptor->name, "SpectrumWorx") == 0);
    CHECK(pDescriptor->features != nullptr);
    CHECK(std::strcmp(pDescriptor->features[0], CLAP_PLUGIN_FEATURE_AUDIO_EFFECT) == 0);
}

TEST_CASE("A plugin survives the host lifecycle in order", "[clap]")
{
    Entry const entry;
    ActivePlugin plugin(48000, 512);
    CHECK(plugin->desc != nullptr);
}

TEST_CASE("Audio ports are stereo in, stereo out, plus a side chain", "[clap]")
{
    Entry const entry;
    ActivePlugin plugin(48000, 512);

    auto const *const ports(static_cast<clap_plugin_audio_ports const *>(
        plugin->get_extension(&*plugin, CLAP_EXT_AUDIO_PORTS)));
    REQUIRE(ports != nullptr);

    CHECK(ports->count(&*plugin, true) == 2);
    CHECK(ports->count(&*plugin, false) == 1);

    clap_audio_port_info info{};
    REQUIRE(ports->get(&*plugin, 0, true, &info));
    CHECK(info.channel_count == 2);
    CHECK((info.flags & CLAP_AUDIO_PORT_IS_MAIN) != 0);

    /// \note Never an in-place pair -- see the note in spectrumWorxCLAP.cpp.
    CHECK(info.in_place_pair == CLAP_INVALID_ID);
}

TEST_CASE("The engine reports the latency its FFT size implies", "[clap]")
{
    Entry const entry;
    ActivePlugin plugin(48000, 512);

    auto const *const latency(static_cast<clap_plugin_latency const *>(
        plugin->get_extension(&*plugin, CLAP_EXT_LATENCY)));
    REQUIRE(latency != nullptr);

    /// 2048 today, which is the default FFT size -- but the bound rather than
    /// the number, because the FFT size is a parameter and this test has no
    /// business pinning its default. What matters is that it is not zero: that
    /// is what the stage 1 pass-through reported, so it is what tells a live
    /// engine from a dead one.
    auto const reported(latency->get(&*plugin));
    CHECK(reported > 0);
    CHECK(reported <= 8192);
}

TEST_CASE("A sine through the default chain comes out as audio, not silence", "[clap]")
{
    constexpr float sampleRate{48000};
    constexpr std::uint32_t blockSize{512};
    // Enough blocks to push the first input past the engine's latency.
    constexpr unsigned int blocks{32};

    Entry const entry;
    ActivePlugin plugin(sampleRate, blockSize);

    std::vector<float> leftIn(blockSize), rightIn(blockSize);
    std::vector<float> leftOut(blockSize), rightOut(blockSize);

    float largestOutput{0};
    for (unsigned int block(0); block < blocks; ++block)
    {
        fillWithSine(leftIn, sampleRate, 440.0f, block * blockSize);
        rightIn = leftIn;
        std::fill(leftOut.begin(), leftOut.end(), 0.0f);
        std::fill(rightOut.begin(), rightOut.end(), 0.0f);

        plugin.process(leftIn, rightIn, leftOut, rightOut);

        REQUIRE(allFinite(leftOut));
        REQUIRE(allFinite(rightOut));
        largestOutput = std::max(largestOutput, peak(leftOut));
    }

    // A bypassed module chain still runs the full analysis/resynthesis path, so
    // the sine has to survive it at roughly its input amplitude. The stage 1
    // pass-through would also pass this; the latency case above is what tells
    // the two apart.
    CHECK(largestOutput > 0.1f);
    CHECK(largestOutput < 2.0f);
}

TEST_CASE("The host sees the engine's own parameters, not a stand-in", "[clap]")
{
    Entry const entry;
    ActivePlugin plugin(48000, 512);

    auto const *const params(
        static_cast<clap_plugin_params const *>(plugin->get_extension(&*plugin, CLAP_EXT_PARAMS)));
    REQUIRE(params != nullptr);

    auto const count(params->count(&*plugin));
    REQUIRE(count > 0);

    // Every entry has to be nameable, in range, and grouped somewhere.
    bool sawAGlobal{false};
    for (std::uint32_t index(0); index < count; ++index)
    {
        clap_param_info info{};
        REQUIRE(params->get_info(&*plugin, index, &info));
        INFO("parameter " << index << " '" << info.name << "'");
        CHECK(std::strlen(info.name) > 0);
        // Strictly less: a host divides by this range, and one parameter in a
        // fixed list that no effect currently owns must not hand it a zero.
        CHECK(info.min_value < info.max_value);
        CHECK(info.default_value >= info.min_value);
        CHECK(info.default_value <= info.max_value);
        CHECK(std::strlen(info.module) > 0);
        if (std::strcmp(info.module, "Global") == 0)
            sawAGlobal = true;

        double value{0};
        CHECK(params->get_value(&*plugin, info.id, &value));

        std::array<char, CLAP_NAME_SIZE> text{};
        CHECK(params->value_to_text(&*plugin, info.id, value, text.data(), text.size()));
    }
    CHECK(sawAGlobal);
}

TEST_CASE("A parameter no effect currently owns is hidden, not broken", "[clap]")
{
    // The cost of declaring every slot's parameters up front: on an empty
    // instance most of them belong to no effect. They keep valid IDs and a
    // usable range, and say so with CLAP_PARAM_IS_HIDDEN.
    Entry const entry;
    ActivePlugin plugin(48000, 512);

    auto const *const params(
        static_cast<clap_plugin_params const *>(plugin->get_extension(&*plugin, CLAP_EXT_PARAMS)));
    REQUIRE(params != nullptr);

    std::uint32_t hidden{0}, shown{0};
    for (std::uint32_t index(0); index < params->count(&*plugin); ++index)
    {
        clap_param_info info{};
        REQUIRE(params->get_info(&*plugin, index, &info));
        INFO("parameter " << index << " '" << info.name << "'");
        // Hidden or not, it must still be answerable.
        double value{0};
        CHECK(params->get_value(&*plugin, info.id, &value));
        CHECK(info.min_value < info.max_value);
        ((info.flags & CLAP_PARAM_IS_HIDDEN) ? hidden : shown)++;
    }

    // The globals and the five slot selectors are real with nothing loaded.
    CHECK(shown > 0);
    CHECK(hidden > 0);
}

TEST_CASE("Filling a module slot renames its parameters without adding any", "[clap]")
{
    // A slot's effect is itself a parameter, and setting it changes what the
    // slot's other parameters *are*. What it must not change is how many there
    // are: ext/params.h only lets a plugin add or remove parameters through
    // clap_host->restart() and a deactivated CLAP_PARAM_RESCAN_ALL, so the
    // count a host reads once has to stay good. Every slot's full complement is
    // therefore declared up front and an unused one reads as N/A.
    Entry const entry;
    ActivePlugin plugin(48000, 512);

    auto const &params(parameters(*plugin));

    auto const before(allParameterInfo(*plugin, params));

    // Every module parameter of every slot, plus the LFOs, plus the globals --
    // present with no effect loaded at all, which is the point.
    CHECK(before.size() == LE::SW::ParameterCounts::maxNumberOfParameters);

    // Slot 1's parameters exist already; they just have nothing to name them.
    auto const slotOne(
        [](clap_param_info const &info) { return std::strncmp(info.module, "Slot 1", 6) == 0; });
    auto const slotOneBefore(std::count_if(before.begin(), before.end(), slotOne));
    CHECK(slotOneBefore > 0);

    // None of slot 1's module or LFO parameters is usable yet, and every one of
    // them says so. The slot's *selector* is not one of them -- it is what fills
    // the slot, so it is always live -- and "Slot 1" is its module path too.
    for (auto const &info : before)
        if (slotOne(info) && isNormalisedType(info.id))
            CHECK((info.flags & CLAP_PARAM_IS_HIDDEN) != 0);

    OneParameterEvent const fillSlotOne(parameterID(moduleChainType, 0),
                                        0 /*the first effect in the list*/);
    params.flush(&*plugin, &*fillSlotOne, &discardedOutputEvents());

    auto const after(allParameterInfo(*plugin, params));

    // The count is the contract: it did not move.
    REQUIRE(after.size() == before.size());
    CHECK(std::count_if(after.begin(), after.end(), slotOne) == slotOneBefore);

    // What moved is the description, and only the parts CLAP_PARAM_RESCAN_INFO
    // names: the parameters stopped being hidden and picked up the effect's names.
    // Their ranges did not move, and could not have -- see the range test below.
    std::uint32_t revealed{0}, renamed{0};
    for (std::size_t index(0); index < after.size(); ++index)
    {
        if (!slotOne(after[index]))
            continue;
        if ((after[index].flags & CLAP_PARAM_IS_HIDDEN) == 0)
            ++revealed;
        if (std::strcmp(after[index].name, before[index].name) != 0)
            ++renamed;
    }
    CHECK(revealed > 0);
    CHECK(renamed > 0);
}

TEST_CASE("Every parameter accepts the bounds it advertises", "[clap]")
{
    // A host is entitled to write min_value and max_value to anything it is
    // shown, and clap-validator's param-fuzz-bounds does exactly that. Whatever
    // the plugin then does with the value, it may not be out of the range the
    // parameter itself considers valid.
    Entry const entry;
    ActivePlugin plugin(48000, 512);

    auto const &params(parameters(*plugin));

    /// \note Every effect in turn, not just the first. The parameters that have
    /// gone out of range are effect-specific ones, and a slot only ever has the
    /// parameters of whatever is currently in it -- so a sweep that fills the
    /// slot once tests one effect's parameters and calls it coverage.
    clap_param_info selector{};
    for (auto const &info : allParameterInfo(*plugin, params))
        if (info.id == parameterID(moduleChainType, 0))
            selector = info;
    REQUIRE(selector.max_value > selector.min_value);

    for (double effect(selector.min_value); effect <= selector.max_value; ++effect)
    {
        OneParameterEvent const fill(selector.id, effect);
        params.flush(&*plugin, &*fill, &discardedOutputEvents());

        for (auto const &info : allParameterInfo(*plugin, params))
        {
            if ((std::strncmp(info.module, "Slot 1", 6) != 0) || (info.id == selector.id))
                continue;

            for (double const value :
                 {info.min_value, info.max_value, (info.min_value + info.max_value) / 2})
            {
                CAPTURE(effect, info.id, info.name, value);
                OneParameterEvent const edit(info.id, value);
                params.flush(&*plugin, &*edit, &discardedOutputEvents());

                double read{0};
                REQUIRE(params.get_value(&*plugin, info.id, &read));
                CHECK(read >= info.min_value);
                CHECK(read <= info.max_value);

                /// \note And asked to render it, which is the other half of what
                /// a host does when it rescans -- get_info, get_value,
                /// value_to_text over the whole list. The renderer builds a
                /// throwaway parameter and assigns the value to it, so a value it
                /// considers invalid asserts there rather than anywhere that
                /// matters (printer.hpp's AutomatedParameterPrinter).
                std::array<char, 128> text{};
                CHECK(params.value_to_text(&*plugin, info.id, value, text.data(), text.size()));
            }
        }
    }
}

TEST_CASE("An effect chosen before activate still processes audio", "[clap]")
{
    // The order a session restore happens in, and the order a standalone starts
    // in: the parameter that fills a slot arrives while the plugin is merely
    // initialised, so the engine has no sample rate, no bins and no step time to
    // configure the effect against. Configuring it anyway is what asserted --
    // Bandpass and Bandstop indexed an empty spectrum, Denoiser divided an empty
    // amplitude range -- so it is deferred to the resize that activate() performs.
    //
    //   Which makes this the test that matters: deferring is only correct if the
    // effect really is set up by the time audio arrives.
    constexpr std::uint32_t blockSize{512};
    constexpr float sampleRate{48000};

    Entry const entry;
    ActivePlugin plugin(sampleRate, blockSize, nullHost(), [](clap_plugin const &inactive) {
        auto const *const params(static_cast<clap_plugin_params const *>(
            inactive.get_extension(&inactive, CLAP_EXT_PARAMS)));
        REQUIRE(params != nullptr);

        OneParameterEvent const fillSlotOne(parameterID(moduleChainType, 0), 0);
        params->flush(&inactive, &*fillSlotOne, &discardedOutputEvents());
    });

    // The slot really is filled, and by an effect rather than by nothing.
    auto const &params(parameters(*plugin));
    double selector{-1};
    REQUIRE(params.get_value(&*plugin, parameterID(moduleChainType, 0), &selector));
    CHECK(selector >= 0);

    std::vector<float> leftIn(blockSize), rightIn(blockSize);
    std::vector<float> leftOut(blockSize), rightOut(blockSize);
    fillWithSine(leftIn, sampleRate, 440, 0);
    fillWithSine(rightIn, sampleRate, 440, 0);

    // Long enough to clear the analysis latency, so silence would be a real answer
    // rather than a not-yet-answer.
    for (std::uint32_t block(0); block < 16; ++block)
    {
        fillWithSine(leftIn, sampleRate, 440, block * blockSize);
        fillWithSine(rightIn, sampleRate, 440, block * blockSize);
        plugin.process(leftIn, rightIn, leftOut, rightOut);
        CHECK(allFinite(leftOut));
        CHECK(allFinite(rightOut));
    }
    CHECK(peak(leftOut) > 0);
}

TEST_CASE("Filling a slot makes the host re-read the descriptions", "[clap]")
{
    // The names, the module paths and the hidden flags of a slot's parameters
    // all change when its effect does, and a host only learns that from
    // CLAP_PARAM_RESCAN_INFO. Without it the parameters keep the names they were
    // first read with -- "N/A" for a slot that was empty at startup -- which is
    // what a host shows however correct get_info would be if it were asked again.
    Entry const entry;
    CurrentRecordingHost host;
    ActivePlugin plugin(48000, 512, **host);

    auto const &params(parameters(*plugin));

    OneParameterEvent const fillSlotOne(parameterID(moduleChainType, 0), 0);
    params.flush(&*plugin, &*fillSlotOne, &discardedOutputEvents());

    // Deferred, because a rescan is main-thread-only and flush() is not: the
    // plugin asks for a callback and does it there.
    CHECK(host->mainThreadCallbacks > 0);
    plugin->on_main_thread(&*plugin);

    CHECK((host->rescanFlags & CLAP_PARAM_RESCAN_INFO) != 0);
    CHECK((host->rescanFlags & CLAP_PARAM_RESCAN_TEXT) != 0);
    CHECK((host->rescanFlags & CLAP_PARAM_RESCAN_VALUES) != 0);

    /// \note And never RESCAN_ALL, which is the one a host may only be given
    /// while the plugin is deactivated. This one is active.
    CHECK((host->rescanFlags & CLAP_PARAM_RESCAN_ALL) == 0);

    // The names really did move, so the rescan had something to find.
    std::uint32_t named{0};
    for (auto const &info : allParameterInfo(*plugin, params))
        if (isNormalisedType(info.id) && (std::strncmp(info.module, "Slot 1", 6) == 0) &&
            (std::strcmp(info.name, "N/A") != 0))
            ++named;
    CHECK(named > 0);
}

TEST_CASE("A host with state and no thread check survives a parameter write", "[clap]")
{
    // week_two.md §2.1a. `clap.thread-check` is optional; `clap.state` is what
    // makes markCurrentProgramAsModified() get as far as asking for it. Before
    // the fix this asserted in a checked build and dereferenced null in a
    // shipping one, on the path every automated parameter change takes.
    Entry const entry;
    CurrentStatefulHost host;
    ActivePlugin plugin(48000, 512, **host);

    auto const &params(parameters(*plugin));

    // From the main thread's side: params.flush() outside process().
    OneParameterEvent const fillSlotOne(parameterID(moduleChainType, 0), 0);
    params.flush(&*plugin, &*fillSlotOne, &discardedOutputEvents());

    // And from the audio thread's: the same write, delivered in process().
    std::vector<float> leftIn(512, 0.0f), rightIn(512, 0.0f);
    std::vector<float> leftOut(512), rightOut(512);
    plugin.process(leftIn, rightIn, leftOut, rightOut);

    // Not marked yet -- it cannot be, since the plugin has no way to establish
    // that this is the main thread, and mark_dirty is main-thread-only.
    CHECK(host->dirtyMarks == 0);
    CHECK(host->mainThreadCallbacks > 0);

    // It is the callback that discharges it, which is what the deferral is for.
    plugin->on_main_thread(&*plugin);
    CHECK(host->dirtyMarks == 1);
}

TEST_CASE("A module parameter's range and step flag survive an effect swap", "[clap]")
{
    // The reason module and LFO parameters present a fixed 0..1 edge instead of
    // their effect's real range. ext/params.h puts min_value, max_value and the
    // is_stepped flag in the CLAP_PARAM_RESCAN_ALL list -- "can only be used
    // while the plugin is deactivated" -- and a slot's effect changes through an
    // ordinary parameter event, while active. So none of the three may move, for
    // any effect, ever.
    Entry const entry;
    ActivePlugin plugin(48000, 512);

    auto const &params(parameters(*plugin));
    auto const empty(allParameterInfo(*plugin, params));

    // Every effect the slot selector can hold, not just the first.
    clap_param_info selector{};
    bool foundSelector{false};
    for (auto const &info : empty)
        if (info.id == parameterID(moduleChainType, 0))
        {
            selector = info;
            foundSelector = true;
        }
    REQUIRE(foundSelector);

    // The selector itself is one of the parameters that keeps its real range, so
    // it is a discrete choice in a host's generic panel rather than a bare 0..1.
    CHECK((selector.flags & CLAP_PARAM_IS_STEPPED) != 0);
    CHECK(selector.max_value > selector.min_value);

    for (double effect(selector.min_value); effect <= selector.max_value; ++effect)
    {
        OneParameterEvent const swap(selector.id, effect);
        params.flush(&*plugin, &*swap, &discardedOutputEvents());

        auto const filled(allParameterInfo(*plugin, params));
        REQUIRE(filled.size() == empty.size());

        for (std::size_t index(0); index < filled.size(); ++index)
        {
            REQUIRE(filled[index].id == empty[index].id);
            CHECK(filled[index].min_value == empty[index].min_value);
            CHECK(filled[index].max_value == empty[index].max_value);
            CHECK((filled[index].flags & CLAP_PARAM_IS_STEPPED) ==
                  (empty[index].flags & CLAP_PARAM_IS_STEPPED));

            // And the edge those module parameters sit on is the unit interval.
            if (isNormalisedType(filled[index].id))
            {
                CHECK(filled[index].min_value == 0);
                CHECK(filled[index].max_value == 1);
                CHECK((filled[index].flags & CLAP_PARAM_IS_STEPPED) == 0);
            }
        }
    }
}

TEST_CASE("A normalised parameter round-trips through the host edge", "[clap]")
{
    // What the host writes is what the host reads back, even though the engine
    // stored it in the effect's own units in between.
    Entry const entry;
    ActivePlugin plugin(48000, 512);

    auto const &params(parameters(*plugin));

    OneParameterEvent const fillSlotOne(parameterID(moduleChainType, 0), 0);
    params.flush(&*plugin, &*fillSlotOne, &discardedOutputEvents());

    /// \note Every module parameter the effect in slot 1 owns, not just the
    /// first: some of them are discrete underneath -- a bool, an enumeration --
    /// and a 0..1 edge cannot represent that. Writing 0.25 to a boolean and
    /// reading back 0 is correct behaviour, not a round-trip failure.
    ///
    ///   So the property asserted for all of them is the weaker one a host
    /// actually relies on: whatever comes back is *stable*. Write it again and it
    /// does not drift. Exactness is then asserted separately, over the
    /// continuous ones, so that "everything snapped to zero" cannot pass.
    std::uint32_t stable{0}, exact{0};
    for (auto const &info : allParameterInfo(*plugin, params))
    {
        if (!isNormalisedType(info.id) || ((info.flags & CLAP_PARAM_IS_HIDDEN) != 0))
            continue;

        constexpr double wanted{0.25};
        OneParameterEvent const edit(info.id, wanted);
        params.flush(&*plugin, &*edit, &discardedOutputEvents());

        double read{-1};
        REQUIRE(params.get_value(&*plugin, info.id, &read));
        CHECK(read >= 0);
        CHECK(read <= 1);

        OneParameterEvent const again(info.id, read);
        params.flush(&*plugin, &*again, &discardedOutputEvents());

        double reread{-1};
        REQUIRE(params.get_value(&*plugin, info.id, &reread));
        CAPTURE(info.id, info.name);
        CHECK_THAT(reread, Catch::Matchers::WithinAbs(read, 1e-4));
        ++stable;

        if (std::abs(read - wanted) < 1e-4)
            ++exact;
    }
    CHECK(stable > 0);
    CHECK(exact > 0);
}

TEST_CASE("A normalised parameter still reads in the effect's own units", "[clap]")
{
    // Normalising the range is only tolerable because the *text* is not
    // normalised: a host showing 0.25 also shows what 0.25 means. This is where
    // the enumerated module parameters keep their names, too, now that they can
    // no longer advertise a step count.
    //
    /// \note What is checked is that the text is in the effect's units, not that
    /// it answers about the value passed in. paramsValueToText deliberately
    /// renders the parameter's own value and ignores the argument -- see the note
    /// there on the printer and dynamic ranges -- so asking about a particular
    /// value would be asserting the opposite of what the code does. The value
    /// below is arbitrary for that reason.
    Entry const entry;
    ActivePlugin plugin(48000, 512);

    auto const &params(parameters(*plugin));

    OneParameterEvent const fillSlotOne(parameterID(moduleChainType, 0), 0);
    params.flush(&*plugin, &*fillSlotOne, &discardedOutputEvents());

    std::uint32_t checked{0}, differedFromTheEdge{0};
    for (auto const &info : allParameterInfo(*plugin, params))
    {
        if (!isNormalisedType(info.id) || ((info.flags & CLAP_PARAM_IS_HIDDEN) != 0))
            continue;

        std::array<char, 128> text{};
        REQUIRE(params.value_to_text(&*plugin, info.id, 1.0, text.data(), text.size()));
        CAPTURE(info.id, info.name, info.module);
        CHECK(text[0] != '\0');
        ++checked;

        // Some parameter in the slot has to read as something other than a bare
        // edge value, or nothing is being converted into the effect's units at
        // all -- which is the whole justification for a 0..1 range.
        if (std::strncmp(text.data(), "1", 2) != 0)
            ++differedFromTheEdge;
    }
    CHECK(checked > 0);
    CHECK(differedFromTheEdge > 0);
}

TEST_CASE("Silence in is silence out", "[clap]")
{
    constexpr std::uint32_t blockSize{512};

    Entry const entry;
    ActivePlugin plugin(48000, blockSize);

    std::vector<float> leftIn(blockSize, 0.0f), rightIn(blockSize, 0.0f);
    std::vector<float> leftOut(blockSize), rightOut(blockSize);

    for (unsigned int block(0); block < 8; ++block)
    {
        // Deliberately dirty, so that "silence out" means the engine wrote it
        // rather than that nobody touched the buffer.
        std::fill(leftOut.begin(), leftOut.end(), 0.5f);
        std::fill(rightOut.begin(), rightOut.end(), 0.5f);

        plugin.process(leftIn, rightIn, leftOut, rightOut);

        CHECK(peak(leftOut) < 1.0e-6f);
        CHECK(peak(rightOut) < 1.0e-6f);
    }
}

////////////////////////////////////////////////////////////////////////////////
// LFO timing
//
//   All three of these failed before SpectrumWorxCLAP::updateLFOTiming() existed.
// The engine's LFO clock only ever moved in SpectrumWorxSharedImpl::process(),
// the 2016 host layer the CLAP does not inherit, so an enabled LFO answered with
// its value at position 0 for the plugin's lifetime -- which is what "setting up
// an LFO does not modulate anything" was.
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("An enabled LFO modulates when the host reports no transport", "[clap][lfo]")
{
    // The standalone, and any host that does not fill in transport. There is no
    // song position to follow, so the block length has to move the clock.
    constexpr float sampleRate{48000};
    constexpr std::uint32_t blockSize{512};
    // ~0.85 of a bar at the engine's assumed 120 BPM 4/4, so a default one-bar
    // sine sweeps most of its range whatever the parameter's resolution.
    constexpr unsigned int blocks{160};

    Entry const entry;
    ActivePlugin plugin(sampleRate, blockSize);

    CHECK(distinctModulatedValues(plugin, parameters(*plugin), sampleRate, blockSize, blocks,
                                  nullptr) > 1);
}

TEST_CASE("An enabled LFO keeps running while the transport is stopped", "[clap][lfo]")
{
    // A parked transport reports the same song position every block, so following
    // it would freeze every LFO on the rack -- which is not what auditioning a
    // patch without pressing play should do. Six Sines and surge-xt2 both keep
    // the LFO moving here, and so does this: the host's tempo and meter are kept,
    // and the phase is carried forward by the block length.
    constexpr float sampleRate{48000};
    constexpr std::uint32_t blockSize{512};
    constexpr unsigned int blocks{160};

    Entry const entry;
    ActivePlugin plugin(sampleRate, blockSize);

    auto const stopped(transportAt(120, 8 /*bar 3*/, 0 /*not CLAP_TRANSPORT_IS_PLAYING*/));
    CHECK(distinctModulatedValues(plugin, parameters(*plugin), sampleRate, blockSize, blocks,
                                  &stopped) > 1);
}

TEST_CASE("A playing transport drives the LFO from song position", "[clap][lfo]")
{
    // The case the other two stand in for: the host is playing and the LFO is
    // phase-locked to the song rather than free-running alongside it.
    constexpr float sampleRate{48000};
    constexpr std::uint32_t blockSize{512};

    Entry const entry;
    ActivePlugin plugin(sampleRate, blockSize);
    auto const &params(parameters(*plugin));

    OneParameterEvent const fillSlotOne(parameterID(moduleChainType, 0), 0);
    params.flush(&*plugin, &*fillSlotOne, &discardedOutputEvents());

    OneParameterEvent const enable(lfoParameterID(0, 0, lfoEnabled), 1);
    params.flush(&*plugin, &*enable, &discardedOutputEvents());

    auto const target(modulatedParameterID(0, 0));

    std::vector<float> leftIn(blockSize, 0.0f), rightIn(blockSize, 0.0f);
    std::vector<float> leftOut(blockSize), rightOut(blockSize);

    /// \brief What the LFO reads with the song parked at \p beats.
    ///
    /// \note One block per position, and the position is what the host says
    /// rather than what the last block left behind -- so a repeat has to read the
    /// same as its first visit, which a free-running clock could not manage.
    auto const valueAtBeat([&](double const beats) {
        auto const playing(transportAt(120, beats, CLAP_TRANSPORT_IS_PLAYING));
        plugin.process(leftIn, rightIn, leftOut, rightOut, &playing);
        double value{0};
        REQUIRE(params.get_value(&*plugin, target, &value));
        return value;
    });

    // A default LFO is a one-bar sine, so bar starts agree and mid-bar does not.
    auto const barZero(valueAtBeat(0));
    auto const midBar(valueAtBeat(2));
    auto const barOne(valueAtBeat(4));

    CHECK(midBar != barZero);
    CHECK(barOne == barZero);
}
