////////////////////////////////////////////////////////////////////////////////
///
/// \file engineSetupStalenessTests.cpp
/// -----------------------------------
///
///   What the audio thread may read while the UI thread is mid-change.
///
///   `SpectrumWorxCore::engineSetup()` asserts that the setup still agrees with
/// the *spectral* parameters -- `isEngineSetupUpToDate()` compares the FFT size,
/// the overlap factor and the window size factor, and nothing else. Between a
/// parameter moving and `updateEngineSetup()` running, it does not, and that
/// window is real: `setGlobalParameter(FFTSize&, …)` sets the value and then
/// updates, under the processing lock.
///
///   `SpectrumWorxCLAP::process()` reads the sample rate through
/// `getSampleRate()` and the channel count through the setup, on every block,
/// *before* `SpectrumWorxCore::process()` takes that lock. Neither field is one
/// the assertion compares, so both were asserting about a staleness that cannot
/// affect the value they return -- and loading a factory preset that changes the
/// FFT size fired it every time audio was running.
///
/// \note This does not test the race, which needs two threads and is 5.8's to
/// fix properly. It tests the property that makes the race harmless to these two
/// readers: a stale setup still answers them, and answers correctly.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "goldens/engineHarness.hpp"

#include "le/spectrumworx/engine/parameters.hpp"
#include "le/spectrumworx/engine/setup.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
//------------------------------------------------------------------------------
namespace
{
//------------------------------------------------------------------------------

using namespace LE;
using namespace LE::SW;

//------------------------------------------------------------------------------
} // anonymous namespace
//------------------------------------------------------------------------------

TEST_CASE("A stale engine setup still answers the audio thread", "[engine-setup]")
{
    SWTest::Engine engine;
    engine.setNumberOfChannels(2, 2);
    engine.setSampleRate(48000);
    engine.setBlockSize(512);
    REQUIRE(engine.initialise());

    REQUIRE(engine.getSampleRate() == 48000);
    auto const channels(engine.uncheckedEngineSetup().numberOfChannels());
    REQUIRE(channels == 2);

    /// \note The parameter, not setGlobalParameter(): going through the setter
    /// would update the setup, which is exactly the state being avoided here.
    /// This is what the engine looks like from another thread partway through
    /// that call.
    auto const staleFFTSize(static_cast<GlobalParameters::FFTSize::param_type>(
        engine.uncheckedEngineSetup().fftSize<unsigned int>() * 2));
    engine.parameters().get<GlobalParameters::FFTSize>().setValue(staleFFTSize);

    // i.e. the setup no longer agrees with the parameters.
    REQUIRE(engine.uncheckedEngineSetup().fftSize<unsigned int>() != staleFFTSize);

    /// \note Both of these asserted here, which in a debug plugin is a break in
    /// the audio callback. Neither reads a field the staleness touches.
    CHECK(engine.getSampleRate() == 48000);
    CHECK(engine.uncheckedEngineSetup().numberOfChannels() == channels);
}
