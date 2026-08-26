////////////////////////////////////////////////////////////////////////////////
///
/// undoTests.cpp
/// -------------
///
///   The undo history over a real plugin: what a step is, what one takes back,
/// and what a *sequence* of them does.
///
///   Sequences are the point. Every one of the four faults this suite exists
/// for survived cases that did one thing and checked it: the value set aside for
/// a gesture that filled up and stayed full, the snapshot taken from three
/// different moments at once, the flag left off by an early return, the browser
/// row that never moved. They only showed when one action followed another --
/// "load, load, undo, load, knob, undo" is the report that found two of them --
/// so the cases here walk a sequence and ask after each step.
///
/// \note What they read is the *name* of what undo would take back, and the
/// values it puts there. Both are stable: no pixels, no timing, no message loop.
/// A name is also what the user reads off the control, so a failure here says
/// what a user would have seen.
///
/// \note No editor. A plugin with one open is not reachable from a test -- see
/// the note in editorHarness.hpp on SWTest::Instance, which is a host of its own
/// with no CLAP layer under it -- so what an undo does to a *widget* is checked
/// by hand. What it does to the engine, the main thread's Program and the host
/// is all here.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "clap/testHost.hpp"

#include "core/parameterID.hpp"
#include "undoHistory.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>
//------------------------------------------------------------------------------
namespace
{
using namespace SWTest;

using ParameterID = LE::SW::ParameterID;

/// The chain's "this slot is empty".
constexpr std::int8_t noModule{-1};

ParameterID idOf(clap_param_info const &info)
{
    ParameterID parameterID;
    parameterID.binaryValue = info.id;
    return parameterID;
}

double valueOf(clap_plugin const &plugin, clap_plugin_params const &params,
               clap_param_info const &info)
{
    double value{0};
    REQUIRE(params.get_value(&plugin, info.id, &value));
    return value;
}

/// \brief What undo would take back, or "" -- the caption on the control.
std::string undoName(LE::SW::GUI::EditorHost const &host)
{
    auto const *const pName(host.undoName());
    return pName ? pName : "";
}

std::string redoName(LE::SW::GUI::EditorHost const &host)
{
    auto const *const pName(host.redoName());
    return pName ? pName : "";
}

////////////////////////////////////////////////////////////////////////////////
///
/// \brief One whole parameter edit, as the editor makes one.
///
/// \note The value goes into the model first and the gesture is reported around
/// it, which is the order the editor uses and not the order that reads
/// naturally: a wheel notch and a typed value both write and *then* say they
/// were an edit. \see ModuleControlBase::publishValue().
///
////////////////////////////////////////////////////////////////////////////////

void editAsUserWould(LE::SW::GUI::EditorHost &host, ParameterID const parameterID,
                     float const value)
{
    host.automation().automatedParameterBeginEdit(parameterID);
    host.editParameter(parameterID, value);
    host.automation().automatedParameterEndEdit(parameterID);
}

////////////////////////////////////////////////////////////////////////////////
///
/// \brief A preset load, in the shape the loader makes one.
///
/// \note Which is a bracket around a run of parameter writes that carry no
/// gesture of their own -- the load is one step, not one per parameter. Each
/// global is written back as it stands: what matters is that the writes happen
/// and are ungestured, and a value of this test's own would have to be a legal
/// one for every parameter it touched, which the FFT size is fussy about.
///
////////////////////////////////////////////////////////////////////////////////

void loadAPresetOver(LE::SW::GUI::EditorHost &host, clap_plugin const &plugin,
                     clap_plugin_params const &params)
{
    host.automation().presetChangeBegin();

    for (auto const &info : allParameterInfo(plugin, params))
    {
        auto const parameterID(idOf(info));
        if (parameterID.type() != ParameterID::GlobalParameter)
            continue;
        host.editParameter(parameterID, static_cast<float>(valueOf(plugin, params, info)));
    }

    host.automation().presetChangeEnd();
}

//------------------------------------------------------------------------------
} // anonymous namespace
//------------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
// One step at a time
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("A fresh plugin has nothing to undo", "[clap][undo]")
{
    Entry const entry;
    TestHost host{TestHost::everything()};
    ActivePlugin plugin(48000, 512, host);

    auto const &editorHost(editorHostOf(*plugin));
    CHECK_FALSE(editorHost.canUndo());
    CHECK_FALSE(editorHost.canRedo());
}

TEST_CASE("A knob edit is undone and redone", "[clap][undo]")
{
    Entry const entry;
    TestHost host{TestHost::everything()};
    ActivePlugin plugin(48000, 512, host);

    auto &editorHost(editorHostOf(*plugin));
    auto const &params(parameters(*plugin));
    auto const global(firstGlobalParameter(*plugin, params));

    auto const before(valueOf(*plugin, params, global));
    auto const wanted(aDifferentValue(*plugin, params, global));
    REQUIRE(before != wanted);

    editAsUserWould(editorHost, idOf(global), static_cast<float>(wanted));
    REQUIRE(valueOf(*plugin, params, global) == Catch::Approx(wanted));
    REQUIRE(editorHost.canUndo());

    editorHost.undo();
    CHECK(valueOf(*plugin, params, global) == Catch::Approx(before));
    CHECK_FALSE(editorHost.canUndo());
    REQUIRE(editorHost.canRedo());

    editorHost.redo();
    CHECK(valueOf(*plugin, params, global) == Catch::Approx(wanted));
    CHECK(editorHost.canUndo());
}

TEST_CASE("Filling a slot is undone as one slot change", "[clap][undo]")
{
    //   Not as a whole state: undoing an added module used to go through the
    // preset loader, which builds a new chain and swaps it in, and the rack then
    // redrew every strip. \see UndoHistory::ChainEdit.
    Entry const entry;
    TestHost host{TestHost::everything()};
    ActivePlugin plugin(48000, 512, host);

    auto &editorHost(editorHostOf(*plugin));
    auto const &params(parameters(*plugin));
    auto const selector(slotSelector(*plugin, params, 0));

    auto const empty(valueOf(*plugin, params, selector));

    REQUIRE(editorHost.editSlot(0, 0));
    editorHost.automation().gestureBegin("Add module");
    editorHost.automation().gestureEnd();
    plugin.pumpMainThread();

    REQUIRE(valueOf(*plugin, params, selector) != empty);
    REQUIRE(undoName(editorHost) == "Add module");

    editorHost.undo();
    plugin.pumpMainThread();

    CHECK(valueOf(*plugin, params, selector) == Catch::Approx(empty));
}

TEST_CASE("Undoing a module removal brings its values back", "[clap][undo]")
{
    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \note The half of a chain edit that cannot be said as "put the slot
    /// back": refilling one builds a *new* module with nothing in it, so the
    /// step has to carry what the destroyed one held.
    ///
    /// \note The value is written in the effect's own units and read back in the
    /// host's, and they are not the same number -- a module parameter is
    /// normalised at the host edge over whatever range the effect owns. So this
    /// moves the parameter *somewhere else* and remembers what the host then
    /// reads, rather than naming a value in one unit and asserting it in the
    /// other.
    ///
    ////////////////////////////////////////////////////////////////////////////
    Entry const entry;
    TestHost host{TestHost::everything()};
    ActivePlugin plugin(48000, 512, host);

    auto &editorHost(editorHostOf(*plugin));
    auto const &params(parameters(*plugin));

    REQUIRE(editorHost.editSlot(0, 0));
    editorHost.automation().gestureBegin("Add module");
    editorHost.automation().gestureEnd();
    plugin.pumpMainThread();

    auto const owned(moduleParameterIn(*plugin, params, 0));
    auto const before(valueOf(*plugin, params, owned));

    auto const moveTo([&](float const internal) {
        editAsUserWould(editorHost, idOf(owned), internal);
        plugin.pumpMainThread();
        return valueOf(*plugin, params, owned);
    });

    auto set(moveTo(1.0f));
    if (set == Catch::Approx(before))
        set = moveTo(0.0f);
    REQUIRE(set != Catch::Approx(before));

    REQUIRE(editorHost.editSlot(0, noModule));
    editorHost.automation().gestureBegin("Remove module");
    editorHost.automation().gestureEnd();
    plugin.pumpMainThread();

    REQUIRE(undoName(editorHost) == "Remove module");

    editorHost.undo();
    plugin.pumpMainThread();

    auto const selector(slotSelector(*plugin, params, 0));
    CHECK(valueOf(*plugin, params, selector) != noModule);
    CHECK(valueOf(*plugin, params, owned) == Catch::Approx(set));
}

////////////////////////////////////////////////////////////////////////////////
// Sequences
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("A knob turned after a preset load undoes the knob", "[clap][undo]")
{
    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \note Reported as "load, knob, undo -- and it undoes the load". A preset
    /// writes every global through editParameter() and brackets none of them,
    /// and each of those writes set a value aside in case a gesture wanted it.
    /// There were four places to set one aside; a load filled all four and
    /// nothing released them, so from the first preset onwards a knob had
    /// nowhere to record where it started from and no drag was ever a step.
    ///
    /// \note A *module* parameter, and it has to be. A global already had a
    /// value set aside by the load, so a gesture on one found an entry and
    /// recorded -- the wrong value, from before the preset, but recorded. The
    /// knobs on a strip are the ones with nothing waiting for them, and they are
    /// the ones a user drags.
    ///
    ////////////////////////////////////////////////////////////////////////////
    Entry const entry;
    TestHost host{TestHost::everything()};
    ActivePlugin plugin(48000, 512, host);

    auto &editorHost(editorHostOf(*plugin));
    auto const &params(parameters(*plugin));

    REQUIRE(editorHost.editSlot(0, 0));
    editorHost.automation().gestureBegin("Add module");
    editorHost.automation().gestureEnd();
    plugin.pumpMainThread();

    loadAPresetOver(editorHost, *plugin, params);
    REQUIRE(undoName(editorHost) == "Load preset");

    auto const owned(moduleParameterIn(*plugin, params, 0));
    auto const before(valueOf(*plugin, params, owned));

    editAsUserWould(editorHost, idOf(owned), 1.0f);
    plugin.pumpMainThread();
    auto const set(valueOf(*plugin, params, owned));
    REQUIRE(set != Catch::Approx(before));

    // the knob, not the load
    REQUIRE(undoName(editorHost) != "Load preset");

    editorHost.undo();
    plugin.pumpMainThread();

    CHECK(valueOf(*plugin, params, owned) == Catch::Approx(before));
    // and the load is what is left to take back
    CHECK(undoName(editorHost) == "Load preset");
}

TEST_CASE("Undo walks back through a sequence one step at a time", "[clap][undo]")
{
    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \note "Load, load, undo, load, knob, undo", which is the report this
    /// suite was written for. Three loads in a row used to record three steps
    /// that all described the state before the *first* of them, because the
    /// snapshot was taken between the loader's two passes and refreshed before
    /// the load it belonged to had finished.
    ///
    ////////////////////////////////////////////////////////////////////////////
    Entry const entry;
    TestHost host{TestHost::everything()};
    ActivePlugin plugin(48000, 512, host);

    auto &editorHost(editorHostOf(*plugin));
    auto const &params(parameters(*plugin));

    REQUIRE(editorHost.editSlot(0, 0));
    editorHost.automation().gestureBegin("Add module");
    editorHost.automation().gestureEnd();
    plugin.pumpMainThread();

    loadAPresetOver(editorHost, *plugin, params);
    loadAPresetOver(editorHost, *plugin, params);
    CHECK(undoName(editorHost) == "Load preset");

    editorHost.undo();
    plugin.pumpMainThread();
    CHECK(undoName(editorHost) == "Load preset");
    CHECK(redoName(editorHost) == "Load preset");

    // a new action, so the way forward closes
    loadAPresetOver(editorHost, *plugin, params);
    CHECK(redoName(editorHost) == "");

    auto const owned(moduleParameterIn(*plugin, params, 0));
    editAsUserWould(editorHost, idOf(owned), 1.0f);
    plugin.pumpMainThread();
    auto const knobStep(undoName(editorHost));
    CHECK(knobStep != "Load preset");

    //   And back down the stack: the knob, then the three loads, then the module
    // that was added before any of them.
    editorHost.undo();
    plugin.pumpMainThread();
    CHECK(undoName(editorHost) == "Load preset");

    //   Two, not three: the middle undo above already took one of them back,
    // and the load after it went on top of what was left rather than beside it.
    for (int load(0); load < 2; ++load)
    {
        REQUIRE(editorHost.canUndo());
        editorHost.undo();
        plugin.pumpMainThread();
    }

    CHECK(undoName(editorHost) == "Add module");

    editorHost.undo();
    plugin.pumpMainThread();
    CHECK_FALSE(editorHost.canUndo());
}

TEST_CASE("The history keeps no more steps than it says", "[clap][undo]")
{
    //   Each edit is its own step: the same parameter twice inside the
    // coalescing window is one, so these are spread over separate parameters.
    Entry const entry;
    TestHost host{TestHost::everything()};
    ActivePlugin plugin(48000, 512, host);

    auto &editorHost(editorHostOf(*plugin));
    auto const &params(parameters(*plugin));

    for (std::size_t beyond(0); beyond < LE::SW::UndoHistory::depth + 4; ++beyond)
        loadAPresetOver(editorHost, *plugin, params);

    std::size_t taken(0);
    while (editorHost.canUndo())
    {
        editorHost.undo();
        plugin.pumpMainThread();
        ++taken;
    }

    CHECK(taken == LE::SW::UndoHistory::depth);
}
