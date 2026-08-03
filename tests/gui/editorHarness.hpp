////////////////////////////////////////////////////////////////////////////////
///
/// \file editorHarness.hpp
/// -----------------------
///
///   One plugin's worth of everything a SpectrumWorxEditor reaches into, with no
/// host and no plugin format under it. The editors are constructed directly
/// rather than through the CLAP shim, as tools/show-ui does: what these cases
/// test is our own bookkeeping, and the shim's half is stood in for by the
/// ScopedJuceInitialiser_GUI that HostSideJuce holds.
///
/// \note Header rather than a .cpp because every member is inline and the whole
/// thing is 60 lines; a second translation unit would be more build than
/// harness. It was twoInstanceTests.cpp's anonymous namespace until a second
/// file needed an editor.
///                                           (03.08.2026.) (SW port)
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef editorHarness_hpp__4D0F2A57_9C4E_4B2C_9D71_5B0E2F6A3C18
#define editorHarness_hpp__4D0F2A57_9C4E_4B2C_9D71_5B0E2F6A3C18
//------------------------------------------------------------------------------
#include "goldens/engineHarness.hpp"

#include "core/host_interop/plugin2Host.hpp"

#include "gui/editor/editorHost.hpp"
#include "gui/editor/spectrumWorxEditor.hpp"
#include "gui/resources.hpp"

#include <juce_gui_basics/juce_gui_basics.h>

#include <catch2/catch_test_macros.hpp>

#include <memory>
//------------------------------------------------------------------------------
namespace SWTest
{
//------------------------------------------------------------------------------
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
    /// into the queue and stays there. These cases are about the interface side,
    /// and a queue that fills would be a finding rather than a nuisance.
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
    Engine engine_;
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
}; // struct HostSideJuce

//------------------------------------------------------------------------------
} // namespace SWTest
//------------------------------------------------------------------------------
#endif // editorHarness_hpp
