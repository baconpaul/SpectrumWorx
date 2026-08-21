////////////////////////////////////////////////////////////////////////////////
///
/// \file spectralPreallocationTests.cpp
/// ------------------------------------
///
///   That changing the FFT size allocates nothing.
///
///   The spectral working set used to be sized for exactly the setup in force,
/// so moving the FFT size or the overlap factor reallocated it -- which is why
/// the change had to wait for `deactivate()`, and why the plugin asked the host
/// to restart it. A host that does not honour `clap_host::request_restart` then
/// never applies the change at all: Ardour answers `kIoChanged` with
/// `kNotImplemented` and the handler is `#if 0`, so the first FFT size change
/// wedged the engine for the life of the instance. \see issue #172.
///
///   `Engine::reserveStorage()` sizes the block for every spectral setup
/// reachable at this channel count and sample rate, and the resize paths only
/// ever grow it. A setup change is then pointer re-layout inside a block that is
/// already there -- no allocation, and so nothing the audio thread may not do
/// itself.
///
/// \note The sweep starts at the *smallest* setup deliberately. Reserving is
/// invisible from the largest one: 8192 by 8 needs the most storage, so a run
/// that begins there never has to grow whatever the policy is. From 128 by 1,
/// every step up would reallocate without the reserve.
///
/// \note What this does not cover is the per-module channel state, which has its
/// own block and its own reserve (`ModuleDSP::allocateStorage`). Nothing here
/// can reach it -- `storage()` is protected and belongs to the module rather
/// than to the engine -- so its regression cover is the effects and golden
/// suites, which render all fifty-seven effects and would notice a channel state
/// laid out in a block sized for the wrong FFT.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "goldens/engineHarness.hpp"

#include "le/spectrumworx/engine/configuration.hpp"
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

TEST_CASE("A spectral setup change reallocates nothing", "[engine-setup][preallocation]")
{
    using Engine::Constants::maximumFFTSize;
    using Engine::Constants::maximumOverlapFactor;
    using Engine::Constants::minimumFFTSize;
    using Engine::Constants::minimumOverlapFactor;

    SWTest::Engine engine;
    engine.setNumberOfChannels(2, 2);
    engine.setSampleRate(48000);
    engine.setBlockSize(512);

    /// \note The smallest reachable setup, before initialise(), so that the one
    /// allocation this test allows is made for it and every later step has to
    /// grow to be satisfied.
    engine.parameters().get<GlobalParameters::FFTSize>().setValue(minimumFFTSize);
    engine.parameters().get<GlobalParameters::OverlapFactor>().setValue(minimumOverlapFactor);
    REQUIRE(engine.initialise());
    REQUIRE(engine.uncheckedEngineSetup().fftSize<unsigned int>() == minimumFFTSize);

    auto const *const block(engine.sharedStorage().data());
    auto const blockSize(engine.sharedStorage().size());
    REQUIRE(block != nullptr);
    REQUIRE(blockSize > 0);

    for (unsigned int fftSize(minimumFFTSize); fftSize <= maximumFFTSize; fftSize *= 2)
    {
        for (unsigned int overlap(minimumOverlapFactor); overlap <= maximumOverlapFactor;
             overlap *= 2)
        {
            INFO("fftSize " << fftSize << ", overlap factor " << overlap);

            REQUIRE(engine.set<GlobalParameters::FFTSize>(
                static_cast<GlobalParameters::FFTSize::param_type>(fftSize)));
            REQUIRE(engine.set<GlobalParameters::OverlapFactor>(
                static_cast<GlobalParameters::OverlapFactor::param_type>(overlap)));

            /// \note The harness engine is suspended, so the setters apply
            /// straight away rather than deferring; assert that rather than
            /// assume it, since a deferred setup would make the checks below
            /// pass for the wrong reason.
            REQUIRE_FALSE(engine.spectralSetupPending());

            CHECK(engine.uncheckedEngineSetup().fftSize<unsigned int>() == fftSize);
            CHECK(engine.uncheckedEngineSetup().windowOverlappingFactor<unsigned int>() == overlap);

            // The whole point: same block, same size, laid out differently.
            CHECK(engine.sharedStorage().data() == block);
            CHECK(engine.sharedStorage().size() == blockSize);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
///
/// \note And the other direction, because "never grows" is only half of it: a
/// block that shrank on the way back down would reallocate on the way up again,
/// and the sweep above only ever climbs within a row.
///
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("The reserve survives a setup change back down", "[engine-setup][preallocation]")
{
    using Engine::Constants::maximumFFTSize;
    using Engine::Constants::minimumFFTSize;

    SWTest::Engine engine;
    engine.setNumberOfChannels(2, 2);
    engine.setSampleRate(48000);
    engine.setBlockSize(512);
    REQUIRE(engine.initialise());

    auto const *const block(engine.sharedStorage().data());
    auto const blockSize(engine.sharedStorage().size());

    REQUIRE(engine.set<GlobalParameters::FFTSize>(
        static_cast<GlobalParameters::FFTSize::param_type>(maximumFFTSize)));
    CHECK(engine.sharedStorage().data() == block);
    CHECK(engine.sharedStorage().size() == blockSize);

    REQUIRE(engine.set<GlobalParameters::FFTSize>(
        static_cast<GlobalParameters::FFTSize::param_type>(minimumFFTSize)));
    CHECK(engine.sharedStorage().data() == block);
    CHECK(engine.sharedStorage().size() == blockSize);

    REQUIRE(engine.set<GlobalParameters::FFTSize>(
        static_cast<GlobalParameters::FFTSize::param_type>(maximumFFTSize)));
    CHECK(engine.sharedStorage().data() == block);
    CHECK(engine.sharedStorage().size() == blockSize);
}
