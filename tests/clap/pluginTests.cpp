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

#include <clap/clap.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numbers>
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
    ActivePlugin(double const sampleRate, std::uint32_t const blockSize) : blockSize_(blockSize)
    {
        pPlugin_ = factory().create_plugin(&factory(), &nullHost(), descriptorID());
        REQUIRE(pPlugin_ != nullptr);
        REQUIRE(pPlugin_->init(pPlugin_));
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
    void process(std::vector<float> &leftIn, std::vector<float> &rightIn,
                 std::vector<float> &leftOut, std::vector<float> &rightOut)
    {
        float *inputChannels[]{leftIn.data(), rightIn.data()};
        float *outputChannels[]{leftOut.data(), rightOut.data()};

        clap_audio_buffer input{&inputChannels[0], nullptr, 2, 0, 0};
        clap_audio_buffer output{&outputChannels[0], nullptr, 2, 0, 0};

        clap_process process{};
        process.steady_time = -1;
        process.frames_count = blockSize_;
        process.transport = nullptr;
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
        CHECK(info.min_value <= info.max_value);
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

TEST_CASE("Filling a module slot grows the host's parameter list", "[clap]")
{
    // The whole reason this port targets CLAP. A slot's effect is itself a
    // parameter, and setting it changes how many parameters exist and what they
    // are called -- which a fixed parameter table cannot express.
    Entry const entry;
    ActivePlugin plugin(48000, 512);

    auto const *const params(
        static_cast<clap_plugin_params const *>(plugin->get_extension(&*plugin, CLAP_EXT_PARAMS)));
    REQUIRE(params != nullptr);

    auto const emptyCount(params->count(&*plugin));

    /// \note ParameterID's members are laid out in reverse so that the hex reads
    /// naturally on a little-endian machine: the type is the top byte and the
    /// module index the one below it. See core/parameterID.hpp.
    constexpr clap_id moduleChainType{1};
    clap_id const slotZero((moduleChainType << 24) | (0u << 16));

    clap_event_param_value event{};
    event.header.size = sizeof(event);
    event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    event.header.type = CLAP_EVENT_PARAM_VALUE;
    event.param_id = slotZero;
    event.note_id = event.port_index = event.channel = event.key = -1;
    event.value = 0; // the first effect in the list

    struct OneEvent
    {
        clap_input_events list;
        clap_event_param_value const *event;
    };
    OneEvent one{{&one, [](clap_input_events const *) -> std::uint32_t { return 1; },
                  [](clap_input_events const *self, std::uint32_t) -> clap_event_header const * {
                      return &static_cast<OneEvent const *>(self->ctx)->event->header;
                  }},
                 &event};

    params->flush(&*plugin, &one.list, &discardedOutputEvents());

    auto const filledCount(params->count(&*plugin));
    CHECK(filledCount > emptyCount);

    // And the new ones are the slot's, named for it.
    bool sawSlotOne{false};
    for (std::uint32_t index(0); index < filledCount; ++index)
    {
        clap_param_info info{};
        REQUIRE(params->get_info(&*plugin, index, &info));
        if (std::strcmp(info.module, "Slot 1") == 0)
            sawSlotOne = true;
    }
    CHECK(sawSlotOne);
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
