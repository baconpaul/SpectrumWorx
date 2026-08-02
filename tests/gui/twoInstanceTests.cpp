////////////////////////////////////////////////////////////////////////////////
///
/// twoInstanceTests.cpp
/// --------------------
///
///   What two instances of the plugin in one process share, and must not.
///
///   SpectrumWorx deadlocks in Logic and in Bitwig with two instances open, and
/// the first cause found for that was that closing one editor tore JUCE down
/// under the other: `ReferenceCountedGUIInitializationGuard` called
/// `juce::shutdownJuce_GUI()` directly, outside the `numScopedInitInstances`
/// counter `juce::ScopedJuceInitialiser_GUI` uses -- so
/// `DeletedAtShutdown::deleteAll()` and `MessageManager::deleteInstance()` ran
/// while the CLAP shim, and the other instance, still held live references.
/// The counter and both singletons are one per binary, which is to say shared by
/// every instance of this plugin the host has loaded.
///
///   The editors are constructed directly rather than through the shim, as
/// tools/show-ui does: what is being tested is our own lifetime bookkeeping, and
/// the shim's half is stood in for by the ScopedJuceInitialiser_GUI each case
/// holds -- which is exactly the reference the old code deleted out from under.
///
/// See doc/tech/correct_the_threading.md.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "goldens/engineHarness.hpp"
#include "presets/presetHarness.hpp"

#include "core/automatedModuleChain.hpp"
#include "core/host_interop/plugin2Host.hpp"

/// \note Before anything that names SW::Module: loadPreset() is a template over
/// its consumer and downcasts a chain node to it, so this translation unit needs
/// the complete type -- and this is the header that has it.
#include "core/modules/moduleDSPAndGUI.hpp"

#include "gui/editor/editorHost.hpp"
#include "gui/editor/spectrumWorxEditor.hpp"
#include "gui/resources.hpp"
#include "gui/theme.hpp"

#include "le/spectrumworx/presetFile.hpp"
#include "le/spectrumworx/presets.hpp"

#include <juce_gui_basics/juce_gui_basics.h>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <memory>
#include <vector>
//------------------------------------------------------------------------------
namespace
{
using namespace LE;
using namespace LE::SW;

/// \note Every one of these is a notification travelling plugin -> host, and
/// there is no host.
class SilentNotifications final : public Plugin2HostInteropControler
{
  private:
    void automatedParameterBeginEdit(ParameterID) const override {}
    void automatedParameterEndEdit(ParameterID) const override {}
    void gestureBegin(char const *) const override {}
    void gestureEnd() const override {}
    void automatedParameterChanged(ParameterID, ParameterValueForAutomation) const override {}
    void moduleChanged(std::uint8_t, Module const *) const override {}
    bool parameterListChanged() const override { return true; }
    void presetChangeBegin() const override {}
    void presetChangeEnd() const override {}
    bool latencyChanged() override { return true; }

#if LE_SW_ENGINE_INPUT_MODE >= 2
    bool hostTryIOConfigurationChange(std::uint8_t, std::uint8_t) override { return false; }
    bool hostSupportsIOConfigurationChanges() const override { return false; }
#endif // LE_SW_ENGINE_INPUT_MODE >= 2
}; // class SilentNotifications

/// One plugin's worth of everything an editor reaches into.
class Instance final : public GUI::EditorHost
{
  public:
    Instance()
    {
        engine_.setNumberOfChannels(2, 2);
        engine_.setSampleRate(48000);
        engine_.setBlockSize(512);
        REQUIRE(engine_.initialise());
    }

    void openEditor() { pEditor_ = std::make_unique<GUI::SpectrumWorxEditor>(*this); }
    void closeEditor() { pEditor_.reset(); }
    GUI::SpectrumWorxEditor &editor() const
    {
        REQUIRE(pEditor_ != nullptr);
        return *pEditor_;
    }

    SpectrumWorxCore &core() override { return engine_; }
    Plugin2HostInteropControler &automation() override { return notifications_; }

    /// \note Real ones, and nobody drains them: what the editor asks for goes
    /// into the queue and stays there. These cases are about lifetime, and a
    /// queue that fills would be a finding rather than a nuisance.
    Threading::ToEngineQueue &toEngine() const override { return toEngine_; }
    Threading::ValueMailbox const &modulatedValues() const override { return values_; }

    void editorOpened(GUI::SpectrumWorxEditor &) override {}
    void editorClosed() override {}

    juce::File currentSampleFile() const override { return {}; }
    void setNewSample(juce::File const &) override {}
    bool isSampleLoadInProgress() const override { return false; }
    void registerSampleLoadedListener(GUI::SpectrumWorxEditor &) override {}
    void deregisterSampleLoadedListener(GUI::SpectrumWorxEditor const &) override {}

    bool completelyDisableIOChanges() const override { return false; }
    bool shouldLoadLastSessionOnStartup() const override { return false; }
    void shouldLoadLastSessionOnStartup(bool) override {}

  private:
    SWTest::Engine engine_;
    SilentNotifications notifications_;
    mutable Threading::ToEngineQueue toEngine_;
    Threading::ValueMailbox values_;
    std::unique_ptr<GUI::SpectrumWorxEditor> pEditor_;
}; // class Instance

/// \brief JUCE, owned the way the shim owns it: one reference held across
/// everything the case does.
struct HostSideJuce
{
    juce::ScopedJuceInitialiser_GUI initialiser;
    /// The skin caches juce::Images, and no juce::Image may outlive JUCE.
    ~HostSideJuce() { GUI::releaseCachedResources(); }
};
} // anonymous namespace

TEST_CASE("Closing one editor leaves JUCE standing for the other", "[gui][two-instances]")
{
    HostSideJuce const juceIsUp;
    REQUIRE(juce::MessageManager::getInstanceWithoutCreating() != nullptr);

    Instance a, b;

    a.openEditor();
    b.openEditor();
    CHECK(GUI::Theme::haveSingleton());

    a.closeEditor();

    // The decisive one. Against the previous implementation this is null: our own
    // reference count reached zero and called shutdownJuce_GUI(), deleting the
    // MessageManager that `juceIsUp` -- standing in for the shim -- still holds.
    CHECK(juce::MessageManager::getInstanceWithoutCreating() != nullptr);
    CHECK(GUI::Theme::haveSingleton());

    // ...and the survivor is still a working editor.
    CHECK(b.editor().getWidth() == GUI::SpectrumWorxEditor::estimatedWidth);

    // Reopening the one that closed is the case a host meets when a user toggles
    // a window, and it is where the old teardown produced a MessageManager that
    // nobody had declared a thread for.
    a.openEditor();
    CHECK(juce::MessageManager::getInstanceWithoutCreating() != nullptr);

    b.closeEditor();
    a.closeEditor();

    // Both gone, and JUCE is still up because something above us holds it.
    CHECK(juce::MessageManager::getInstanceWithoutCreating() != nullptr);
    CHECK(!GUI::Theme::haveSingleton());
}

TEST_CASE("Selection and the active control belong to an editor, not to the process",
          "[gui][two-instances]")
{
    HostSideJuce const juceIsUp;

    Instance a, b;
    a.openEditor();
    b.openEditor();

    // They were one pointer each, file-scope, shared by every instance in the
    // host. That they are addressable per editor at all is the change; that both
    // start empty and stay independent is what a headless case can say without a
    // window server to give something focus.
    CHECK(a.editor().selectedModule() == nullptr);
    CHECK(b.editor().selectedModule() == nullptr);
    CHECK(a.editor().activeControl() == nullptr);
    CHECK(b.editor().activeControl() == nullptr);

    a.closeEditor();

    // Nothing about b changed because a went away. Against the shared statics,
    // whatever a had selected would have been b's answer too -- and freed.
    CHECK(b.editor().selectedModule() == nullptr);
    CHECK(b.editor().activeControl() == nullptr);

    b.closeEditor();
}

TEST_CASE("A preset load counts its problems instead of raising dialogs",
          "[gui][two-instances][presets]")
{
    // Symptom 1: the preset load test leaks a juce object. The default reporter
    // was GUI::warningMessageBox, one per problem, and a 2011 preset does not
    // mention the parameters its effect grew later -- 806 across the factory
    // banks. This case installs no reporter of its own on purpose: if the default
    // still raised dialogs, JUCE's leak detector would fail the run at exit.
    std::filesystem::path const bank(std::filesystem::path(SW_PRESET_DATA_DIR) / "Voices");
    REQUIRE(std::filesystem::is_directory(bank));

    takePresetLoadReport(); // whatever an earlier case left

    // The whole bank rather than one file: how many parameters a preset fails to
    // mention depends on which effects it uses and when it was written, and
    // plenty of them mention everything. What matters is that a run over real
    // 2011 files produces a count at all.
    unsigned int loaded(0);
    for (auto const &entry : std::filesystem::directory_iterator(bank))
    {
        if (entry.path().extension() != ".swp")
            continue;

        auto const presetData(readPresetFile(juce::File(entry.path().string())));
        REQUIRE(presetData);
        std::string_view const text(presetData.get());
        std::vector<char> buffer(text.begin(), text.end());
        buffer.push_back('\0'); // the parse is destructive and wants a terminator

        /// \note One engine per preset, as presetCorpusTests.cpp does: loading B
        /// on top of A is a merge, and it is not what this case is about.
        SWTest::Engine engine;
        engine.setNumberOfChannels(2, 2);
        engine.setSampleRate(48000);
        engine.setBlockSize(512);
        REQUIRE(engine.initialise());

        REQUIRE(LE::SW::loadPreset(buffer.data(), true /*ignore external samples*/, nullptr,
                                   SWTest::PresetConsumer{engine}));
        ++loaded;
    }
    REQUIRE(loaded > 0);

    auto const report(takePresetLoadReport());
    INFO("over " << loaded << " presets");
    CHECK(report.missingParameters > 0);
    CHECK(report.failures == 0);

    // And taking it clears it, so the next load's summary is the next load's.
    CHECK(takePresetLoadReport().total() == 0);
}
