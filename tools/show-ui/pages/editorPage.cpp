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
#include "core/automatedModuleChain.hpp"
#include "gui/editor/editorModuleInitialiser.hpp"
#include "le/spectrumworx/effects/configuration/effectNames.hpp"
#include "gui/editor/spectrumWorxEditor.hpp"
#include "gui/theme.hpp"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdio>
#include <cstdlib>
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
    /// \param sampleRate zero to leave the engine unset up, which is the state a
    /// plugin is in between its constructor and activate() -- and the state a
    /// standalone restores a session in. A knob whose range quantises to a step
    /// time or a bin width has neither to work from then; see
    /// ModuleKnob::updateForEngineSetupChanges.
    explicit HarnessEngine(float const sampleRate)
    {
        setProgram(program_);
        if (sampleRate <= 0)
            return;

        setNumberOfChannels(2, 2);
        setSampleRate(sampleRate);
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
    explicit HarnessHost(float const sampleRate) : engine_(sampleRate) {}

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
    /// \param sampleRate zero for an engine that has not been set up, which is
    /// what a session restored before activate() sees. See HarnessEngine.
    /// \param openPresetBrowser opens the browser as the presets button does.
    EditorPage(bool const withModuleInFirstSlot, float const sampleRate,
               bool const openPresetBrowser = false)
        : host_(sampleRate)
    {
        /// \note No setLookAndFeel() here, unlike the other pages: the editor's
        /// own ReferenceCountedGUIInitializationGuard makes Theme the default
        /// LookAndFeel (gui.cpp), and a second reference to it outlives that
        /// guard -- which JUCE asserts on when the singleton goes.
        editor_ = std::make_unique<GUI::SpectrumWorxEditor>(host_);
        addAndMakeVisible(*editor_);
        setSize(editor_->getWidth(), editor_->getHeight());

        if (withModuleInFirstSlot)
            fillFirstSlot();

        if (openPresetBrowser)
        {
            /// \note SW_SHOW_UI_PRESET_BANK opens the browser inside a factory
            /// bank rather than at its root, so a single page can be swept over
            /// the eighteen of them from a shell loop -- the same idea as
            /// SW_SHOW_UI_EFFECT above.
            if (auto const *const bank = std::getenv("SW_SHOW_UI_PRESET_BANK"))
                editor_->showFactoryBank(bank);
            else
                editor_->showPresetBrowser(true);
        }
    }

    /// \note The editor goes before the host it holds a reference to.
    ~EditorPage() override { editor_.reset(); }

    void resized() override { editor_->setTopLeftPosition(0, 0); }

  private:
    /// \brief Adds an effect exactly as choosing it from the module menu does.
    ///
    /// \note Through the editor's own entry point rather than through the module
    /// chain, because adding a module is five steps and only the first two are
    /// reachable from the chain: create, build the region, take focus, select,
    /// notify the host. Driving the chain directly renders a perfectly good
    /// picture while testing none of the last three, which is where the bugs have
    /// been.
    ///
    /// \note SW_SHOW_UI_EFFECT picks which effect, so a single page can be swept
    /// over the whole list from a shell loop. Building a module's widgets is
    /// per-effect work -- every widget type an effect uses is a template
    /// instantiation of its own, and the ranges are quantised against the engine
    /// setup per parameter -- so "one effect renders" says very little about the
    /// other fifty-six.
    void fillFirstSlot()
    {
        std::uint8_t effectIndex{0};
        if (auto const *const requested = std::getenv("SW_SHOW_UI_EFFECT"))
            effectIndex = static_cast<std::uint8_t>(std::atoi(requested));

        std::fprintf(stderr, "sw-show-ui: slot 1 <- effect %u (%s)\n", unsigned(effectIndex),
                     Effects::effectName(effectIndex));

        editor_->addUserAddedModule(effectIndex);

        auto const pModule(host_.core().moduleChain().module(0));
        LE_ASSERT_MSG(pModule, "The harness could not create a module.");
    }

    HarnessHost host_;
    std::unique_ptr<GUI::SpectrumWorxEditor> editor_;
}; // class EditorPage

SWShowUI::PageRegistration const registration{
    "editor", "the whole editor, over a headless engine",
    [] { return std::unique_ptr<juce::Component>(std::make_unique<EditorPage>(false, 48000)); }};

SWShowUI::PageRegistration const registrationWithModule{
    "editor-module", "the editor with an effect in the first slot",
    [] { return std::unique_ptr<juce::Component>(std::make_unique<EditorPage>(true, 48000)); }};

/// \note The order a standalone starts in, and the one that broke: a session is
/// restored -- modules created and their UI built -- before activate() has given
/// the engine a sample rate, so a knob whose range quantises to a step time has
/// nothing to quantise against.
SWShowUI::PageRegistration const registrationBeforeSetup{
    "editor-module-cold", "an effect added before the engine has a sample rate",
    [] { return std::unique_ptr<juce::Component>(std::make_unique<EditorPage>(true, 0)); }};

/// \note The presets button, which had no headless coverage at all until it
/// asserted on its first real press -- `presetsFolder()` was half of a two-phase
/// initialisation whose initialiser nothing called. Constructing the browser is
/// most of what that button does: it reads six skin bitmaps, builds a list box
/// and a comment editor, asks where the user's presets are and lists them.
SWShowUI::PageRegistration const registrationWithPresetBrowser{
    "editor-presets", "the editor with the preset browser open", [] {
        return std::unique_ptr<juce::Component>(
            std::make_unique<EditorPage>(true, 48000, true /*preset browser*/));
    }};

//------------------------------------------------------------------------------
} // anonymous namespace
//------------------------------------------------------------------------------
