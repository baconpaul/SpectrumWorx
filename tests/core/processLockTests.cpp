////////////////////////////////////////////////////////////////////////////////
///
/// processLockTests.cpp
/// --------------------
///
///   What the engine does when it cannot have the processing lock.
///
///   `SpectrumWorxCore::process()` takes it with `try_lock`, because blocking
/// on the audio thread is the one thing it may not do. That is right. What was
/// wrong is what happened next: it returned, having written nothing, and every
/// caller then handed the host back a buffer it had never touched -- so a preset
/// load or an FFT-size change played whatever the previous plugin in the chain
/// had left in it, at full level. week_two.md §2.1b.
///
///   Both sides of that are pinned below. The engine's: the return value says
/// whether `outputs` was written, and it is false exactly when another thread
/// holds the lock. And the wrapper's: what a host is then given instead.
///
/// \note The wrapper case reaches the lock through `clap_plugin::plugin_data`,
/// which is how a test can hold it while the C entry point runs. That is not
/// something a host would do -- it is the one join where a test has to know more
/// than the C API says, because the contention it is reproducing comes from a
/// thread the C API has no name for.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "goldens/engineHarness.hpp"

#include "spectrumWorxCLAP.hpp"
#include "swClapEntryImpl.hpp"

#include "le/spectrumworx/engine/parameters.hpp"

#include <clap/clap.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <thread>
#include <vector>
//------------------------------------------------------------------------------
namespace
{
constexpr std::uint32_t blockSize{256};
constexpr std::uint8_t channels{2};
constexpr float sampleRate{48000};

/// An engine taken as far as a host takes it before the first block.
class RunningEngine
{
  public:
    RunningEngine()
    {
        engine_.setNumberOfChannels(channels, channels);
        engine_.setSampleRate(sampleRate);
        engine_.setBlockSize(blockSize);
        REQUIRE(engine_.initialise());
        engine_.resume();
    }

    ~RunningEngine() { engine_.suspend(); }

    RunningEngine(RunningEngine const &) = delete; // makes non-copyable
    RunningEngine &operator=(RunningEngine const &) = delete;

    SWTest::Engine &operator*() { return engine_; }
    SWTest::Engine *operator->() { return &engine_; }

  private:
    SWTest::Engine engine_;
}; // class RunningEngine

/// \brief Holds the engine's processing lock on a thread of its own for as long
/// as it is alive.
///
/// \note A second thread and not simply a second lock on this one: the critical
/// section is recursive, so an owner relocking it succeeds -- which is the
/// property SpectrumWorxCLAP::runEngine() relies on and precisely the reason a
/// same-thread test would prove nothing.
class LockHeldElsewhere
{
  public:
    explicit LockHeldElsewhere(LE::SW::SpectrumWorxCore const &engine)
        : thread_([this, &engine] {
              auto const lock(engine.getProcessingLock());
              held_.store(true, std::memory_order_release);
              while (!release_.load(std::memory_order_acquire))
                  std::this_thread::yield();
          })
    {
        while (!held_.load(std::memory_order_acquire))
            std::this_thread::yield();
    }

    ~LockHeldElsewhere()
    {
        release_.store(true, std::memory_order_release);
        thread_.join();
    }

    LockHeldElsewhere(LockHeldElsewhere const &) = delete; // makes non-copyable
    LockHeldElsewhere &operator=(LockHeldElsewhere const &) = delete;

  private:
    std::atomic<bool> held_{false};
    std::atomic<bool> release_{false};
    std::thread thread_;
}; // class LockHeldElsewhere

/// One block of whatever \p input holds, into \p output, through the engine.
bool runOneBlock(SWTest::Engine &engine, std::vector<std::vector<float>> &input,
                 std::vector<std::vector<float>> &output)
{
    std::vector<float const *> inputPointers(channels);
    std::vector<float *> outputPointers(channels);
    for (std::uint8_t channel(0); channel < channels; ++channel)
    {
        inputPointers[channel] = input[channel].data();
        outputPointers[channel] = output[channel].data();
    }
    return engine.process(inputPointers.data(), inputPointers.data(), outputPointers.data(), 1.0f,
                          blockSize);
}

/// A buffer pattern no engine output can be mistaken for.
constexpr float sentinel{-12345.0f};

bool isUntouched(std::vector<std::vector<float>> const &output)
{
    return std::all_of(output.begin(), output.end(), [](std::vector<float> const &channel) {
        return std::all_of(channel.begin(), channel.end(),
                           [](float const sample) { return sample == sentinel; });
    });
}

bool isSilent(std::vector<std::vector<float>> const &output)
{
    return std::all_of(output.begin(), output.end(), [](std::vector<float> const &channel) {
        return std::all_of(channel.begin(), channel.end(),
                           [](float const sample) { return sample == 0.0f; });
    });
}

////////////////////////////////////////////////////////////////////////////////
// The wrapper's side: the plugin a host actually holds.
////////////////////////////////////////////////////////////////////////////////

/// \brief RAII around the entry point, whose init/deinit are refcounted.
class Entry
{
  public:
    Entry() { REQUIRE(LE::SW::ClapFirst::clapInit("sw-tests")); }
    ~Entry() { LE::SW::ClapFirst::clapDeinit(); }

    Entry(Entry const &) = delete; // makes non-copyable
    Entry &operator=(Entry const &) = delete;
}; // class Entry

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

/// \brief The engine inside a plugin the host created, so that a test can hold
/// the lock a DAW's UI thread holds.
///
/// \note clap-helpers stores the C++ object in `plugin_data`; that is the whole
/// trick, and it is a downcast through a non-virtual base.
LE::SW::SpectrumWorxCore const &engineOf(clap_plugin const &plugin)
{
    auto *const pHelper(static_cast<LE::SW::PluginHelper *>(plugin.plugin_data));
    REQUIRE(pHelper != nullptr);
    return *static_cast<LE::SW::SpectrumWorxCLAP *>(pHelper);
}
} // anonymous namespace

TEST_CASE("The process lock knows which thread holds it", "[core][lock]")
{
    // It did not. `currentThreadOwnsTheProcessLock()` returned a hardcoded `true`
    // off Windows and `reinterpret_cast` the mutex to a CRITICAL_SECTION on it, so
    // all six `LE_ASSERT( currentThreadOwnsTheProcessLock() )` sites in the engine
    // asserted nothing on any platform this port has built for. The last CHECK
    // below is the one that fails against that implementation.
    RunningEngine engine;

    CHECK(!engine->currentThreadOwnsTheProcessLock());
    {
        auto const lock(engine->getProcessingLock());
        CHECK(engine->currentThreadOwnsTheProcessLock());

        // Recursive, and the ownership has to survive the inner release -- which
        // is the property SpectrumWorxCLAP::runEngine() relies on.
        {
            auto const nested(engine->getProcessingLock());
            CHECK(engine->currentThreadOwnsTheProcessLock());
        }
        CHECK(engine->currentThreadOwnsTheProcessLock());
    }
    CHECK(!engine->currentThreadOwnsTheProcessLock());

    {
        LockHeldElsewhere const contention(*engine);
        CHECK(!engine->currentThreadOwnsTheProcessLock());
    }
    CHECK(!engine->currentThreadOwnsTheProcessLock());
}

TEST_CASE("A block the engine processes says so, and is written", "[core][lock]")
{
    RunningEngine engine;

    std::vector<std::vector<float>> input(channels, std::vector<float>(blockSize));
    SWTest::generate(SWTest::Signal::Sweep, input[0], sampleRate);
    input[1] = input[0];
    std::vector<std::vector<float>> output(channels, std::vector<float>(blockSize, sentinel));

    CHECK(runOneBlock(*engine, input, output));

    // Silence would be a legitimate first block -- the engine has latency -- but
    // the sentinel would not, and that is the whole question here.
    CHECK(!isUntouched(output));
}

TEST_CASE("A block the engine declines says so, and is not written", "[core][lock]")
{
    RunningEngine engine;

    std::vector<std::vector<float>> input(channels, std::vector<float>(blockSize));
    SWTest::generate(SWTest::Signal::Sweep, input[0], sampleRate);
    input[1] = input[0];
    std::vector<std::vector<float>> output(channels, std::vector<float>(blockSize, sentinel));

    {
        LockHeldElsewhere const contention(*engine);
        CHECK(!runOneBlock(*engine, input, output));
    }

    /// \note The core leaves the buffer alone deliberately: it does not know
    /// how many of the host's channels there are, only how many it is
    /// configured for. Deciding what the host hears is the wrapper's, and
    /// SpectrumWorxCLAP::runEngine() writes silence across every output port.
    CHECK(isUntouched(output));

    // And the contention was the only reason: with the lock free again, the
    // very next block goes through.
    CHECK(runOneBlock(*engine, input, output));
    CHECK(!isUntouched(output));
}

TEST_CASE("Changing the FFT size is what holds the lock in practice", "[core][lock]")
{
    // Not a hypothetical: setGlobalParameter<FFTSize> takes the lock and
    // reallocates the whole spectral working set under it, from the UI thread,
    // while audio runs. That is the §2.1b case a user actually meets -- opening
    // the settings panel and moving the FFT size -- and this is it, in the one
    // order a test can arrange deterministically.
    using namespace LE::SW::GlobalParameters;

    RunningEngine engine;

    std::vector<std::vector<float>> input(channels, std::vector<float>(blockSize));
    SWTest::generate(SWTest::Signal::Voice, input[0], sampleRate);
    input[1] = input[0];
    std::vector<std::vector<float>> output(channels, std::vector<float>(blockSize, sentinel));

    REQUIRE(runOneBlock(*engine, input, output));

    {
        LockHeldElsewhere const contention(*engine);
        std::fill(output[0].begin(), output[0].end(), sentinel);
        std::fill(output[1].begin(), output[1].end(), sentinel);
        CHECK(!runOneBlock(*engine, input, output));
        CHECK(isUntouched(output));
    }

    engine->set<FFTSize>(1024);

    CHECK(runOneBlock(*engine, input, output));
    CHECK(!isUntouched(output));
}

TEST_CASE("A host whose block the plugin declines is given silence", "[core][lock][clap]")
{
    // The fix, from where it is visible: whatever the engine does about the
    // lock, the host's buffer is never left holding what was in it. Before this
    // the plugin returned CLAP_PROCESS_CONTINUE having written nothing, so a
    // preset load played back the previous plugin in the chain.
    Entry const entry;

    auto const *const pFactory(static_cast<clap_plugin_factory const *>(
        LE::SW::ClapFirst::getFactory(CLAP_PLUGIN_FACTORY_ID)));
    REQUIRE(pFactory != nullptr);

    auto const *const pDescriptor(pFactory->get_plugin_descriptor(pFactory, 0));
    REQUIRE(pDescriptor != nullptr);

    auto const *const pPlugin(pFactory->create_plugin(pFactory, &nullHost(), pDescriptor->id));
    REQUIRE(pPlugin != nullptr);
    REQUIRE(pPlugin->init(pPlugin));
    REQUIRE(pPlugin->activate(pPlugin, sampleRate, 1, blockSize));
    REQUIRE(pPlugin->start_processing(pPlugin));

    std::vector<float> leftIn(blockSize), rightIn(blockSize);
    SWTest::generate(SWTest::Signal::Sweep, leftIn, sampleRate);
    rightIn = leftIn;

    // Dirty, and deliberately loud: this is the buffer the host handed over, and
    // "the plugin left it alone" and "the plugin wrote silence" are only
    // distinguishable if there was something in it to begin with.
    std::vector<float> leftOut(blockSize, sentinel), rightOut(blockSize, sentinel);

    float *inputChannels[]{leftIn.data(), rightIn.data()};
    float *outputChannels[]{leftOut.data(), rightOut.data()};

    clap_audio_buffer input{&inputChannels[0], nullptr, channels, 0, 0};
    clap_audio_buffer output{&outputChannels[0], nullptr, channels, 0, 0};

    clap_process process{};
    process.steady_time = -1;
    process.frames_count = blockSize;
    process.audio_inputs = &input;
    process.audio_inputs_count = 1;
    process.audio_outputs = &output;
    process.audio_outputs_count = 1;

    {
        LockHeldElsewhere const contention(engineOf(*pPlugin));
        CHECK(pPlugin->process(pPlugin, &process) != CLAP_PROCESS_ERROR);
    }

    std::vector<std::vector<float>> const result{leftOut, rightOut};
    CHECK(!isUntouched(result));
    CHECK(isSilent(result));

    pPlugin->stop_processing(pPlugin);
    pPlugin->deactivate(pPlugin);
    pPlugin->destroy(pPlugin);
}
