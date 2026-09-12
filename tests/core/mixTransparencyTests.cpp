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

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
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
/// \brief What the two global gains do at Mix 0, which is not the same thing.
///
///   `outputScaling_` is `outputGain * mix`, so at 0 the Out control is dead;
/// the dry is added at `1 - mix` with no gain of its own, so In reaches the
/// output in full. Factory presets that ship In away from unity are therefore
/// louder at Mix 0 than the track they are on, and Out cannot bring them back.
///
/// \note Pinned rather than fixed: either half is a change to what a preset
/// sounds like at Mix 0.
///
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("At Mix 0 the In gain reaches the output and the Out gain does not", "[mix]")
{
    auto const gain(GENERATE(0.5f, 2.0f));
    auto const input(sweep());
    SWTest::Slot const bypassed[]{{-1, {}}};

    auto withInput(setupFor(2048, 4));
    withInput.inputGain = gain;
    auto const scaled(mixResidue(channelOf(SWTest::renderChain(withInput, bypassed, input), 0),
                                 input, withInput.fftSize));

    auto withOutput(setupFor(2048, 4));
    withOutput.outputGain = gain;
    auto const unscaled(mixResidue(channelOf(SWTest::renderChain(withOutput, bypassed, input), 0),
                                   input, withOutput.fftSize));

    REQUIRE(scaled.compared > 0);
    REQUIRE(unscaled.compared > 0);
    INFO("gain " << gain << ": in gives peak " << scaled.outputPeak << ", out gives peak "
                 << unscaled.outputPeak);
    CHECK(scaled.outputPeak == Catch::Approx(gain * 0.5).epsilon(1e-6));
    CHECK(unscaled.worst == 0.0);
}
