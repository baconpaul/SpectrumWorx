////////////////////////////////////////////////////////////////////////////////
///
/// amplifyingEffectsTests.cpp
/// --------------------------
///
///   Properties for the nine effects the golden contract deliberately holds
/// loosely, because a one-ulp FFT difference becomes a percent-level output
/// difference in each of them.
///
///   Those nine -- Pitch Spring, Pitch Spring (PV), Pitch Magnet, Octaver,
/// To PV, From PV, Imploder, Exploder and Slew Limiter -- each make a
/// *decision* somewhere: a pitch detector picks a maximum, a phase vocoder
/// unwraps a phase, the ex/imploder thresholds a bin, the slew limiter compares
/// a rate of change against a limit. One ulp of difference in the spectrum flips
/// a comparison, the chosen bin moves, and the output moves by percent -- 21 %
/// on a peak for Pitch Spring between macOS/Accelerate and Linux/pffft. So
/// `Tolerances::amplified()` is what they are held to off the machine that minted
/// the fixture file, and a bound that wide is not much of a test.
///
///   These are the test instead. Not "the output is these numbers" but "the
/// effect does what it is called": a magnet lands on its target where the
/// resynthesis can carry it, a spring oscillates and in the direction it was
/// told to, an octaver puts energy an octave away, To PV and From PV are
/// inverses, an imploder sustains, an exploder grows, a slew limiter slows a
/// change down. None of that moves when a bin does, so all of it holds on any
/// platform and in either build type -- the goldens render in Release only, and
/// these do not.
///
/// \note The properties are deliberately one-sided where the effect is: "Up
/// never goes below the input pitch" is the guarantee worth making, "Up reaches
/// exactly +N cents" is a claim about a pitch detector's accuracy and would be a
/// tolerance argument dressed up as a property. One-sided is not the same as
/// unconditional -- that first one holds of pitch as a 30 ms window measures it,
/// and not of every cycle of the waveform.
///
/// \note And where an effect does not do what it is called, the case says so
/// rather than picking a setting that reads well. A two-octave pitch magnet puts
/// out three tones and the target is not reliably the loudest; a pitch scale the
/// shifter carries cleanly when it is held still comes apart when it is swept.
/// Both are recorded here, with the arithmetic that produces them, against the
/// day the phase-locked rewrite in issue #246 replaces them. A test that cannot
/// see the defect cannot tell you the fix worked either.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "goldens/engineHarness.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <span>
#include <string_view>
#include <vector>
//------------------------------------------------------------------------------
namespace
{
using SWTest::Slot;

constexpr double pi{std::numbers::pi};

constexpr std::uint32_t sampleRate{44100};
constexpr std::uint8_t channels{2};

/// \note 1024/4 rather than the goldens' 512/4 and 2048/8. The step is then
/// 256 samples, 172 a second, which is what every "per second" parameter here
/// is quantised to -- a slew rate, a glissando, a magnet's speed. Fine enough
/// that a 200 ms oscillation has ~34 frames to draw itself with, coarse enough
/// that a one second render is a few hundred frames rather than a few thousand
/// in a checked build.
constexpr SWTest::RenderSetup standardSetup{1024, 4, channels, sampleRate, 256};

/// \note 2048 for the properties that are claims about a *frequency* rather than
/// about a level or a direction. Overlap 4 because it is the shipping default
/// -- `Constants::defaultOverlapFactor` -- and what a shift does at the setting
/// people run it at is the thing worth pinning.
///
///   No setting here is chosen to make the pitch magnet read well. It cannot be:
/// a two-octave shift aliases at every FFT size this engine offers, and which of
/// the target and its two images comes out loudest is not stable across them.
/// \see the two magnet cases below, which record that rather than dodge it.
constexpr SWTest::RenderSetup pitchSetup{2048, 4, channels, sampleRate, 256};

constexpr std::uint32_t oneSecond{sampleRate};

/// \note By streaming name, like everywhere else a test names an effect: a
/// title is free to move and did -- "PVD start" became "To PV" -- which broke
/// six cases here that were about phase vocoders and not about spelling.
/// \see SWTest::effectByStreamingName().
using SWTest::effectByStreamingName;

//------------------------------------------------------------------------------
// Signals
//------------------------------------------------------------------------------

/// A pure tone. One partial, so "the dominant frequency" has one answer.
std::vector<float> tone(double const frequency, std::uint32_t const frames,
                        float const amplitude = 0.5f)
{
    std::vector<float> signal(frames);
    for (std::uint32_t frame(0); frame < frames; ++frame)
        signal[frame] =
            amplitude * static_cast<float>(std::sin(2 * pi * frequency * frame / sampleRate));
    return signal;
}

/// A tone that starts at \p onset and stops at \p offset, both in frames. What
/// an envelope property needs: an attack to be slowed and a release to be held.
std::vector<float> gatedTone(double const frequency, std::uint32_t const frames,
                             std::uint32_t const onset, std::uint32_t const offset)
{
    auto signal(tone(frequency, frames));
    std::fill(signal.begin(), signal.begin() + std::min(onset, frames), 0.0f);
    if (offset < frames)
        std::fill(signal.begin() + offset, signal.end(), 0.0f);
    return signal;
}

//------------------------------------------------------------------------------
// Measurement
//------------------------------------------------------------------------------

/// One channel of an interleaved render, as a span over a frame window.
std::vector<float> window(std::span<float const> interleaved, std::uint32_t const first,
                          std::uint32_t const count, std::uint8_t const channel = 0)
{
    std::vector<float> mono;
    mono.reserve(count);
    auto const frames(interleaved.size() / channels);
    for (std::uint32_t frame(first); (frame < first + count) && (frame < frames); ++frame)
        mono.push_back(interleaved[static_cast<std::size_t>(frame) * channels + channel]);
    return mono;
}

float rms(std::span<float const> mono)
{
    if (mono.empty())
        return 0;
    double sum{0};
    for (auto const sample : mono)
        sum += static_cast<double>(sample) * sample;
    return static_cast<float>(std::sqrt(sum / mono.size()));
}

float peak(std::span<float const> mono)
{
    float largest{0};
    for (auto const sample : mono)
        largest = std::max(largest, std::abs(sample));
    return largest;
}

bool allFinite(std::span<float const> samples)
{
    return std::all_of(samples.begin(), samples.end(),
                       [](float const sample) { return std::isfinite(sample); });
}

/// \brief The magnitude of one frequency, Hann-windowed.
///
/// \note A single-frequency DFT rather than a whole transform. What these
/// properties ask is always "how much is there at *this* frequency" or "where is
/// the largest of these candidates", and both are cheaper and clearer this way
/// than through a spectrum whose bin spacing then has to be argued with.
double magnitudeAt(std::span<float const> mono, double const frequency)
{
    auto const count(mono.size());
    if (count < 2)
        return 0;
    double real{0}, imaginary{0};
    for (std::size_t n(0); n < count; ++n)
    {
        auto const w(0.5 * (1 - std::cos(2 * pi * static_cast<double>(n) / (count - 1))));
        auto const angle(-2 * pi * frequency * static_cast<double>(n) / sampleRate);
        real += mono[n] * w * std::cos(angle);
        imaginary += mono[n] * w * std::sin(angle);
    }
    return std::sqrt(real * real + imaginary * imaginary) / count;
}

////////////////////////////////////////////////////////////////////////////////
///
/// \brief The loudest frequency between \p lowest and \p highest, in Hz.
///
///   Swept linearly at the analysed window's own resolution, then refined in
/// cents around the winner.
///
/// \note The sweep step has to come from the window rather than from the ear.
/// A Hann window of \p mono samples resolves `sampleRate / size` and its main
/// lobe is twice that wide, so a step of one resolution unit cannot straddle a
/// peak. A grid in *cents* can, and did: at 25 cents the samples near 880 Hz
/// fall 12 Hz apart while a half-second window's lobe is 8 Hz wide, so the pitch
/// magnet's loudest partial -- an alias at 966 Hz, above the 880 the effect was
/// asked for -- landed between two samples and read as 0.0004 against the
/// target's 0.067. The case then reported "+0.2 cents" and passed. \see #19.
///
////////////////////////////////////////////////////////////////////////////////
double dominantFrequency(std::span<float const> mono, double const lowest = 60,
                         double const highest = 4000)
{
    if (mono.size() < 2)
        return lowest;

    double best{lowest}, bestMagnitude{-1};
    auto const consider([&](double const frequency) {
        auto const magnitude(magnitudeAt(mono, frequency));
        if (magnitude > bestMagnitude)
        {
            bestMagnitude = magnitude;
            best = frequency;
        }
    });

    auto const resolution(sampleRate / static_cast<double>(mono.size()));
    for (double frequency(lowest); frequency <= highest; frequency += resolution)
        consider(frequency);

    // then in cents, one resolution unit either side, for a reading finer than
    // the sweep that found it
    auto const coarse(best);
    bestMagnitude = -1;
    auto const step(std::pow(2.0, 2.0 / 1200));
    for (double frequency(std::max(lowest, coarse - resolution));
         frequency <= std::min(highest, coarse + resolution); frequency *= step)
        consider(frequency);
    return best;
}

double cents(double const from, double const to) { return 1200 * std::log2(to / from); }

/// How far apart two renders of the same length are, relative to the louder.
float relativeDifference(std::span<float const> a, std::span<float const> b)
{
    REQUIRE(a.size() == b.size());
    float largestDifference{0};
    for (std::size_t index(0); index < a.size(); ++index)
        largestDifference = std::max(largestDifference, std::abs(a[index] - b[index]));
    auto const reference(std::max(peak(a), peak(b)));
    return (reference > 0) ? (largestDifference / reference) : largestDifference;
}

//------------------------------------------------------------------------------
// The chains under test
//------------------------------------------------------------------------------

/// Base parameter indices, in Effects::BaseParameters' declaration order.
enum BaseParameter : std::uint8_t
{
    bypass = 0,
    gain = 1,
    wet = 2,
    startFrequency = 3,
    stopFrequency = 4
};

using Module = LE::SW::Engine::ModuleParameters;

/// The chain with nothing in it: the engine's own analysis/resynthesis, which
/// is what "transparent" is measured against. An effect cannot be compared to
/// its input -- there is an FFT's worth of latency and a window in between.
std::vector<float> dryRender(std::span<float const> input,
                             SWTest::RenderSetup const &setup = standardSetup)
{
    Slot const empty[]{{-1, {}}};
    return SWTest::renderChain(setup, empty, input);
}

std::vector<float> renderOne(std::string_view const name, std::span<float const> input,
                             std::function<void(Module &)> configure = {},
                             SWTest::RenderSetup const &setup = standardSetup)
{
    Slot const slots[]{{effectByStreamingName(name), std::move(configure)}};
    return SWTest::renderChain(setup, slots, input);
}

/// The nine, by the names the golden keys carry -- which are streaming names.
constexpr std::string_view amplifyingEffects[]{
    "Pitch Spring", "Pitch Spring (pvd)", "Pitch Magnet", "Octaver",      "PVD start",
    "PVD stop",     "Imploder",           "Exploder",     "Slew Limiter",
};
} // anonymous namespace

////////////////////////////////////////////////////////////////////////////////
// What holds for all nine
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("A bypassed amplifying effect is exactly the empty chain", "[effects][property]")
{
    // The strongest transparency claim available, and it has to be exact: bypass
    // is a branch taken before any of the effect's own code, so anything other
    // than bit-equality means the *chain* changed, not the effect.
    auto const input(tone(220, oneSecond / 2));
    auto const dry(dryRender(input));

    for (auto const name : amplifyingEffects)
    {
        UNSCOPED_INFO(name);
        auto const bypassed(
            renderOne(name, input, [](Module &module) { module.setBaseParameter(bypass, 1); }));
        REQUIRE(bypassed.size() == dry.size());
        CHECK(bypassed == dry);
    }
}

TEST_CASE("Every amplifying effect renders finite, bounded audio", "[effects][property]")
{
    /// \note The golden suite has this for all 57, and skips in a checked build
    /// -- so on the configuration that runs the ~1200 asserts, nothing checked
    /// it. These nine are the ones whose decisions can land on a denormal or a
    /// divide, so these nine are the ones worth having it for in both.
    auto const input(tone(220, oneSecond / 2));

    for (auto const name : amplifyingEffects)
    {
        UNSCOPED_INFO(name);
        auto const output(renderOne(name, input));
        CHECK(allFinite(output));
        CHECK(peak(output) < 100.0f);
    }
}

TEST_CASE("Every amplifying effect renders the same twice", "[effects][property]")
{
    // A decision that reads uninitialised state, or state carried between
    // renders, shows up here and nowhere else in the suite -- the goldens render
    // each fixture once.
    auto const input(tone(220, oneSecond / 4));

    for (auto const name : amplifyingEffects)
    {
        UNSCOPED_INFO(name);
        CHECK(renderOne(name, input) == renderOne(name, input));
    }
}

////////////////////////////////////////////////////////////////////////////////
// Pitch Spring: an oscillation, and a direction
////////////////////////////////////////////////////////////////////////////////

namespace
{
/// Pitch Spring's effect parameters, in declaration order.
enum SpringParameter : std::uint8_t
{
    springType = 0,
    springDepth = 1,
    springPeriod = 2
};

/// CommonParameters::SpringType's enumerators.
enum SpringDirection : int
{
    symmetric = 0,
    up = 1,
    down = 2
};

////////////////////////////////////////////////////////////////////////////////
///
/// \brief The dominant frequency in each of \p count windows spread over the
/// render's second half -- the first half is where the effect's own state
/// settles.
///
/// \note **Offset by the engine's latency**, so that the windows land on the same
/// part of the *signal* rather than the same part of the buffer.
///
///   Without it these windows are a sampling grid laid over a modulated pitch,
/// and where that grid falls relative to the modulation decides what the extremes
/// read. `standardSetup` is 1024/4, so when #83 moved the engine's delay from
/// `fftSize - stepSize` to `fftSize` the whole render slid 256 samples under a
/// fixed grid and "A pitch spring oscillates" changed its answer -- on a render
/// that is bit-identical once the shift is taken out. Anchoring to the signal is
/// what stops a latency change reading as a pitch change.
///
/// \note The grid also has to be dense enough for the modulation it is laid
/// over. Four windows per cycle reads the extremes 140 cents short of the truth
/// and moves 48 of them on grid placement alone; sixteen holds 4. What a caller
/// owes this function is a \p count that gives it a dozen or more windows per
/// cycle of whatever it is measuring.
///
////////////////////////////////////////////////////////////////////////////////

std::vector<double> pitchOverTime(std::span<float const> render, unsigned const count,
                                  SWTest::RenderSetup const &setup = standardSetup)
{
    auto const frames(static_cast<std::uint32_t>(render.size() / channels));
    auto const latency(setup.fftSize);
    auto const span((frames - latency) / 2);
    auto const width(span / count);
    std::vector<double> pitches;
    for (unsigned index(0); index < count; ++index)
        pitches.push_back(dominantFrequency(window(render, latency + span + index * width, width)));
    return pitches;
}
} // anonymous namespace

TEST_CASE("A pitch spring at zero depth does not move the pitch", "[effects][property][pitch]")
{
    constexpr double input{220};
    auto const rendered(renderOne("Pitch Spring", tone(input, oneSecond), [](Module &module) {
        module.setEffectParameter(springDepth, 0);
    }));

    for (auto const pitch : pitchOverTime(rendered, 4))
    {
        UNSCOPED_INFO(pitch << " Hz");
        CHECK(std::abs(cents(input, pitch)) < 15);
    }
}

TEST_CASE("A pitch spring oscillates, and only where it is told to", "[effects][property][pitch]")
{
    constexpr double input{220};
    constexpr double depthInCents{600};

    /// \note 500 ms against 32 windows, so the grid gets ~16 windows per cycle
    /// of the modulation. At the 250 ms and 16 windows this case used to run,
    /// four windows a cycle read the extremes up to 140 cents short and moved 48
    /// of them on where the grid happened to fall. \see issue #246.
    auto const spring([&](SpringDirection const direction, std::string_view const name) {
        return pitchOverTime(renderOne(name, tone(input, 2 * oneSecond),
                                       [direction](Module &module) {
                                           module.setEffectParameter(springType, direction);
                                           module.setEffectParameter(springDepth, depthInCents);
                                           module.setEffectParameter(springPeriod, 500);
                                       }),
                             32);
    });

    auto const extremes([](std::vector<double> const &pitches) {
        auto const [lowest, highest](std::ranges::minmax_element(pitches));
        return std::pair{*lowest, *highest};
    });

    auto const [upLow, upHigh](extremes(spring(up, "Pitch Spring")));
    auto const [downLow, downHigh](extremes(spring(down, "Pitch Spring")));
    UNSCOPED_INFO("up " << upLow << ".." << upHigh << " Hz, down " << downLow << ".." << downHigh
                        << " Hz");

    // It oscillates: the range it covers is a real fraction of the depth it was
    // given, rather than one value repeated.
    CHECK(cents(upLow, upHigh) > (depthInCents / 4));
    CHECK(cents(downLow, downHigh) > (depthInCents / 4));

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \note One-sided, and a claim about pitch as a 30 ms window measures it
    /// rather than about every cycle of the waveform. Up is "the input pitch and
    /// above", which is what the modulator asks for and what a listener hears --
    /// but the render does dip below it, briefly, each time the scale sweeps.
    /// The last case in this section is where that is recorded.
    ///
    ////////////////////////////////////////////////////////////////////////////
    CHECK(cents(input, upLow) > -100);
    CHECK(cents(input, downHigh) < 100);

    // And neither exceeds the depth it was asked for. The slack is the
    // measurement's own error, which this grid holds under 5 cents.
    CHECK(cents(input, upHigh) < (depthInCents + 50));
    CHECK(cents(downLow, input) < (depthInCents + 50));
}

TEST_CASE("A PV pitch spring oscillates, and holds nothing else", "[effects][property][pitch]")
{
    ////////////////////////////////////////////////////////////////////////////
    ///
    ///   The same oscillator driving `PVPitchShifter` instead of the plain one,
    /// and the pitch property the case above asserts does not survive the
    /// change. On the same grid the plain spelling reads every bound within 2
    /// cents; this one puts its "up" extreme anywhere between 445 and 993 cents
    /// depending only on where the windows fall, and its "up" floor 58 cents
    /// under the input.
    ///
    /// \note Which is what #246 predicts of it rather than a surprise: the PV
    /// variants hold no phase state at all -- `PVPitchShifter::process()` is
    /// `pitchShiftAndScale()` and nothing else -- so a partial replicated across
    /// output bins has no accumulated phase to be coherent with, and a moving
    /// scale re-replicates it every frame.
    ///
    /// \note So only the claim that survives is made. Recording that the others
    /// do not is the point of the case: a fix for #246 has to bring this
    /// spelling up to the one above, and nothing else here would notice.
    ///
    ////////////////////////////////////////////////////////////////////////////
    constexpr double input{220};
    constexpr double depthInCents{600};

    auto const spring([&](SpringDirection const direction) {
        return pitchOverTime(renderOne("Pitch Spring (pvd)", tone(input, 2 * oneSecond),
                                       [direction](Module &module) {
                                           module.setEffectParameter(springType, direction);
                                           module.setEffectParameter(springDepth, depthInCents);
                                           module.setEffectParameter(springPeriod, 500);
                                       }),
                             32);
    });

    for (auto const direction : {up, down})
    {
        UNSCOPED_INFO((direction == up ? "up" : "down"));
        auto const pitches(spring(direction));
        auto const [lowest, highest](std::ranges::minmax_element(pitches));
        UNSCOPED_INFO(*lowest << ".." << *highest << " Hz");
        CHECK(cents(*lowest, *highest) > (depthInCents / 4));
    }
}

////////////////////////////////////////////////////////////////////////////////
// A moving pitch scale, which is #246 seen from the time domain
////////////////////////////////////////////////////////////////////////////////

namespace
{
/// Pitch Shifter's effect parameters, in declaration order.
enum ShifterParameter : std::uint8_t
{
    shifterSemitones = 0,
    shifterCents = 1
};

/// \brief How many of a render's cycles have a period that departs from its
/// neighbours' by more than \p tolerance cents.
///
/// \note Successive positive-going zero crossings, linearly interpolated. No
/// window and no transform, so nothing here is a claim about a detector: a
/// sweep moves the trend and leaves this at zero, and only a cycle out of step
/// with the ones around it counts. The dry chain and a shift the effect can
/// carry both score zero.
unsigned disturbedCycles(std::span<float const> render, std::uint32_t const first,
                         std::uint32_t const count, double const tolerance = 40,
                         SWTest::RenderSetup const &setup = standardSetup)
{
    auto const mono(window(render, setup.fftSize + first, count));
    std::vector<double> periods;
    double previous{-1};
    for (std::size_t n(1); n < mono.size(); ++n)
        if ((mono[n - 1] <= 0) && (mono[n] > 0))
        {
            auto const crossing(static_cast<double>(n - 1) +
                                (mono[n - 1] == mono[n]
                                     ? 0.0
                                     : -mono[n - 1] / static_cast<double>(mono[n] - mono[n - 1])));
            if (previous >= 0)
                periods.push_back(crossing - previous);
            previous = crossing;
        }

    constexpr std::size_t reach{5};
    unsigned disturbed{0};
    for (std::size_t index(reach); (index + reach) < periods.size(); ++index)
    {
        std::vector<double> neighbours;
        for (std::size_t other(index - reach); other <= (index + reach); ++other)
            if (other != index)
                neighbours.push_back(periods[other]);
        std::ranges::sort(neighbours);
        if (std::abs(cents(neighbours[neighbours.size() / 2], periods[index])) > tolerance)
            ++disturbed;
    }
    return disturbed;
}
} // anonymous namespace

TEST_CASE("A pitch scale the effect can hold still, it cannot sweep", "[effects][property][pitch]")
{
    ////////////////////////////////////////////////////////////////////////////
    ///
    ///   The before-picture for the moving half of #246, and the reason the case
    /// above measures pitch on 30 ms windows rather than cycle by cycle.
    ///
    ///   A tritone is scale 1.414. Held there, the shifter is clean -- the
    /// images one hop rate either side sit at a few percent of the partial and
    /// no cycle is out of step with its neighbours. Swept across the same range
    /// by a spring, the same shifter puts cycles a semitone and more off the
    /// sweep it is on, at full level. #246's `overlapFactor >= scale` reaches
    /// the first and says nothing about the second.
    ///
    /// \note So a fix has to be measured here as well as on a static spectrum.
    /// Identity phase locking that clears the images of a two-octave shift can
    /// leave this untouched, and every other case in this file would stay green.
    ///
    ////////////////////////////////////////////////////////////////////////////
    constexpr double input{220};
    constexpr std::uint32_t analysed{oneSecond / 2};

    // the sweep's own top, held: six semitones, the same 1.414 the spring reaches
    auto const held(renderOne("Pitch Shifter", tone(input, 2 * oneSecond), [](Module &module) {
        module.setEffectParameter(shifterSemitones, 6);
        module.setEffectParameter(shifterCents, 0);
    }));
    auto const heldDisturbance(disturbedCycles(held, oneSecond, analysed));
    UNSCOPED_INFO("held at +6 st: " << heldDisturbance << " disturbed cycles");
    CHECK(heldDisturbance == 0);

    // and the empty chain, so the counter is known to have a floor
    CHECK(disturbedCycles(dryRender(tone(input, 2 * oneSecond)), oneSecond, analysed) == 0);

    // and the same range swept, in both spellings
    auto const swept([&](std::string_view const name) {
        return disturbedCycles(renderOne(name, tone(input, 2 * oneSecond),
                                         [](Module &module) {
                                             module.setEffectParameter(springType, up);
                                             module.setEffectParameter(springDepth, 600);
                                             module.setEffectParameter(springPeriod, 250);
                                         }),
                               oneSecond, analysed);
    });

    auto const plain(swept("Pitch Spring"));
    auto const pv(swept("Pitch Spring (pvd)"));
    UNSCOPED_INFO("swept 0..+600 ct: " << plain << " disturbed cycles plain, " << pv << " PV");
    CHECK(plain > 10);

    // the PV spelling, which carries no phase state at all, is the worse of the
    // two -- so a fix that only reaches the phase half would show up here
    CHECK(pv > plain);
}

////////////////////////////////////////////////////////////////////////////////
// Pitch Magnet: it lands on the target, from either side
////////////////////////////////////////////////////////////////////////////////

namespace
{
enum MagnetParameter : std::uint8_t
{
    magnetTarget = 0,
    magnetSpeed = 1
};
} // anonymous namespace

TEST_CASE("A pitch magnet at zero strength does not move the pitch", "[effects][property][pitch]")
{
    constexpr double input{220};
    auto const rendered(renderOne(
        "Pitch Magnet", tone(input, oneSecond),
        [](Module &module) {
            module.setEffectParameter(magnetTarget, 880);
            module.setEffectParameter(magnetSpeed, 0);
        },
        pitchSetup));

    auto const settled(dominantFrequency(window(rendered, oneSecond / 2, oneSecond / 4)));
    UNSCOPED_INFO(settled << " Hz");
    CHECK(std::abs(cents(input, settled)) < 15);
}

namespace
{
/// A settled half-second of the magnet pulling \p input to \p target, one
/// channel. Two seconds rendered: half of it is the ramp, the rest is the look.
std::vector<float> magnetised(double const input, double const target)
{
    auto const rendered(renderOne(
        "Pitch Magnet", tone(input, 2 * oneSecond),
        [target](Module &module) {
            module.setEffectParameter(magnetTarget, static_cast<float>(target));
            // 60 semitones a second: two octaves inside half a second, so a two
            // second render is settling time and then a long look.
            module.setEffectParameter(magnetSpeed, 60);
        },
        pitchSetup));
    return window(rendered, oneSecond, oneSecond / 2);
}

/// One hop rate, the spacing of a phase vocoder's alias images. \see the
/// two-octave case below for why this is the number that decides them.
double hopRate(SWTest::RenderSetup const &setup)
{
    return static_cast<double>(setup.sampleRate) / (setup.fftSize / setup.overlapFactor);
}
} // anonymous namespace

TEST_CASE("A pitch magnet arrives at its target and stays there", "[effects][property][pitch]")
{
    // The property the effect is named for, and the one a golden cannot state:
    // whatever the pitch detector picks and however the bins land, the output
    // pitch ends up at the target frequency. A fifth up and two octaves down,
    // because the clamp that limits the movement is two-sided.
    //
    // Not two octaves *up*: that one does not arrive, and the case below is what
    // says so. A shift of scale s spreads each input bin over s output bins,
    // the resynthesis carries +-overlapFactor/2 bins, and 330 and 110 are the
    // two targets whose scale -- 1.5 and 0.5 -- fits inside the default 4.
    constexpr double input{220};

    for (double const target : {330.0, 110.0})
    {
        auto const arrived(dominantFrequency(magnetised(input, target)));
        UNSCOPED_INFO("target " << target << " Hz, arrived at " << arrived << " Hz, "
                                << cents(target, arrived) << " cents off");
        /// \note Twenty cents, which is a hundredth of the smaller distance
        /// being travelled. It could be two -- they measure at 0.5 and 1.8 --
        /// but the bound worth writing down is the one that says "this is the
        /// note it was asked for" rather than one that pins today's arithmetic.
        CHECK(std::abs(cents(target, arrived)) < 20);
    }
}

////////////////////////////////////////////////////////////////////////////////
///
/// \brief A two-octave magnet puts out three tones, not one.
///
///   Recorded rather than asserted away, because it is what the effect does
/// today and the fix is a phase-locked rewrite that has not happened. \see #246.
///
///   The mechanism decides the frequencies exactly, which is why this case
/// derives them instead of pasting them. `pitchShiftAndScale()` back-maps every
/// output bin to `round(k / scale)`, so an input bin is replicated across
/// `scale` output bins, all carrying the same commanded frequency. Resynthesis
/// can only realise a deviation of +-`overlapFactor`/2 bins from a bin's centre,
/// so the replicas past that fold, and reappear one hop rate --
/// `sampleRate / hop` -- above and below the target. Alias-free would need
/// `overlapFactor >= scale`; the default overlap is 4 and two octaves is scale
/// 4, exactly marginal.
///
/// \note What is *not* asserted is which of the three is loudest. They are
/// within a factor of two of each other here, and the ordering moves with the
/// FFT size and, on the evidence of the other eight amplifying effects, with the
/// FFT backend. \see Tolerances::amplified() and the note at the top of this
/// file. The stable facts are that all three are present and that the images are
/// not a rounding error, and those are the two this case states.
///
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("A two-octave pitch magnet lands on its target and on two aliases",
          "[effects][property][pitch]")
{
    constexpr double input{220}, target{880};

    auto const settled(magnetised(input, target));
    auto const fold(hopRate(pitchSetup));

    auto const atTarget(magnitudeAt(settled, target));
    auto const below(magnitudeAt(settled, target - fold));
    auto const above(magnitudeAt(settled, target + fold));

    UNSCOPED_INFO("fold " << fold << " Hz | " << (target - fold) << " Hz " << below << ", "
                          << target << " Hz " << atTarget << ", " << (target + fold) << " Hz "
                          << above);

    /// \note A hundredth of the input's amplitude. The three measure around 0.05
    /// to 0.07 and the spectrum away from them is under 0.001, so this separates
    /// "there is a partial here" from "there is nothing here" with two orders of
    /// margin either way, and does not pin a level.
    constexpr double present{0.005};
    CHECK(atTarget > present);
    CHECK(below > present);
    CHECK(above > present);

    // The images are the defect: a quarter of the target is the bound that says
    // "audible artefact" rather than "numerical residue". Both measure above 0.7.
    CHECK(std::max(below, above) > (atTarget / 4));

    // And whichever of the three wins, it is one of the three -- the claim the
    // effect can actually support at this setting.
    auto const loudest(dominantFrequency(settled));
    auto const offBy(
        std::min({std::abs(cents(target, loudest)), std::abs(cents(target - fold, loudest)),
                  std::abs(cents(target + fold, loudest))}));
    UNSCOPED_INFO("loudest " << loudest << " Hz, " << offBy
                             << " cents from the nearest of the three");
    CHECK(offBy < 20);
}

////////////////////////////////////////////////////////////////////////////////
// Octaver: an octave, where there was not one
////////////////////////////////////////////////////////////////////////////////

namespace
{
enum OctaverParameter : std::uint8_t
{
    octave1 = 0,
    gainOctave1 = 1,
    octave2 = 2,
    gainOctave2 = 3,
    cutoffFrequency = 4
};

/// Octaver's Octave1/Octave2 enumerators. `Off` is 2, and the implementation
/// takes `value - 2` as the number of octaves -- which is why this reads as an
/// offset rather than as a list.
enum Octave : int
{
    twoDown = 0,
    oneDown = 1,
    off = 2,
    oneUp = 3,
    twoUp = 4
};

/// \note The cutoff is a low pass over the *output*, so an Octaver whose cutoff
/// sits below the octave it just added removes it again. It defaulted to 350 Hz
/// until 19.08.2026 and every case below had to open it to measure the effect
/// rather than the filter; they still set it, explicitly, because a property
/// about the octave should not silently become a property about the default.
constexpr float openCutoff{16000};
} // anonymous namespace

TEST_CASE("An octaver with both octaves off passes the signal through",
          "[effects][property][octave]")
{
    auto const input(tone(220, oneSecond / 2));
    auto const dry(dryRender(input));
    auto const rendered(renderOne("Octaver", input, [](Module &module) {
        module.setEffectParameter(octave1, off);
        module.setEffectParameter(octave2, off);
        module.setEffectParameter(cutoffFrequency, openCutoff);
    }));

    // Not bit-exact: the signal still goes out through the ReIm domain and back,
    // which is arithmetic the empty chain does not do. Audibly the same, though.
    CHECK(relativeDifference(rendered, dry) < 0.02f);
}

TEST_CASE("An octaver puts energy an octave away", "[effects][property][octave]")
{
    constexpr double input{220};
    auto const signal(tone(input, oneSecond));

    auto const octaver([&](Octave const first, float const octaveGain) {
        return renderOne("Octaver", signal, [first, octaveGain](Module &module) {
            module.setEffectParameter(octave1, first);
            module.setEffectParameter(gainOctave1, octaveGain);
            module.setEffectParameter(octave2, off);
            module.setEffectParameter(cutoffFrequency, openCutoff);
        });
    });

    auto const late([&](std::vector<float> const &rendered) {
        return window(rendered, oneSecond / 2, oneSecond / 4);
    });

    auto const dry(late(dryRender(signal)));
    auto const upOne(late(octaver(oneUp, 0)));
    auto const downOne(late(octaver(oneDown, 0)));

    // The input has essentially nothing an octave either side of itself; the
    // effect's whole job is to put something there.
    CHECK(magnitudeAt(upOne, 2 * input) > (10 * magnitudeAt(dry, 2 * input)));
    CHECK(magnitudeAt(downOne, input / 2) > (10 * magnitudeAt(dry, input / 2)));

    // And the original is still there underneath -- an octaver adds, it does not
    // replace. That is the difference between it and a pitch shifter.
    CHECK(magnitudeAt(upOne, input) > (0.5 * magnitudeAt(dry, input)));
}

TEST_CASE("An octaver's gain decides how much octave there is", "[effects][property][octave]")
{
    // Monotone in a parameter, which is the shape of property a golden cannot
    // express at all: it pins one setting and says nothing about the map.
    constexpr double input{220};
    auto const signal(tone(input, oneSecond));

    double previous{0};
    for (float const octaveGain : {-24.0f, -12.0f, 0.0f, 12.0f})
    {
        auto const rendered(renderOne("Octaver", signal, [octaveGain](Module &module) {
            module.setEffectParameter(octave1, oneUp);
            module.setEffectParameter(gainOctave1, octaveGain);
            module.setEffectParameter(octave2, off);
            module.setEffectParameter(cutoffFrequency, openCutoff);
        }));
        auto const present(magnitudeAt(window(rendered, oneSecond / 2, oneSecond / 4), 2 * input));
        UNSCOPED_INFO(octaveGain << " dB -> " << present);
        CHECK(present > previous);
        previous = present;
    }
}

TEST_CASE("An octaver's cutoff is a low pass on what comes out", "[effects][property][octave]")
{
    /// \note Worth its own case because the parameter is called "Lowpass" in the
    /// editor, "Low pass" in a preset and `CutoffFrequency` in the source, and
    /// because it is the one control here that can silently remove everything the
    /// effect produces. It defaulted low enough to do exactly that until
    /// 19.08.2026 -- issue #15 -- so this pins the direction rather than a value.
    constexpr double input{220};
    auto const signal(tone(input, oneSecond));

    auto const withCutoff([&](float const cutoff) {
        auto const rendered(renderOne("Octaver", signal, [cutoff](Module &module) {
            module.setEffectParameter(octave1, oneUp);
            module.setEffectParameter(octave2, off);
            module.setEffectParameter(cutoffFrequency, cutoff);
        }));
        return magnitudeAt(window(rendered, oneSecond / 2, oneSecond / 4), 2 * input);
    });

    // 300 Hz is below the added octave at 440 and below the input at 220.
    CHECK(withCutoff(300) < (0.1 * withCutoff(openCutoff)));
}

TEST_CASE("An octaver renders where its cutoff is above Nyquist",
          "[effects][property][octave][issue-15]")
{
    /// \note The one case here that renders at *default* parameters, and it is
    /// here for the checked build specifically. `Low pass` reaches 16 kHz and
    /// since 19.08.2026 rests there, while Nyquist at 22.05 kHz is 11 kHz -- so
    /// an Octaver nobody has touched now asks for a cutoff above the spectrum it
    /// is filtering. `Setup::frequencyInHzToBin()` asserted that this could not
    /// happen; every caller already clamped the bin into its own working range,
    /// so a release build was unaffected and a checked one aborted on a module
    /// doing nothing wrong. Reproduced by reverting the clamp before it was
    /// written: this case aborts, and the golden suite -- which skips in a
    /// checked build -- could not have caught it.
    ///
    /// \note 22.05 kHz rather than a contrived rate: it is an ordinary host
    /// setting, and the other eight callers of frequencyInHzToBin() reach the
    /// same edge from their own defaults at a low enough rate.
    constexpr SWTest::RenderSetup lowRate{1024, 4, channels, 22050, 256};

    /// The tone is generated at the file's 44.1 kHz and played out at half that,
    /// so it sounds an octave high. Irrelevant here -- what is being asked is
    /// whether the render happens at all, and whether the cutoff at the top of
    /// the range still passes the signal rather than clamping to nothing.
    auto const input(tone(220, oneSecond / 4));
    auto const rendered(renderOne("Octaver", input, {}, lowRate));

    REQUIRE_FALSE(rendered.empty());
    CHECK(allFinite(rendered));
    CHECK(rms(rendered) > 0);
}

////////////////////////////////////////////////////////////////////////////////
// To PV and From PV: inverses
//
// \note Titled "PVD start" and "PVD stop" until 17.08.2026, which is still what
// they are named by below -- a streaming name outlives the title it was taken
// from, and every preset written since 2011 says the old one.
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("To PV followed by From PV is transparent", "[effects][property][pvd]")
{
    // The one property that says what these two are *for*. Analysis converts
    // each bin's phase into an instantaneous frequency and synthesis converts it
    // back; with nothing in between, the pair has to return what it was given.
    // Everything in the Phase Vocoder group is only meaningful inside that
    // sandwich.
    auto const input(tone(220, oneSecond));
    auto const dry(dryRender(input));

    Slot const sandwich[]{{effectByStreamingName("PVD start"), {}},
                          {effectByStreamingName("PVD stop"), {}}};
    auto const rendered(SWTest::renderChain(standardSetup, sandwich, input));

    REQUIRE(rendered.size() == dry.size());

    /// \note Compared over the second half. The first is where the phase
    /// vocoder's own accumulator is still catching up with a signal that started
    /// abruptly, and a round trip is not claimed to be transparent through that.
    auto const window([](std::vector<float> const &render) {
        return ::window(render, oneSecond / 2, oneSecond / 2);
    });
    CHECK(relativeDifference(window(rendered), window(dry)) < 0.05f);
}

TEST_CASE("To PV alone is not transparent", "[effects][property][pvd]")
{
    /// \note The control for the case above, and not a formality: if the pair
    /// were transparent because *neither one did anything*, the round trip would
    /// pass and mean nothing. A phase treated as a frequency is a different
    /// signal, and this is that difference.
    auto const input(tone(220, oneSecond));
    auto const dry(dryRender(input));
    auto const analysed(renderOne("PVD start", input));

    CHECK(relativeDifference(window(analysed, oneSecond / 2, oneSecond / 2),
                             window(dry, oneSecond / 2, oneSecond / 2)) > 0.2f);
}

////////////////////////////////////////////////////////////////////////////////
// Imploder and Exploder: a magnitude that decays, and one that grows
////////////////////////////////////////////////////////////////////////////////

namespace
{
/// PVImploder's parameters (Decay, Gliss, Threshold, Gate) and PVExploder's
/// (Growth, Gliss, Threshold, Gate) share their layout, which is what
/// Detail::ExImPloder exists for.
enum ExImPloderParameter : std::uint8_t
{
    magnitudeScale = 0, ///< Decay for the Imploder, Growth for the Exploder
    glissando = 1,
    limit = 2,
    gate = 3
};
} // anonymous namespace

TEST_CASE("An imploder sustains a note after it stops", "[effects][property][eximploder]")
{
    // What "spectral implosion" amounts to: a bin's magnitude is held and let
    // down slowly rather than following the input. So a note that stops dead
    // leaves a tail, and a longer decay leaves a longer one.
    constexpr double input{440};
    auto const signal(gatedTone(input, 2 * oneSecond, 0, oneSecond / 2));
    auto const dry(dryRender(signal));

    auto const imploder([&](float const decaySeconds) {
        return renderOne("Imploder", signal, [decaySeconds](Module &module) {
            module.setEffectParameter(magnitudeScale, decaySeconds);
            module.setEffectParameter(glissando, 0); // no pitch drift to confuse the measurement
            module.setEffectParameter(limit, -120);
            module.setEffectParameter(gate, -120);
        });
    });

    /// Well after the note stopped and well after the engine's latency, so
    /// anything here is the effect's doing.
    auto const tail([](std::vector<float> const &rendered) {
        return rms(window(rendered, oneSecond, oneSecond / 2));
    });

    auto const dryTail(tail(dry));
    auto const shortDecay(tail(imploder(1)));
    auto const longDecay(tail(imploder(200)));

    UNSCOPED_INFO("dry " << dryTail << ", 1 s decay " << shortDecay << ", 200 s decay "
                         << longDecay);

    // There is a tail at all, and it is the effect's rather than the window's.
    CHECK(longDecay > (10 * dryTail));
    // And it is monotone in the decay time, which is what the parameter means.
    CHECK(longDecay > shortDecay);
}

TEST_CASE("An imploder never makes a frequency louder than it has been",
          "[effects][property][eximploder]")
{
    /// \note The energy bound stage 4.4 asks for, and it is an invariant of the
    /// algorithm rather than a measured limit: the accumulator takes the current
    /// amplitude only when that is the *larger*, and otherwise multiplies by a
    /// decay below 1. So no bin can exceed the loudest it has been -- which is
    /// what separates this effect from the Exploder, whose scale is deliberately
    /// above 1.
    ///
    /// \note Stated per frequency and not as a sample peak, because the sample
    /// peak is not bounded and should not be expected to be: four overlapping
    /// windows all holding the same magnitude add up in phase where the input's
    /// did not, and this render measures **1.6x** the dry peak while every bin
    /// in it obeys the rule. An invariant of the spectrum, asserted in the
    /// spectrum.
    constexpr double input{440};
    auto const signal(gatedTone(input, oneSecond, 0, oneSecond / 2));
    auto const dry(dryRender(signal));
    auto const rendered(renderOne("Imploder", signal, [](Module &module) {
        module.setEffectParameter(magnitudeScale, 200);
        module.setEffectParameter(glissando, 0);
        module.setEffectParameter(limit, -120);
        module.setEffectParameter(gate, -120);
    }));

    auto const at([&](std::vector<float> const &render, std::uint32_t const first) {
        return magnitudeAt(window(render, first, oneSecond / 8), input);
    });

    auto const dryWhileSounding(at(dry, oneSecond / 8));
    auto const whileSounding(at(rendered, oneSecond / 8));
    auto const afterwards(at(rendered, 3 * oneSecond / 4));

    UNSCOPED_INFO("dry " << dryWhileSounding << ", sounding " << whileSounding << ", tail "
                         << afterwards);

    // Never louder than the input was at that frequency, while the note plays...
    CHECK(whileSounding <= (1.05 * dryWhileSounding));
    // ...nor afterwards, when the accumulator is all there is left.
    CHECK(afterwards <= whileSounding);
    // And it really is a decay rather than a mute: the tail is still there.
    CHECK(afterwards > (0.1 * whileSounding));
}

TEST_CASE("An exploder grows a steady note", "[effects][property][eximploder]")
{
    // The mirror image of the Imploder: the magnitude scale is deliberately
    // above 1, so a constant input comes out rising.
    //
    /// \note A *quiet* input, and the growth measured inside the first second.
    /// "Limit" is not a ceiling the level approaches -- reaching it resets the
    /// accumulator to whatever the input is doing, and the effect starts again
    /// from there. So the honest property is about the growth phase, and a test
    /// that measured "later is louder than earlier" across a whole render would
    /// be sampling a sawtooth at two arbitrary points. Measured: at Growth 1 s
    /// and Limit -20 dB this render is a ~7x climb over 1 s and then a reset.
    constexpr double input{440};
    auto const signal(tone(input, 2 * oneSecond, 0.05f));

    auto const exploder([&](float const growthSeconds) {
        return renderOne("Exploder", signal, [growthSeconds](Module &module) {
            module.setEffectParameter(magnitudeScale, growthSeconds);
            module.setEffectParameter(glissando, 0);
            module.setEffectParameter(limit, -20);
            module.setEffectParameter(gate, -120);
        });
    });

    /// Quarter-second windows across the first second: the growth phase.
    auto const climb([](std::vector<float> const &rendered) {
        std::vector<float> levels;
        for (std::uint32_t quarter(0); quarter < 4; ++quarter)
            levels.push_back(rms(window(rendered, quarter * oneSecond / 4, oneSecond / 4)));
        return levels;
    });

    auto const dry(climb(dryRender(signal)));
    auto const fast(climb(exploder(1)));
    auto const slow(climb(exploder(200)));

    UNSCOPED_INFO("dry " << dry.front() << ".." << dry.back() << ", fast " << fast.front() << ".."
                         << fast.back() << ", slow " << slow.front() << ".." << slow.back());

    // It grows, monotonically, over an input that does not. Measured at 6.7x
    // over its own start and 17x over the dry render; the bounds are half that,
    // so they say "it climbed" rather than "it climbed this far".
    CHECK(std::ranges::is_sorted(fast));
    CHECK(fast.back() > (3 * fast.front()));
    CHECK(fast.back() > (8 * dry.back()));

    // Faster growth gets further, which is what the parameter means. At 200
    // seconds to climb 120 dB it has barely left the input behind.
    CHECK(fast.back() > slow.back());
    CHECK(slow.back() < (2 * dry.back()));

    // And it does not run away: the reset is what stops it.
    auto const rendered(exploder(1));
    CHECK(allFinite(rendered));
    CHECK(peak(rendered) < 100.0f);
}

////////////////////////////////////////////////////////////////////////////////
// Slew Limiter: a change, slowed
////////////////////////////////////////////////////////////////////////////////

namespace
{
enum SlewParameter : std::uint8_t
{
    slewDirection = 0,
    slewRate = 1
};

/// SlewLimiter::Direction's enumerators.
enum SlewDirection : int
{
    riseAndFall = 0,
    riseOnly = 1,
    fallOnly = 2
};
} // anonymous namespace

TEST_CASE("A slew limiter at its maximum rate limits nothing", "[effects][property][slew]")
{
    // 300 dB/s over a 172 Hz frame rate is 1.7 dB a frame, which no part of a
    // steady tone asks for. The limiter is then a multiply by one.
    auto const input(tone(440, oneSecond));
    auto const dry(dryRender(input));
    auto const rendered(renderOne("Slew Limiter", input, [](Module &module) {
        module.setEffectParameter(slewDirection, riseAndFall);
        module.setEffectParameter(slewRate, 300);
    }));

    CHECK(relativeDifference(window(rendered, oneSecond / 2, oneSecond / 2),
                             window(dry, oneSecond / 2, oneSecond / 2)) < 0.05f);
}

TEST_CASE("A slew limiter slows an attack down", "[effects][property][slew]")
{
    // Rise-limited, so a note that starts abruptly may not get loud as fast as
    // it was told to -- and the higher the rate, the sooner it arrives.
    //
    /// \note The rise starts from `FLT_EPSILON` and not from the input's level:
    /// the implementation floors the previous amplitude there so that a bin can
    /// ever leave silence at all. That is 138 dB below unity, so the parameter's
    /// low end is far slower than it reads -- 3 dB/s needs **46 seconds** to
    /// open, and a three second render at that setting is indistinguishable from
    /// a mute. Measured, and the reason the rates compared here start at 60.
    constexpr std::uint32_t onset{oneSecond / 2};
    auto const signal(gatedTone(440, 3 * oneSecond, onset, 3 * oneSecond));
    auto const dry(dryRender(signal));

    auto const attack([&](float const rate, std::uint32_t const at) {
        auto const rendered(renderOne("Slew Limiter", signal, [rate](Module &module) {
            module.setEffectParameter(slewDirection, riseOnly);
            module.setEffectParameter(slewRate, rate);
        }));
        return rms(window(rendered, at, oneSecond / 4));
    });

    // Half a second after the note starts: still climbing, at every rate, so the
    // three are ordered by how fast they climb.
    constexpr std::uint32_t halfWayIn{onset + oneSecond / 2};
    auto const dryLevel(rms(window(dry, halfWayIn, oneSecond / 4)));
    auto const slowest(attack(60, halfWayIn));
    auto const middling(attack(150, halfWayIn));
    auto const quickest(attack(300, halfWayIn));

    UNSCOPED_INFO("dry " << dryLevel << ", 60 dB/s " << slowest << ", 150 " << middling << ", 300 "
                         << quickest);

    CHECK(slowest < middling);
    CHECK(middling < quickest);
    CHECK(quickest <= dryLevel);

    // Slowed and not removed: at the top of the range it is all the way up
    // within half a second, and at the bottom it is still nowhere near.
    CHECK(quickest > (0.9f * dryLevel));
    CHECK(slowest < (0.1f * dryLevel));
}

TEST_CASE("A slew limiter holds a release up", "[effects][property][slew]")
{
    // The other direction, and the reason Direction exists: fall-limited, the
    // note cannot get quiet as fast as it stopped, so there is a tail where the
    // dry signal has silence.
    constexpr std::uint32_t offset{oneSecond};
    auto const signal(gatedTone(440, 2 * oneSecond, 0, offset));
    auto const dry(dryRender(signal));

    auto const release([&](float const rate) {
        auto const rendered(renderOne("Slew Limiter", signal, [rate](Module &module) {
            module.setEffectParameter(slewDirection, fallOnly);
            module.setEffectParameter(slewRate, rate);
        }));
        return rms(window(rendered, offset + oneSecond / 8, oneSecond / 4));
    });

    auto const dryTail(rms(window(dry, offset + oneSecond / 8, oneSecond / 4)));
    auto const slow(release(3));
    auto const quick(release(60));

    UNSCOPED_INFO("dry " << dryTail << ", 60 dB/s " << quick << ", 3 dB/s " << slow);

    CHECK(slow > dryTail);
    CHECK(slow > quick);
}
