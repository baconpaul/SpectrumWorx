////////////////////////////////////////////////////////////////////////////////
///
/// mixThroughThePluginTests.cpp
/// ----------------------------
///
///   `core/mixTransparencyTests.cpp` asks the engine whether Mix 0 returns the
/// input. This asks the **plugin**: a real `clap_plugin` from the factory, five
/// seconds of sweep through `process()` in host blocks, compared against the
/// latency the plugin told the host through `clap_plugin_latency::get()`.
///
///   What that adds is everything between a host and the engine -- the input
/// gain, the chunking `process()` does at the hop, the parameter edge a Mix of 0
/// crosses, and the restart a spectral change needs.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "testHost.hpp"

#include "core/mixResidue.hpp"
#include "goldens/engineHarness.hpp"

#include "le/parameters/parameter.hpp"
#include "le/spectrumworx/engine/parameters.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>
//------------------------------------------------------------------------------
namespace
{
using namespace SWTest;
namespace Globals = LE::SW::GlobalParameters;

constexpr double sampleRate{48000};

/// \note Not a multiple of any hop, so every block ends on the short call
/// `SpectrumWorxCLAP::process()` cuts for itself. \see doc/tech/latency.md §4.
constexpr std::uint32_t blockSize{500};

constexpr std::uint32_t renderedFrames{5 * 48000};

/// Where a global parameter sits in `GlobalParameters::Parameters`, which is
/// what both the host id and `editGlobalParameter` are keyed on.
template <class Parameter> constexpr std::uint8_t globalIndex()
{
    return LE::Parameters::IndexOf<Globals::Parameters, Parameter>::value;
}

std::uint32_t reportedLatency(clap_plugin const &plugin)
{
    auto const *const latency(
        static_cast<clap_plugin_latency const *>(plugin.get_extension(&plugin, CLAP_EXT_LATENCY)));
    REQUIRE(latency != nullptr);
    return latency->get(&plugin);
}

/// \brief What the host reads back for \p id, so that "it was set" is checked
/// rather than assumed.
double readsBack(ActivePlugin const &plugin, clap_id const id)
{
    double value{-1};
    REQUIRE(parameters(*plugin).get_value(&*plugin, id, &value));
    return value;
}

struct Rendered
{
    std::vector<float> left, right;
};

/// \brief Five seconds of \p input through the plugin, in host blocks.
Rendered render(ActivePlugin &plugin, std::span<float const> const input)
{
    Rendered out{std::vector<float>(input.size()), std::vector<float>(input.size())};

    std::vector<float> leftIn(blockSize), rightIn(blockSize);
    std::vector<float> leftOut(blockSize), rightOut(blockSize);

    for (std::uint32_t at(0); at + blockSize <= input.size(); at += blockSize)
    {
        std::copy_n(input.begin() + at, blockSize, leftIn.begin());
        rightIn = leftIn;
        std::fill(leftOut.begin(), leftOut.end(), 0.0f);
        std::fill(rightOut.begin(), rightOut.end(), 0.0f);

        plugin.process(leftIn, rightIn, leftOut, rightOut);

        std::copy_n(leftOut.begin(), blockSize, out.left.begin() + at);
        std::copy_n(rightOut.begin(), blockSize, out.right.begin() + at);
    }
    return out;
}

void requireTransparent(Rendered const &rendered, std::span<float const> const input,
                        std::uint32_t const latency, char const *const what)
{
    for (auto const *const channel : {&rendered.left, &rendered.right})
    {
        auto const residue(mixResidue(*channel, input, latency));
        INFO(what << " at a declared latency of " << latency << ": worst " << residue.worst
                  << " at " << residue.worstAt << " of " << residue.compared << ", residue "
                  << residue.errorDb() << " dB, output peak " << residue.outputPeak
                  << ", latency region peak " << leadingPeak(*channel, latency));

        REQUIRE(residue.compared > 0);
        CHECK(residue.worst == 0.0);
        CHECK(leadingPeak(*channel, latency) == 0.0);
    }
}

std::vector<float> sweep()
{
    std::vector<float> input(renderedFrames);
    SWTest::generate(SWTest::Signal::Sweep, input, static_cast<float>(sampleRate));
    return input;
}

/// \brief The id a host addresses a global parameter by.
///
/// \note The *module* field of `SWTest::parameterID` and not the parameter one:
/// `ParameterID::Global` packs its index at bits 16-23. In the last field it
/// addresses nothing, and a flush of it is dropped in silence.
template <class Parameter> clap_id globalParameterID()
{
    return parameterID(globalType, globalIndex<Parameter>());
}

/// \brief Mix, set the way a host automating it does.
void setMixThroughTheHost(ActivePlugin const &plugin, double const mix)
{
    OneParameterEvent const event(globalParameterID<Globals::MixPercentage>(), mix);
    plugin.flush(&*event);
    REQUIRE(readsBack(plugin, globalParameterID<Globals::MixPercentage>()) == mix);
}

} // anonymous namespace

/// \note Mix through the host rather than the editor, because the conversion is
/// part of the question: a mix a hair above zero leaks the wet path.
TEST_CASE("Mix 0 through the plugin returns the input", "[clap][mix][latency]")
{
    Entry const entry;
    TestHost host(TestHost::everything());
    ActivePlugin plugin(sampleRate, blockSize, host);

    setMixThroughTheHost(plugin, 0.0);

    auto const input(sweep());
    requireTransparent(render(plugin, input), input, reportedLatency(*plugin), "the default chain");

    CHECK(host.misbehaviours().empty());
}

////////////////////////////////////////////////////////////////////////////////
///
/// \brief The FFT size moved under a running instance, and the delay with it.
///
///   Playing at 2048, the settings page picks 1024, `drainCommands()` answers
/// with `request_restart`, the host deactivates and reactivates, and `activate()`
/// announces the new latency. The same instance then has to be transparent at
/// the new number, everything underneath it having been rebuilt.
///
/// \note Downwards, so that a stale buffer is still large enough to write into
/// and a stale *latency* is the only thing left to be wrong.
///
/// \note `editGlobalParameter` is the UI's route -- what
/// `SpectrumWorxEditor::queueGlobalParameter` calls -- because the FFT size is
/// deliberately not automatable. \see issue #171.
///
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Mix 0 survives the UI changing the FFT size under a running instance",
          "[clap][mix][latency]")
{
    Entry const entry;
    TestHost host(TestHost::everything());
    ActivePlugin plugin(sampleRate, blockSize, host);

    setMixThroughTheHost(plugin, 0.0);

    auto const input(sweep());

    auto const before(reportedLatency(*plugin));
    CHECK(before == 2048); // the default, which this case is written around
    requireTransparent(render(plugin, input), input, before, "before the change");

    auto const restartsBefore(host.restartRequests.load());
    auto const latencyChangesBefore(host.latencyChanges.load());

    editorHostOf(*plugin).editGlobalParameter(globalIndex<Globals::FFTSize>(), 1024.0f);

    // nothing drains the queue but process(), flush() and deactivate()
    plugin.flush();
    CHECK(host.restartRequests.load() > restartsBefore);

    // still the old size until the host comes back, which is the contract
    CHECK(reportedLatency(*plugin) == before);

    plugin.restartIfAsked();

    auto const after(reportedLatency(*plugin));
    CHECK(after == 1024);
    CHECK(host.latencyChanges.load() > latencyChangesBefore);

    auto const rendered(render(plugin, input));
    requireTransparent(rendered, input, after, "after the restart");

    // and it really moved: the same audio against the delay it used to have
    auto const atTheOldLatency(mixResidue(rendered.left, input, before));
    INFO("at the old latency of " << before << ": worst " << atTheOldLatency.worst << ", residue "
                                  << atTheOldLatency.errorDb() << " dB");
    CHECK(atTheOldLatency.errorDb() > -20.0);

    CHECK(host.misbehaviours().empty());
}
