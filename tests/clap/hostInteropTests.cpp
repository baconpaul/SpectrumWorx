////////////////////////////////////////////////////////////////////////////////
///
/// \file hostInteropTests.cpp
/// --------------------------
///
///   What the plugin says to its host, and what it does with what the host says
/// back.
///
///   Everything here needs a host that answers rather than one that merely
/// exists, which is why none of it could be written before `TestHost`. Three
/// things it reaches that nothing did:
///
///   - **The main-thread arm of the deferral.** `markCurrentProgramAsModified()`
///     branches on `_host.canUseThreadCheck() && _host.isMainThread()`
///     (spectrumWorxCLAP.cpp:1329). No test host offered `clap.thread-check`, so
///     only the else was ever taken and the branch was half dead code.
///
///   - **The output event list.** Every case in the tree hands `process()` and
///     `flush()` a list that throws away what it is given, so what the *editor*
///     tells the host -- the value events and the gesture pair around them -- has
///     never been looked at.
///
///   - **clap-helpers' own contract checking.** `ensureMainThread` and
///     `ensureAudioThread` return immediately unless the host answers a thread
///     check (plugin.hxx:2262-2287), and `CheckingLevel::Maximal` is what this
///     plugin is built at. Offering `clap.log` catches what they report.
///
/// See doc/tech/week_two.md §2.3 and §5.1.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "clap/testHost.hpp"

#include "core/host_interop/plugin2Host.hpp"
#include "core/parameterID.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <string>
#include <vector>
//------------------------------------------------------------------------------
namespace
{
//------------------------------------------------------------------------------

using namespace SWTest;

/// \brief The first parameter a host is shown that belongs to no slot.
///
/// \note A global rather than a module parameter, for two reasons: it exists
/// whatever the slots hold -- an event for a parameter no effect owns is dropped
/// by handleEvent() before it reaches anything this file is about -- and it keeps
/// its own range at the host edge, so a value in the effect's own units and a
/// value the host would write are the same number. The three main knobs are
/// globals, so this is also what a real drag moves.
clap_param_info firstGlobalParameter(clap_plugin const &plugin, clap_plugin_params const &params)
{
    for (auto const &info : allParameterInfo(plugin, params))
        if (std::strcmp(info.module, "Global") == 0)
            return info;
    FAIL("no global parameter");
    return {};
}

/// Somewhere in the parameter's range that is not where it already is.
double aDifferentValue(clap_plugin const &plugin, clap_plugin_params const &params,
                       clap_param_info const &info)
{
    double current{0};
    REQUIRE(params.get_value(&plugin, info.id, &current));
    auto const middle((info.min_value + info.max_value) / 2);
    return (current == middle) ? info.max_value : middle;
}

/// Everything the host was told, in one string, so a failure says what.
std::string joined(std::vector<std::string> const &lines)
{
    std::string all;
    for (auto const &line : lines)
        all += "\n  " + line;
    return all;
}

//------------------------------------------------------------------------------
} // anonymous namespace
//------------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
// Which thread the plugin thinks it is on
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("A host that answers the thread check is marked dirty where it stands", "[clap][host]")
{
    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \note The arm that had never run. `clap_host_state::mark_dirty` is
    /// `[main-thread]`, so a plugin that cannot establish which thread it is on
    /// has to defer -- which is what every host in this tree forced, and what
    /// pluginTests.cpp's "A host with state and no thread check" case pins. A DAW
    /// offers the thread check; against one, the mark belongs *here*, in the call,
    /// and a deferral would be a pointless round trip through the message loop.
    ///
    /// \note An edit made in the *editor* is the main-thread route, and very
    /// nearly the only one. `params.flush()` is `[active ? audio-thread :
    /// main-thread]` (ext/params.h:303) and this plugin is active, so a host
    /// event is an audio-thread event whether it arrives through flush() or
    /// through process(); the case below is that half. What is left on the main
    /// thread is the window: a knob, a preset load, a session restore.
    ///
    ////////////////////////////////////////////////////////////////////////////
    Entry const entry;
    TestHost host{TestHost::everything()};
    ActivePlugin plugin(48000, 512, host);

    auto const &params(parameters(*plugin));
    auto const global(firstGlobalParameter(*plugin, params));
    auto const wanted(aDifferentValue(*plugin, params, global));

    LE::SW::ParameterID target;
    target.binaryValue = global.id;

    auto const callbacksBefore(host.mainThreadCallbacks);

    editorHostOf(*plugin).automation().automatedParameterChanged(
        target,
        {static_cast<float>(wanted),
         static_cast<float>((wanted - global.min_value) / (global.max_value - global.min_value))});

    // It asked...
    CHECK(host.threadChecks > 0);
    // ...and acted on the answer, without a callback to come back on.
    CHECK(host.dirtyMarks > 0);
    CHECK(host.mainThreadCallbacks == callbacksBefore);
}

TEST_CASE("The same host is not marked dirty from inside the audio callback", "[clap][host]")
{
    // The other arm, against a host that *can* be asked -- which is what makes
    // this different from the deferral a thread-check-less host gets. The plugin
    // asks, is told this is not the main thread, and defers on the strength of
    // the answer rather than on the absence of one.
    Entry const entry;
    TestHost host{TestHost::everything()};
    ActivePlugin plugin(48000, 512, host);

    auto const &params(parameters(*plugin));
    auto const global(firstGlobalParameter(*plugin, params));

    OneParameterEvent const edit(global.id, aDifferentValue(*plugin, params, global));

    std::vector<float> leftIn(512, 0.0f), rightIn(512, 0.0f);
    std::vector<float> leftOut(512), rightOut(512);
    plugin.process(leftIn, rightIn, leftOut, rightOut, nullptr, &*edit);

    CHECK(host.threadChecks > 0);
    CHECK(host.dirtyMarks == 0);
    CHECK(host.mainThreadCallbacks > 0);

    // ...and the deferral is only correct if it really does arrive later.
    plugin->on_main_thread(&*plugin);
    CHECK(host.dirtyMarks == 1);
}

////////////////////////////////////////////////////////////////////////////////
// What the editor sends out
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("A knob drag reaches the host as a balanced gesture around its value", "[clap][host]")
{
    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \note What a host does with an unbalanced pair is latch the parameter's
    /// automation lane in write mode until something else ends it, which is a
    /// user-visible fault with no failure anywhere near it. `flushUIEdits()` is
    /// the only thing that can produce one and nothing has ever read its output.
    ///
    /// \note Through `automation()`, which is the interface the editor holds --
    /// `SpectrumWorxEditor::host()` is exactly this reference. A test stands in
    /// for the knob; the three calls are the ones a drag makes.
    ///
    ////////////////////////////////////////////////////////////////////////////
    Entry const entry;
    TestHost host{TestHost::everything()};
    ActivePlugin plugin(48000, 512, host);

    auto const &params(parameters(*plugin));
    auto const global(firstGlobalParameter(*plugin, params));
    auto const wanted(aDifferentValue(*plugin, params, global));

    LE::SW::ParameterID target;
    target.binaryValue = global.id;

    auto &automation(editorHostOf(*plugin).automation());
    auto const flushRequestsBefore(host.flushRequests);

    automation.automatedParameterBeginEdit(target);
    automation.automatedParameterChanged(
        target,
        {static_cast<float>(wanted),
         static_cast<float>((wanted - global.min_value) / (global.max_value - global.min_value))});
    automation.automatedParameterEndEdit(target);

    // Nothing has been sent yet: the editor queues and asks the host to come and
    // collect, because a host takes parameter changes only through the output
    // list it hands to process() or flush().
    CHECK(host.flushRequests > flushRequestsBefore);

    RecordedOutputEvents recorded;
    plugin.flush(nullptr, &*recorded);

    auto const mine(recorded.forParameter(global.id));
    REQUIRE(mine.size() == 3);
    CHECK(mine[0].type == CLAP_EVENT_PARAM_GESTURE_BEGIN);
    CHECK(mine[1].type == CLAP_EVENT_PARAM_VALUE);
    CHECK(mine[2].type == CLAP_EVENT_PARAM_GESTURE_END);

    // Balanced across the whole list, not just this parameter's slice.
    CHECK(recorded.count(CLAP_EVENT_PARAM_GESTURE_BEGIN) ==
          recorded.count(CLAP_EVENT_PARAM_GESTURE_END));

    // And the value is the one that was dragged to, in the units this parameter
    // advertises rather than the ones the editor works in.
    CHECK(mine[1].value >= global.min_value);
    CHECK(mine[1].value <= global.max_value);
    CHECK(mine[1].value != 0);

    // Drained, not copied: a second flush has nothing left to send.
    recorded.clear();
    plugin.flush(nullptr, &*recorded);
    CHECK(recorded.events().empty());
}

TEST_CASE("Edits made while audio runs come out of process(), not flush()", "[clap][host]")
{
    // The same queue, collected from the other end. A host that is playing never
    // calls flush() -- ext/params.h says so: "flush() will not be called while
    // the plugin is processing" -- so the edits have to leave through the output
    // list process() is handed, or a knob moved during playback is silently lost.
    Entry const entry;
    TestHost host{TestHost::everything()};
    ActivePlugin plugin(48000, 512, host);

    auto const &params(parameters(*plugin));
    auto const global(firstGlobalParameter(*plugin, params));
    auto const wanted(aDifferentValue(*plugin, params, global));

    LE::SW::ParameterID target;
    target.binaryValue = global.id;

    auto &automation(editorHostOf(*plugin).automation());
    automation.automatedParameterBeginEdit(target);
    automation.automatedParameterChanged(
        target,
        {static_cast<float>(wanted),
         static_cast<float>((wanted - global.min_value) / (global.max_value - global.min_value))});
    automation.automatedParameterEndEdit(target);

    RecordedOutputEvents recorded;
    std::vector<float> leftIn(512, 0.0f), rightIn(512, 0.0f);
    std::vector<float> leftOut(512), rightOut(512);
    plugin.process(leftIn, rightIn, leftOut, rightOut, nullptr, nullptr, &*recorded);

    auto const mine(recorded.forParameter(global.id));
    REQUIRE(mine.size() == 3);
    CHECK(mine[0].type == CLAP_EVENT_PARAM_GESTURE_BEGIN);
    CHECK(mine[2].type == CLAP_EVENT_PARAM_GESTURE_END);
}

////////////////////////////////////////////////////////////////////////////////
// The contract clap-helpers checks when it can
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Driven the way a DAW drives it, nobody misbehaves", "[clap][host]")
{
    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \note This case is two assertions wearing one coat, and both are worth
    /// having. clap-helpers reports the *plugin* misbehaving -- a callback run
    /// off the main thread, a lifecycle call out of order -- and the *host*
    /// misbehaving, which here means this harness: `ensureAudioThread` fires when
    /// `process()` was called with no TestHost::AudioCallback around it. A test
    /// harness that drives the audio entry points from nowhere in particular is
    /// not reproducing what a DAW does, and until a host answered the thread
    /// check there was no way to find out that it was not.
    ///
    ///   All of it is invisible without `clap.log`: the reports go to `std::cerr`
    /// otherwise (host-proxy.hxx:91-107), where a green run swallows them.
    ///
    ////////////////////////////////////////////////////////////////////////////
    Entry const entry;
    TestHost host{TestHost::everything()};

    {
        ActivePlugin plugin(48000, 512, host);
        auto const &params(parameters(*plugin));

        // The whole of what a host asks a plugin about its parameters.
        auto const infos(allParameterInfo(*plugin, params));
        for (auto const &info : infos)
        {
            double value{0};
            CHECK(params.get_value(&*plugin, info.id, &value));
            std::array<char, 128> text{};
            CHECK(params.value_to_text(&*plugin, info.id, value, text.data(), text.size()));
        }

        // An effect in a slot, a rescan collected, and audio through it.
        OneParameterEvent const fill(parameterID(moduleChainType, 0), 0);
        plugin.flush(&*fill);
        plugin->on_main_thread(&*plugin);

        std::vector<float> leftIn(512, 0.0f), rightIn(512, 0.0f);
        std::vector<float> leftOut(512), rightOut(512);
        for (unsigned block(0); block < 8; ++block)
            plugin.process(leftIn, rightIn, leftOut, rightOut);

        // And a restart, which is deactivate/activate with the audio entry points
        // on the audio thread either side of it.
        plugin.restartIfAsked();
        for (unsigned block(0); block < 8; ++block)
            plugin.process(leftIn, rightIn, leftOut, rightOut);
    }
    // ...including the teardown, which is where "host forgot to deactivate the
    // plugin before destroying it" would be reported.

    INFO("what the plugin was reported for:" << joined(host.pluginMisbehaviours()));
    CHECK(host.pluginMisbehaviours().empty());

    /// \note And the harness, which is the half that has already earned its keep:
    /// this went red the first time it ran, on `params.flush()` being called from
    /// the main thread against an active plugin. See TestHost::flush().
    INFO("what this harness was reported for:" << joined(host.hostMisbehaviours()));
    CHECK(host.hostMisbehaviours().empty());
}
