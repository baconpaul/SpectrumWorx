////////////////////////////////////////////////////////////////////////////////
///
/// mixTransparencyTests.cpp
/// ------------------------
///
///   **At Mix 0 the output is the input, delayed by the latency the plugin
/// reports and unchanged otherwise.** The wet term is multiplied by the mix and
/// the consumed input hop added back at `1 - mix`, so at 0 the claim is
/// exactness rather than a tolerance, and the FFT size, the overlap factor and
/// whatever is loaded are all supposed to be invisible.
///
/// \see doc/tech/latency.md, and chunkTransparencyTests.cpp, which measures the
/// same delay through the wet path.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "core/mixResidue.hpp"
#include "goldens/engineHarness.hpp"

#include "le/spectrumworx/effects/configuration/constants.hpp"
#include "le/spectrumworx/effects/configuration/effectNames.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>
//------------------------------------------------------------------------------
namespace
{
namespace Effects = LE::SW::Effects;

using SWTest::leadingPeak;
using SWTest::mixResidue;

constexpr std::uint32_t sampleRate{44100};
constexpr std::uint8_t channels{2};
constexpr std::uint32_t renderedFrames{5 * sampleRate};

// not a multiple of any hop here, so every block ends on a short call
constexpr std::uint32_t hostBlock{500};

/// \brief One channel of an interleaved render, de-interleaved.
std::vector<float> channelOf(std::span<float const> const interleaved, std::uint8_t const channel)
{
    std::vector<float> mono(interleaved.size() / channels);
    for (std::size_t frame(0); frame < mono.size(); ++frame)
        mono[frame] = interleaved[frame * channels + channel];
    return mono;
}

std::vector<float> sweep()
{
    std::vector<float> input(renderedFrames);
    SWTest::generate(SWTest::Signal::Sweep, input, static_cast<float>(sampleRate));
    return input;
}

SWTest::RenderSetup setupFor(std::uint16_t const fftSize, std::uint8_t const overlapFactor)
{
    SWTest::RenderSetup setup{fftSize, overlapFactor, channels, sampleRate, hostBlock};
    // what SpectrumWorxCLAP::process() cuts a host block into, clamped as it clamps
    setup.callSize = std::min<std::uint32_t>(fftSize / overlapFactor, hostBlock);
    setup.mix = 0.0f;
    return setup;
}

void requireTransparent(SWTest::RenderSetup const &setup, std::span<float const> const input,
                        std::span<float const> const rendered)
{
    for (std::uint8_t channel(0); channel < channels; ++channel)
    {
        auto const mono(channelOf(rendered, channel));
        auto const residue(mixResidue(mono, input, setup.fftSize));
        auto const leading(leadingPeak(mono, setup.fftSize));

        INFO("fft " << setup.fftSize << '/' << unsigned(setup.overlapFactor) << ", channel "
                    << unsigned(channel) << ": worst " << residue.worst << " at " << residue.worstAt
                    << " of " << residue.compared << ", residue " << residue.errorDb()
                    << " dB, output peak " << residue.outputPeak << ", latency region peak "
                    << leading);

        // or "worst 0" is a comparison of nothing against nothing
        REQUIRE(residue.compared > 0);

        // exact, because x * 0 + y * 1 is y; a tolerance would accept the report
        CHECK(residue.worst == 0.0);
        CHECK(leading == 0.0);
    }
}

} // anonymous namespace

TEST_CASE("Mix 0 returns the input at every FFT size", "[mix][latency]")
{
    // minimumFFTSize to maximumFFTSize: the whole of what a host can ask for
    auto const fftSize(GENERATE(std::uint16_t{128}, 256, 512, 1024, 2048, 4096, 8192));
    auto const setup(setupFor(fftSize, 4));

    auto const input(sweep());
    SWTest::Slot const bypassed[]{{-1, {}}};
    requireTransparent(setup, input, SWTest::renderChain(setup, bypassed, input));
}

TEST_CASE("Mix 0 returns the input at every overlap factor", "[mix][latency]")
{
    auto const overlapFactor(GENERATE(std::uint8_t{1}, 2, 4, 8));
    auto const setup(setupFor(2048, overlapFactor));

    auto const input(sweep());
    SWTest::Slot const bypassed[]{{-1, {}}};
    requireTransparent(setup, input, SWTest::renderChain(setup, bypassed, input));
}

/// \note Every effect, because exactness survives a multiply by zero and a
/// non-finite wet sample would not.
TEST_CASE("Mix 0 makes every effect unobservable", "[mix]")
{
    auto const setup(setupFor(2048, 4));
    auto const input(sweep());

    for (int effect(0); effect < Effects::Constants::numberOfEffects; ++effect)
    {
        SWTest::Slot const loaded[]{{static_cast<std::int8_t>(effect), {}}};
        auto const rendered(SWTest::renderChain(setup, loaded, input));
        auto const residue(mixResidue(channelOf(rendered, 0), input, setup.fftSize));

        INFO(Effects::effectStreamingName(effect)
             << ": worst " << residue.worst << " at " << residue.worstAt << ", residue "
             << residue.errorDb() << " dB, peak " << residue.outputPeak);
        REQUIRE(residue.compared > 0);
        CHECK(residue.worst == 0.0);
    }
}

////////////////////////////////////////////////////////////////////////////////
///
/// \brief That the comparison above can fail, which "worst 0" does not say.
///
///   Off by one sample the sweep disagrees with itself by 0.67 of full scale,
/// and off by a hop by more than the signal -- so a dry path aligned to the
/// wrong hop would be caught rather than merely sounding right.
///
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("A misaligned dry path would be visible", "[mix][latency]")
{
    auto const setup(setupFor(2048, 4));
    auto const input(sweep());
    SWTest::Slot const bypassed[]{{-1, {}}};
    auto const rendered(SWTest::renderChain(setup, bypassed, input));

    std::uint32_t const latencyIs(setup.fftSize);
    auto const hop(latencyIs / setup.overlapFactor);
    for (auto const latency : {latencyIs - hop, latencyIs - 1, latencyIs + 1, latencyIs + hop})
    {
        auto const residue(mixResidue(channelOf(rendered, 0), input, latency));
        INFO("at " << latency << " rather than " << setup.fftSize << ": worst " << residue.worst
                   << ", residue " << residue.errorDb() << " dB");
        CHECK(residue.errorDb() > -20.0);
    }
}

////////////////////////////////////////////////////////////////////////////////
///
/// \brief The two global gains, which sit either side of the whole mix:
/// `out * (mix * process(in * x) + (1 - mix) * in * x)`. \see issue #256.
///
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("At Mix 0 both gains reach the output", "[mix][issue-256]")
{
    auto const gain(GENERATE(0.5f, 2.0f));
    auto const input(sweep());
    SWTest::Slot const bypassed[]{{-1, {}}};

    auto scaledInput(input);
    for (auto &sample : scaledInput)
        sample *= gain;

    auto withInput(setupFor(2048, 4));
    withInput.inputGain = gain;
    auto withOutput(setupFor(2048, 4));
    withOutput.outputGain = gain;

    for (auto const &[which, setup] : {std::pair{"in", withInput}, std::pair{"out", withOutput}})
    {
        auto const residue(mixResidue(channelOf(SWTest::renderChain(setup, bypassed, input), 0),
                                      scaledInput, setup.fftSize));
        INFO(which << " gain " << gain << ": worst " << residue.worst << " at " << residue.worstAt
                   << ", output peak " << residue.outputPeak);
        REQUIRE(residue.compared > 0);

        // exact for the same reason Mix 0 is, the wet term being multiplied by zero
        CHECK(residue.worst == 0.0);
    }
}

TEST_CASE("Out scales the wet and the dry alike at every Mix", "[mix][issue-256]")
{
    constexpr float out{2.0f};
    auto const mix(GENERATE(0.25f, 0.5f, 0.8f));

    auto const input(sweep());
    // an octave up, so the wet and the dry are nothing alike
    SWTest::Slot const octaver[]{{SWTest::effectByStreamingName("Octaver"), {}}};

    auto const renderAt([&](float const renderMix, float const renderOut) {
        auto setup(setupFor(2048, 4));
        setup.mix = renderMix;
        setup.outputGain = renderOut;
        return channelOf(SWTest::renderChain(setup, octaver, input), 0);
    });

    auto const wet(renderAt(1.0f, 1.0f));
    auto const dry(renderAt(0.0f, 1.0f));
    auto const mixed(renderAt(mix, out));
    REQUIRE(mixed.size() == wet.size());

    double worst{0}, worstOldLaw{0}, peak{0};
    for (std::size_t frame(0); frame < mixed.size(); ++frame)
    {
        auto const expected(double{out} *
                            (mix * double{wet[frame]} + (1 - mix) * double{dry[frame]}));
        auto const oldLaw(double{out} * mix * wet[frame] + (1 - mix) * double{dry[frame]});
        worst = std::max(worst, std::abs(mixed[frame] - expected));
        worstOldLaw = std::max(worstOldLaw, std::abs(mixed[frame] - oldLaw));
        peak = std::max(peak, std::abs(double{mixed[frame]}));
    }

    INFO("mix " << mix << ": worst " << worst << ", against the old law " << worstOldLaw
                << ", peak " << peak);
    REQUIRE(peak > 0.1);
    CHECK(worst < 1e-5);
    // or the comparison above could not tell the two laws apart
    CHECK(worstOldLaw > 0.01);
}
