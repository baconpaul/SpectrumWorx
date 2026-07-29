////////////////////////////////////////////////////////////////////////////////
///
/// \file editorPage.cpp
/// --------------------
///
///   The whole editor, with an engine under it and no plugin.
///
///   This is the page that matters: everything else here shows a layer, and
/// this shows whether the layers add up. The editor's constructor builds three
/// knobs, a module menu, a gradient, the slot widgets and whatever the module
/// chain already holds, and any one of those can assert or throw. Rendering it
/// offscreen catches that without a DAW, which is the only way to catch it at
/// all in a sandbox with no window server.
///
/// \note The EditorHost below is the honest minimum: a SpectrumWorxCore for the
/// engine half and a Plugin2HostInteropControler whose notifications go
/// nowhere, because there is no host to notify.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "../page.hpp"

#include "core/host_interop/plugin2Host.hpp"
#include "core/modules/moduleDSPAndGUI.hpp"
#include "core/spectrumWorxCore.hpp"
#include "gui/editor/editorHost.hpp"
#include "gui/editor/spectrumWorxEditor.hpp"
#include "gui/theme.hpp"

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
//------------------------------------------------------------------------------
namespace
{
//------------------------------------------------------------------------------

using namespace LE;
using namespace LE::SW;

/// The engine, which cannot be instantiated on its own -- SpectrumWorxCore's
/// constructor is protected and Engine::Processor downcasts to it.
class HarnessEngine final : public SpectrumWorxCore
{
  public:
    HarnessEngine()
    {
        setProgram(program_);
        setNumberOfChannels(2, 2);
        setSampleRate(48000);
        setBlockSize(512);
        initialise();
    }

  private:
    Program program_;
}; // class HarnessEngine

/// \note Every one of these is a notification travelling plugin -> host, and
/// there is no host. They are pure virtual rather than optional because a real
/// plugin format needs all of them; see spectrumWorxCLAP.hpp for the CLAP ones.
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

class HarnessHost final : public GUI::EditorHost
{
  public:
    SpectrumWorxCore &core() override { return engine_; }
    Plugin2HostInteropControler &automation() override { return notifications_; }

    void editorOpened(GUI::SpectrumWorxEditor &) override {}
    void editorClosed() override {}

    bool completelyDisableIOChanges() const override { return false; }
    bool shouldLoadLastSessionOnStartup() const override { return false; }
    void shouldLoadLastSessionOnStartup(bool) override {}

  private:
    HarnessEngine engine_;
    SilentNotifications notifications_;
}; // class HarnessHost

/// Owns the host so that it outlives the editor reaching into it.
class EditorPage final : public juce::Component
{
  public:
    EditorPage()
    {
        /// \note No setLookAndFeel() here, unlike the other pages: the editor's
        /// own ReferenceCountedGUIInitializationGuard makes Theme the default
        /// LookAndFeel (gui.cpp), and a second reference to it outlives that
        /// guard -- which JUCE asserts on when the singleton goes.
        editor_ = std::make_unique<GUI::SpectrumWorxEditor>(host_);
        addAndMakeVisible(*editor_);
        setSize(editor_->getWidth(), editor_->getHeight());
    }

    /// \note The editor goes before the host it holds a reference to.
    ~EditorPage() override { editor_.reset(); }

    void resized() override { editor_->setTopLeftPosition(0, 0); }

  private:
    HarnessHost host_;
    std::unique_ptr<GUI::SpectrumWorxEditor> editor_;
}; // class EditorPage

SWShowUI::PageRegistration const registration{
    "editor", "the whole editor, over a headless engine",
    [] { return std::unique_ptr<juce::Component>(std::make_unique<EditorPage>()); }};

//------------------------------------------------------------------------------
} // anonymous namespace
//------------------------------------------------------------------------------
