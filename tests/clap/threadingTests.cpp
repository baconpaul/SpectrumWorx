////////////////////////////////////////////////////////////////////////////////
///
/// \file threadingTests.cpp
/// ------------------------
///
///   What happens when the two threads the plugin has are both busy.
///
///   Everything else under `tests/clap/` drives one thread at a time, which is
/// the right way to test what a call *does*. This file is for the cases where
/// the answer depends on two of them overlapping: a preset arriving while blocks
/// are being rendered, a restart asked for from both sides at once, a chain
/// built against a spectral setup that changes before it is installed.
///
/// \note Most of these cannot fail deterministically, and saying so is the point
/// rather than an apology. A data race is undefined behaviour, not a wrong
/// answer that shows up once in a hundred runs -- the compiler is entitled to
/// keep a racy read in a register and it does. So each case here does two jobs:
/// it asserts the outcome that must hold however the two threads interleave, and
/// it *creates* the overlap so that a `-fsanitize=thread` build has something to
/// report. The second is what actually pins the fix; the first is what keeps the
/// case honest in the ordinary build everyone runs.
///
///   To run them as they were written to be run:
///
///     cmake -B build-tsan -D SW_SANITIZER=thread -D SW_BUILD_PLUGIN_BUNDLES=OFF
///     cmake --build build-tsan --target sw-plugin-tests
///     ./build-tsan/tests/sw-plugin-tests "[threading]"
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "clap/testHost.hpp"

#include "core/spectrumWorxCore.hpp"
#include "gui/editor/presetLoading.hpp"
#include "le/spectrumworx/presetStorage.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <thread>
#include <vector>
//------------------------------------------------------------------------------
namespace
{
//------------------------------------------------------------------------------

using namespace SWTest;

constexpr double sampleRate{48000};
constexpr std::uint32_t blockSize{64};

/// \brief A factory preset whose FFT size is not the default one.
///
/// \note 8192 against the engine's default 2048, and named rather than swept.
/// The whole family of Tier 2 findings is about a *spectral* parameter moving
/// while audio runs -- a preset that happens to agree with the running setup
/// exercises none of it, and there are 213 of those. This one is also the
/// largest FFT the build offers, so the working set it asks for grows rather
/// than shrinks: a chain installed against the old size has too little room
/// rather than too much, which is the difference between a wrong answer and a
/// heap overrun.
std::filesystem::path presetWithABiggerFFT()
{
    std::filesystem::path const preset(std::filesystem::path(SW_PRESET_DATA_DIR) / "Martin Walker" /
                                       "Gamma Shift" / "Whistle A Tune.swp");
    REQUIRE(std::filesystem::is_regular_file(preset));
    return preset;
}

unsigned int runningFFTSize(clap_plugin const &plugin)
{
    return editorHostOf(plugin).core().uncheckedEngineSetup().fftSize<unsigned int>();
}

/// \brief The description of one parameter, found by the id a host addresses it
/// by rather than by its position in the list.
clap_param_info infoFor(clap_plugin const &plugin, clap_id const id)
{
    for (auto const &info : allParameterInfo(plugin, parameters(plugin)))
        if (info.id == id)
            return info;
    FAIL("no parameter with that id");
    return {};
}

////////////////////////////////////////////////////////////////////////////////
///
/// \class AudioThread
///
/// \brief A thread rendering blocks back to back for as long as it is in scope,
/// the way a host with the transport rolling does.
///
/// \note `processStatus` rather than `process`, and no `REQUIRE` anywhere on
/// this thread: Catch2's assertion machinery writes a shared counter, so an
/// assertion here is a race in the harness that tsan reports at length while
/// saying nothing about the plugin. What this thread saw is carried out in
/// atomics and checked after the join.
///
////////////////////////////////////////////////////////////////////////////////

class AudioThread
{
  public:
    /// \param automation an event list delivered with every block, as a host with
    /// a lane running does. Null is a host that is only rendering.
    explicit AudioThread(ActivePlugin &plugin, clap_input_events const *const automation = nullptr)
        : thread_([this, &plugin, automation] {
              std::vector<float> leftIn(blockSize, 0.0f), rightIn(blockSize, 0.0f);
              std::vector<float> leftOut(blockSize), rightOut(blockSize);
              while (!stop_.load(std::memory_order_acquire))
              {
                  if (plugin.processStatus(leftIn, rightIn, leftOut, rightOut, nullptr,
                                           automation) == CLAP_PROCESS_ERROR)
                      failed_.store(true, std::memory_order_release);
                  blocks_.fetch_add(1, std::memory_order_acq_rel);
              }
          })
    {
        // Nothing the case does happens before the audio thread is really going.
        while (blocks_.load(std::memory_order_acquire) == 0)
        {
        }
    }

    ~AudioThread() { join(); }

    AudioThread(AudioThread const &) = delete; // makes non-copyable
    AudioThread &operator=(AudioThread const &) = delete;

    void join()
    {
        if (!thread_.joinable())
            return;
        stop_.store(true, std::memory_order_release);
        thread_.join();
    }

    /// \note Only after join(), which is what makes reading them ordered.
    bool failed() const { return failed_.load(std::memory_order_acquire); }
    unsigned int blocks() const { return blocks_.load(std::memory_order_acquire); }

  private:
    std::atomic<bool> stop_{false};
    std::atomic<bool> failed_{false};
    std::atomic<unsigned int> blocks_{0};
    std::thread thread_;
}; // class AudioThread

//------------------------------------------------------------------------------
} // anonymous namespace
//------------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
///
/// \note T2.5. Three flags crossed threads as plain `bool`s and this is the case
/// that has two threads at all of them at once.
///
///   `restartRequested_` is the one with something to lose. It gates
/// `clap_host::request_restart`, and it is read-and-then-written at two sites --
/// `drainCommands()`'s tail on the audio thread and
/// `HostProxy::automatedParameterChanged` on the main thread, which is where a
/// preset load arrives. "Test it and set it" is not one operation on a `bool`, so
/// the two can agree to ask twice; worse, a racy read is undefined behaviour and
/// the compiler may hold it in a register, in which case the request is never
/// made at all. Nothing then applies the pending setup: the engine goes on
/// running one FFT size while every parameter readout says another, until some
/// unrelated change asks again.
///
///   What the case can assert without depending on an interleaving is the
/// outcome: whichever thread wins, the host is asked, and once it honours the
/// restart the engine is running the size the preset named. What pins the fix is
/// the tsan run -- before it, `spectralSetupPending_`, `restartRequested_` and
/// `blockAutomation_` are each named as a write/read race between the two
/// threads.
///
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("A preset that changes the FFT size while audio runs still gets its restart",
          "[clap][threading][presets]")
{
    Entry const entry;

    TestHost host(TestHost::everything());
    ActivePlugin plugin(sampleRate, blockSize, host);

    REQUIRE(runningFFTSize(*plugin) != 8192);

    auto presetData(LE::SW::readPresetFile(presetWithABiggerFFT()));
    REQUIRE(presetData);

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \note And a lane running while the browser is clicked, which is what makes
    /// the third of the three flags cross threads at all. `blockAutomation_` is
    /// raised by the preset loader on the main thread and was read by
    /// `SpectrumWorxCore::blockAutomation()`'s assertion, reached from
    /// `setParameter()` -- which for a host automation event is the audio thread.
    /// With no events in the block there is no reader and the flag looks
    /// single-threaded.
    ///
    ///   Adding them is what turned that assertion up: it claimed a preset load
    /// could not be in progress here, and roughly one run in ten proved otherwise
    /// by aborting inside the audio callback. A checked build did that on two
    /// ordinary user actions at once, and a shipped build did nothing whatsoever,
    /// `LE_ASSERT` being a no-op there. \see SpectrumWorxCore::blockAutomation().
    ///
    /// \note The first global parameter, whose default value is a legal one by
    /// construction, and deliberately not a spectral one: a lane fighting the
    /// preset over the FFT size would be testing which of them won rather than
    /// whether the two threads may touch these at all.
    ///
    ////////////////////////////////////////////////////////////////////////////
    auto const inputGain(infoFor(*plugin, parameterID(globalType, 0, 0)));
    OneParameterEvent const automation(inputGain.id, inputGain.default_value);

    {
        AudioThread audio(plugin, &*automation);

        /// \note No editor, so nothing is reported to a user and nothing draws --
        /// this is the load itself against a running engine, which is what a
        /// browser click with the transport rolling is underneath.
        REQUIRE(LE::SW::GUI::loadPreset(editorHostOf(*plugin), nullptr, presetData.get(),
                                        true /*ignore external samples*/, nullptr, "Whistle"));

        audio.join();
        CHECK_FALSE(audio.failed());
    }

    // The preset moved a spectral parameter, so somebody had to ask.
    CHECK(host.restartRequests >= 1);

    /// \note And the restart is where it lands. Until the host honours one the
    /// engine is *supposed* to still be running the old size -- that is the whole
    /// design of the deferral -- so this is the assertion that the request was not
    /// lost on the way.
    plugin.restartIfAsked();
    CHECK(runningFFTSize(*plugin) == 8192);
}

////////////////////////////////////////////////////////////////////////////////
///
/// \note T2.1, and it needs no second thread at all -- only a host that does not
/// render between the preset and the restart it asks for. Logic with the
/// transport parked is the reported one; every host does it while a track is
/// muted or the plugin is offline.
///
///   A preset is parsed into a chain built against the storage factors in force
/// at the time, and handed over through `Threading::publishChain()`, which with
/// audio running *queues* it. The restart the preset's FFT size then asks for
/// lands in `deactivate()`, which resized the live chain -- and the preset's was
/// still in the ring. So the first `process()` after the restart spliced in
/// modules sized for 2048 and ran them at 8192, writing per-bin channel state
/// past the end of the block each of them owns.
///
///   The case is written around the *observable* half of that: which chain ends
/// up live and at what size. The overrun itself needs a sanitizer to see, and an
/// `-fsanitize=address` build reports it here as a heap-buffer-overflow inside
/// the first block.
///
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("A chain queued behind a restart is resized before it is played",
          "[clap][threading][presets]")
{
    Entry const entry;

    TestHost host(TestHost::everything());
    ActivePlugin plugin(sampleRate, blockSize, host);

    auto &engine(editorHostOf(*plugin).core());
    REQUIRE(runningFFTSize(*plugin) != 8192);
    REQUIRE(engine.moduleChain().size() == 0);

    auto presetData(LE::SW::readPresetFile(presetWithABiggerFFT()));
    REQUIRE(presetData);

    REQUIRE(LE::SW::GUI::loadPreset(editorHostOf(*plugin), nullptr, presetData.get(),
                                    true /*ignore external samples*/, nullptr, "Whistle"));

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \note Not one block between the load and the restart, which is the whole
    /// case. The plugin is active, so `publishChain()` queued the chain instead
    /// of installing it, and nothing has drained the ring: the engine is still
    /// running the previous, empty chain at the previous FFT size.
    ///
    ////////////////////////////////////////////////////////////////////////////
    CHECK(engine.moduleChain().size() == 0);
    CHECK(runningFFTSize(*plugin) != 8192);
    CHECK(host.restartRequests >= 1);

    plugin.restartIfAsked();

    // The restart is where both halves land, and they have to land together.
    CHECK(runningFFTSize(*plugin) == 8192);
    CHECK(engine.moduleChain().size() == 3);

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \note And then the blocks that used to run the old modules at the new
    /// size. Enough of them to reach a spectral frame, which is the part that
    /// took some finding: at 8192 with an overlap factor of 4 a hop is 2048
    /// samples and a block here is 64, so **no module runs at all for the first
    /// 32 blocks**. One block after the restart looks perfectly healthy.
    ///
    ///   What it looks like when it is not: `Math::multiply` reports "Buffer
    /// sizes mismatch" from inside `PitchShifter::process`, an input of the new
    /// FFT's width against an output sized for the old one -- and then writes the
    /// input's length anyway. Only the engine's own bounds assertion sees it.
    /// ASan does not: modules are suballocated out of one `HeapSharedStorage`
    /// arena, so running off the end of a module's range lands in the next
    /// module's, which is a live part of an allocation the sanitizer knows to be
    /// live. In a shipped build the assertion is not compiled and the write is
    /// simply made.
    ///
    ////////////////////////////////////////////////////////////////////////////

    std::vector<float> leftIn(blockSize, 0.25f), rightIn(blockSize, 0.25f);
    std::vector<float> leftOut(blockSize), rightOut(blockSize);
    for (unsigned int block(0); block < 64; ++block)
    {
        plugin.process(leftIn, rightIn, leftOut, rightOut);
        REQUIRE(std::all_of(leftOut.begin(), leftOut.end(),
                            [](float const sample) { return std::isfinite(sample); }));
    }
}

////////////////////////////////////////////////////////////////////////////////
///
/// \note T2.6. `activate()` is `[main-thread & !active]`, and a host that ignores
/// the second half of that gets the whole spectral working set reallocated under
/// a `process()` that is reading it.
///
///   clap-helpers is not the backstop it looks like. It reports the double
/// activation, and when the sample rate differs it simulates a deactivation
/// first -- but at the *same* rate it reports and calls through, with only an
/// `assert` in between, which no shipped build compiles.
///
/// \note Against `nullHost()` deliberately, where every other case here uses a
/// `TestHost`. Offering `clap.thread-check` would turn clap-helpers' own contract
/// checking on, and this harness cannot answer it truthfully with two threads
/// live: `TestHost` tracks one audio-callback window for the whole host, so while
/// the audio thread is inside `process()` it answers "not the main thread" to the
/// main thread as well. That is the harness's limit, not the plugin's, and
/// borrowing it here would only produce a misbehaviour report about the test.
///
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("A second activate while the first is still processing changes nothing",
          "[clap][threading]")
{
#ifndef NDEBUG
    SKIP("A checked build cannot reach the plugin at all: clap-helpers' own "
         "`assert( !_isActive )` aborts first. That assertion is exactly what a shipped build "
         "does not have, which is what this is about.");
#endif // NDEBUG

    Entry const entry;

    ActivePlugin plugin(sampleRate, blockSize);
    auto const fftSizeBefore(runningFFTSize(*plugin));

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \note An input gain that is not unity, delivered with every block, and it
    /// is what makes the reallocation observable rather than merely wrong.
    /// `SpectrumWorxCore::process()` passes the host's own pointers straight
    /// through while the gain is 1 and copies into `buffers_` only when it is
    /// not -- so at the default gain the buffers `setBlockSize()` frees are
    /// buffers nothing is reading.
    ///
    ////////////////////////////////////////////////////////////////////////////
    auto const inputGain(infoFor(*plugin, parameterID(globalType, 0, 0)));
    OneParameterEvent const loud(inputGain.id, inputGain.max_value);

    {
        AudioThread audio(plugin, &*loud);

        ////////////////////////////////////////////////////////////////////////
        ///
        /// \note The same sample rate and a *different* maximum block size, which
        /// is the arrangement with something to reallocate. clap-helpers keys its
        /// simulated deactivation on the sample rate alone, so this walks
        /// straight through it -- and at an unchanged rate and block size
        /// `resize()` finds nothing to do, which is why activating twice with
        /// identical arguments is harmless and proves nothing.
        ///
        ///   Many, not one: the window in which a reallocation overlaps a block
        /// is the length of a `process()` call.
        ///
        ////////////////////////////////////////////////////////////////////////
        for (unsigned int attempt(0); attempt < 256; ++attempt)
            CHECK(plugin->activate(&*plugin, sampleRate, 1,
                                   (attempt % 2) ? blockSize * 8 : blockSize));

        audio.join();
        CHECK_FALSE(audio.failed());
        CHECK(audio.blocks() > 0);
    }

    // Still the plugin it was, and still able to render.
    CHECK(runningFFTSize(*plugin) == fftSizeBefore);

    std::vector<float> leftIn(blockSize, 0.25f), rightIn(blockSize, 0.25f);
    std::vector<float> leftOut(blockSize), rightOut(blockSize);
    plugin.process(leftIn, rightIn, leftOut, rightOut);
    CHECK(std::all_of(leftOut.begin(), leftOut.end(),
                      [](float const sample) { return std::isfinite(sample); }));
}
