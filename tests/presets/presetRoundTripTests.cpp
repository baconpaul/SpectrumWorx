////////////////////////////////////////////////////////////////////////////////
///
/// \file presetRoundTripTests.cpp
/// ------------------------------
///
///   Save a preset, load it back, and require that nothing moved.
///
///   The other half of stage 8's "done when": the corpus test proves the reader
/// still reads what the 2016 writer wrote, and this proves the writer and the
/// reader still agree with each other. Neither implies the other, and 8.1
/// replaces both at once.
///
///   Every effect in turn, with every parameter driven off its default first --
/// a round-trip of defaults is the one that passes even when the format writes
/// nothing at all. LFOs go on too: they are a nested element rather than an
/// attribute, and the saver deliberately omits any LFO value that is still at
/// its default, so "what was left out" is part of what has to round-trip.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "presets/presetHarness.hpp"

#include "core/modules/factory.hpp"

/// \note Not sorted with the block above: finalImplementations names
/// GUI::ModuleUI, which moduleDSPAndGUI is what defines.
#include "core/modules/moduleDSPAndGUI.hpp"
#include "core/modules/finalImplementations.hpp"

#include "le/parameters/lfoImpl.hpp"
#include "le/parameters/runtimeInformation.hpp"
#include "le/spectrumworx/effects/configuration/constants.hpp"
#include "le/spectrumworx/effects/configuration/effectNames.hpp"
#include "le/spectrumworx/engine/moduleParameters.hpp"
#include "le/spectrumworx/presets.hpp"

#include <juce_core/juce_core.h>

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
//------------------------------------------------------------------------------
namespace
{
//------------------------------------------------------------------------------

using namespace LE;
using namespace LE::SW;

namespace Effects = LE::SW::Effects;

using ModuleParameters = LE::SW::Engine::ModuleParameters;
using LFO = LE::Parameters::LFOImpl;

using SWTest::dump;
using SWTest::PresetConsumer;
using SWTest::ScopedProblemCounter;

/// \note Far larger than Preset::InMemoryPresetBuffer's 4096 bytes, and
/// deliberately: the 2016 sources record that five TuneWorx modules breach that
/// buffer, and driving every parameter off its default makes a document larger
/// still. What is under test here is the round-trip, not the shipping buffer
/// size -- saveTo() refuses to overrun one now, and returns 0 instead.
constexpr std::size_t generousBuffer{1u << 20};

/// \note A class rather than a function returning one: SWTest::Engine points
/// SpectrumWorxCore at a Program it holds by value, so it can be neither copied
/// nor moved without the pointer dangling.
class Fixture
{
  public:
    Fixture()
    {
        engine_.setNumberOfChannels(2, 2);
        engine_.setSampleRate(48000);
        engine_.setBlockSize(512);
        REQUIRE(engine_.initialise());
    }

    SWTest::Engine &engine() { return engine_; }

  private:
    SWTest::Engine engine_;
}; // class Fixture

//------------------------------------------------------------------------------
// Driving everything off its default
//------------------------------------------------------------------------------

/// \brief A value inside \p info's range that is not its default.
///
/// \note Discrete parameters step to the neighbour rather than to a fraction of
/// the range: a boolean has two values and 37% of the way along is not one of
/// them, and an enumerated parameter that quantises back to its default would
/// make this test pass without having tested anything.
float offDefault(Parameters::RuntimeInformation const &info)
{
    using Type = Parameters::RuntimeInformation::Type;

    switch (info.type)
    {
    case Type::Boolean:
    case Type::Enumerated:
    case Type::Integer:
    {
        auto const stepUp(info.default_ + 1);
        return (stepUp <= info.maximum) ? stepUp : (info.default_ - 1);
    }

    case Type::FloatingPoint:
    {
        /// \note Snapped to four decimals, which is all the format carries --
        /// the 2016 writer's floats are "1.0000" and "100.0000", and
        /// lexical_cast still writes exactly that. Phasevolution's Period was
        /// the one parameter whose 0.371-of-range landed on 1.85563 and came
        /// back 1.8556: the format's resolution, not a fault. A test value the
        /// format cannot hold would test the rounding rather than the trip.
        auto const snap([](float const value) { return std::round(value * 1.0e4f) / 1.0e4f; });

        auto const candidate(snap(info.minimum + 0.371f * (info.maximum - info.minimum)));
        return (candidate != info.default_)
                   ? candidate
                   : snap(info.minimum + 0.617f * (info.maximum - info.minimum));
    }

    /// \note Left alone. A trigger consumes its value when read, so "set it and
    /// see whether it survives a save" has no meaning for one.
    case Type::Trigger:
        break;
    }
    return info.default_;
}

/// \brief Sets an LFO parameter to \p fraction of the way along its own range.
///
/// \note Off the parameter's declared range rather than off a number chosen
/// here. Guessing produced two asserts in a row: the LFO's Phase is a fraction
/// of a period over +/- 0.5, and the global gains are linear multipliers over
/// 0.001 .. 2, so "-40 degrees" and "-3.5 dB" are both simply out of range. The
/// range is right there in the traits; ask it.
template <class Parameter> void setFraction(LFO &lfo, float const fraction)
{
    constexpr auto minimum(static_cast<float>(Parameter::unscaledMinimum) /
                           Parameter::rangeValuesDenominator);
    constexpr auto maximum(static_cast<float>(Parameter::unscaledMaximum) /
                           Parameter::rangeValuesDenominator);

    lfo.parameters().template get<Parameter>().setValue(
        static_cast<typename Parameter::param_type>(minimum + fraction * (maximum - minimum)));
}

/// \note SyncTypes is left alone on purpose: the saver writes it
/// unconditionally *because* leaving it out makes a preset read as
/// pre-synced-LFO, and PeriodScale is snapped against it on the way back in, so
/// changing either here would test the snapping rather than the round-trip.
///
/// \note `enabled` defaults to false, and the settings still round-trip: an LFO
/// is written as a nested element with the parameter's value as its text, and
/// the saver omits any LFO value still at its default -- so what is being
/// exercised is the element path and the omissions, not whether the LFO runs.
/// See the second test case for what enabling one does.
void driveLFO(LFO &lfo, unsigned int const seed, bool const enabled = false)
{
    lfo.parameters().get<LFO::Enabled>().setValue(enabled);
    setFraction<LFO::Phase>(lfo, 0.25f + 0.05f * static_cast<float>(seed % 5));
    setFraction<LFO::LowerBound>(lfo, 0.125f);
    setFraction<LFO::UpperBound>(lfo, 0.875f);
    setFraction<LFO::Waveform>(lfo, 0.5f);
}

void driveModule(ModuleParameters &module, unsigned int const seed)
{
    auto const baseParameters(ModuleParameters::numberOfBaseParameters);

    module.setBaseParameter(0, 1.0f); // Bypass

    for (std::uint8_t index(1); index < module.numberOfParameters(); ++index)
    {
        auto const value(offDefault(module.parameterInfo(index)));
        if (index < baseParameters)
            module.setBaseParameter(index, value);
        else
            module.setEffectParameter(static_cast<std::uint8_t>(index - baseParameters), value);
    }

    /// \note Every other LFO-able parameter, not all of them: a preset with an
    /// LFO on all forty of TuneWorx's parameters is a large document, and what
    /// is being tested is that an LFO survives beside a parameter that has none.
    for (std::uint8_t index(0); index < module.numberOfLFOControledParameters(); index += 2)
        driveLFO(module.lfo(index), seed + index);
}

/// \note Globals too. The FFT size and overlap factor reconfigure the engine
/// rather than only moving a number, so a preset that carries them is the case
/// where the loader has to go through setGlobalParameter -- and the one where
/// getting it wrong is silent.
/// \note The gains are linear multipliers over 0.001 .. 2.0 and the mix is
/// 0 .. 1, not decibels and not a percentage -- whatever their names say. The
/// committed presets agree: `In="1.0000" Out="1.0000" Mix="1.0000"` is unity.
void driveGlobals(SWTest::Engine &engine)
{
    using namespace LE::SW::GlobalParameters;
    engine.set<FFTSize>(1024);
    engine.set<OverlapFactor>(8);
    engine.set<InputGain>(0.7f);
    engine.set<OutputGain>(1.5f);
    engine.set<MixPercentage>(0.625f);
}

//------------------------------------------------------------------------------
} // anonymous namespace
//------------------------------------------------------------------------------

TEST_CASE("A saved preset loads back as itself", "[preset-roundtrip]")
{
    for (std::uint8_t effect(0); effect < Effects::Constants::numberOfEffects; ++effect)
    {
        INFO("effect " << unsigned(effect) << " (" << Effects::effectName(effect) << ')');

        std::string saved;
        std::string original;
        {
            Fixture fixture;
            auto &engine(fixture.engine());
            driveGlobals(engine);

            REQUIRE(
                engine.program()
                    .moduleChain()
                    .setParameter(0, static_cast<std::int8_t>(effect), engine.moduleInitialiser())
                    .second == static_cast<std::int8_t>(effect));

            engine.program().moduleChain().forEach<ModuleParameters>(
                [&](ModuleParameters const &module) {
                    driveModule(const_cast<ModuleParameters &>(module), effect);
                });

            original = dump(engine).text;

            std::vector<char> buffer(generousBuffer, '\0');
            auto const written(
                savePreset(buffer, juce::File(), juce::String("round trip"), engine.program()));
            REQUIRE(written > 0);
            REQUIRE(written < buffer.size());
            saved.assign(buffer.data(), written - 1 /*the terminator saveTo() appends*/);
        }

        INFO("saved preset:\n" << saved);

        Fixture reloaded;
        auto &engine(reloaded.engine());
        std::vector<char> parseBuffer(saved.begin(), saved.end());
        parseBuffer.push_back('\0'); // the parse is destructive and wants a terminator

        SWTest::clearPresetProblems();
        {
            ScopedProblemCounter const counting;
            REQUIRE(LE::SW::loadPreset(parseBuffer.data(), true, nullptr, PresetConsumer{engine}));
        }

        /// \note A preset this build wrote a moment ago must not be missing a
        /// parameter this build knows about. This is what named the TuneWorx
        /// problem: twenty one of its parameters serialised under the
        /// placeholder name "N/A", so it wrote twenty one `<N/A>` elements --
        /// not a legal XML name -- and reading one back failed the parse.
        CHECK(SWTest::presetProblems().missingParameter == 0);
        CHECK(SWTest::presetProblems().unknownEffect == 0);

        CHECK(dump(engine).text == original);
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// An enabled LFO
// --------------
//
////////////////////////////////////////////////////////////////////////////////
///
/// \note This is the one thing a preset deliberately does *not* restore, and it
/// took a failing round-trip to notice. The saver writes an LFO-controlled
/// parameter's value as the element's text; the loader
/// (ParametersLoader::getLFOParameterValue) reads the LFO first and, if it comes
/// back enabled, returns nothing rather than the value -- because the LFO is
/// what drives that parameter from now on, so the stored value is a snapshot of
/// a moving thing.
///
///   Pinned here rather than worked around silently, because it is exactly the
/// sort of asymmetry a parser rewrite can lose in either direction: stop writing
/// the value, or start applying it.
///
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("A parameter under an enabled LFO takes its value from the LFO", "[preset-roundtrip]")
{
    /// Gain is base parameter 1, the first with an LFO of its own.
    constexpr std::uint8_t gain{1};

    std::string saved;
    float drivenValue{0};
    {
        Fixture fixture;
        auto &engine(fixture.engine());

        REQUIRE(
            engine.program().moduleChain().setParameter(0, 0, engine.moduleInitialiser()).second ==
            0);

        engine.program().moduleChain().forEach<ModuleParameters>(
            [&](ModuleParameters const &constModule) {
                auto &module(const_cast<ModuleParameters &>(constModule));
                drivenValue = module.setBaseParameter(gain, offDefault(module.parameterInfo(gain)));
                driveLFO(module.baseLFO(gain - 1), 0, true /*enabled*/);
            });

        REQUIRE(drivenValue != 0);

        std::vector<char> buffer(generousBuffer, '\0');
        auto const written(savePreset(buffer, juce::File(), juce::String("lfo"), engine.program()));
        REQUIRE(written > 0);
        saved.assign(buffer.data(), written - 1);
    }

    /// The value is in the file -- it is what is not applied.
    INFO("saved preset:\n" << saved);
    CHECK(saved.find(SWTest::number(drivenValue)) != std::string::npos);

    Fixture reloaded;
    auto &engine(reloaded.engine());
    std::vector<char> parseBuffer(saved.begin(), saved.end());
    parseBuffer.push_back('\0');

    SWTest::clearPresetProblems();
    {
        ScopedProblemCounter const counting;
        REQUIRE(LE::SW::loadPreset(parseBuffer.data(), true, nullptr, PresetConsumer{engine}));
    }

    engine.program().moduleChain().forEach<ModuleParameters>([&](ModuleParameters const &module) {
        CHECK(module.getBaseParameter(gain) == module.parameterInfo(gain).default_);
        CHECK(module.baseLFO(gain - 1).enabled());
        // ...while the LFO's own settings did come back.
        CHECK(module.baseLFO(gain - 1).parameters().get<LFO::LowerBound>().getValue() == 0.125f);
        CHECK(module.baseLFO(gain - 1).parameters().get<LFO::UpperBound>().getValue() == 0.875f);
    });
}
