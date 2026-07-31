////////////////////////////////////////////////////////////////////////////////
///
/// presetLoading.cpp
/// -----------------
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "presetLoading.hpp"

#include "editorHost.hpp"
#include "editorModuleInitialiser.hpp"
#include "spectrumWorxEditor.hpp"

#include "configuration/versionConfiguration.hpp" // MB_WARNING

#include "core/automatedModuleChain.hpp"
#include "core/host_interop/plugin2Host.hpp"
#include "core/modules/finalImplementations.hpp"
#include "core/modules/moduleDSPAndGUI.hpp"
#include "core/spectrumWorxCore.hpp"

#include "gui/gui.hpp" // warningMessageBox()

#include "le/parameters/parametersUtilities.hpp"
#include "le/spectrumworx/presetFile.hpp"

#include "le/utility/assert.hpp"
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace SW
{
//------------------------------------------------------------------------------
namespace GUI
{
//------------------------------------------------------------------------------
namespace
{
//------------------------------------------------------------------------------

/// \brief Pushes a whole GlobalParameters::Parameters through the engine's own
/// setter, one parameter at a time.
///
/// \note This was `SpectrumWorx::resetForGlobalParameters()`, and it is not the
/// same as assigning the struct: FFT size, overlap factor and window function
/// each reconfigure the engine, so a preset that changes one has to go through
/// `setGlobalParameter` rather than land in the field behind its back.
struct GlobalParameterUpdater
{
    using result_type = void;

    SpectrumWorxCore &core;

    template <class Parameter> result_type operator()(Parameter const &parameter) const
    {
        LE_VERIFY((SpectrumWorxCore::setGlobalParameter<Parameter, SpectrumWorxCore>(
            core, parameter.getValue())));
    }
}; // struct GlobalParameterUpdater

/// \brief Where a preset being loaded puts what it reads.
///
/// \note The 2016 original carried a target *program* index, because VST 2.4
/// gave a plugin 128 of them and the browser could load into one that was not
/// current -- whence `onlySetParameters()`, which meant "this program is not
/// live, so move the numbers and leave the engine alone". Neither CLAP nor the
/// harness has more than one program, so that case cannot arise and the answer
/// is a constant.
struct Loader
{
    EditorHost &host;
    SpectrumWorxEditor &editor;

    /// The chain reaches for this typedef; see presets.hpp's loadPreset().
    using Module = SpectrumWorxCore::Module;

    Program &program() const { return host.core().program(); }
    GlobalParameters::Parameters &targetGlobalParameters() const { return program().parameters(); }
    AutomatedModuleChain &targetChain() const { return program().moduleChain(); }

    Host2PluginInteropControler::AutomationBlocker automationBlocker() const
    {
        return {host.core()};
    }
    Utility::CriticalSectionLock processingLock() const { return host.core().getProcessingLock(); }

    /// \note The editor-aware initialiser, so that a preset which fills slots
    /// builds their UI regions as it goes rather than leaving five modules with
    /// nowhere to draw.
    EditorModuleInitialiser moduleInitialiser() const
    {
        return {host.core().moduleInitialiser(), &editor};
    }

    static bool onlySetParameters() { return false; }

    bool setNewGlobalParameters(GlobalParameters::Parameters const &newParameters) const
    {
        LE::Parameters::forEach(newParameters, GlobalParameterUpdater{host.core()});
        return true;
    }

    void moduleChainFinished(std::uint8_t const moduleCount, bool const syncedLFOFound) const
    {
        editor.setLastModulePosition(moduleCount);

        /// \note Some hosts (Renoise 2.8) provide tempo information lazily,
        /// after the first process() call, so the transport position is checked
        /// too rather than warning at a host that simply has not said yet.
        ///                                   (03.07.2012.) (Domagoj Saric)
        using Timer = SpectrumWorxCore::LFO::Timer;
        if (syncedLFOFound && !Timer::hasTempoInformation() &&
            host.core().lfoTimer().currentTimeInBars())
        {
            warningMessageBox(MB_WARNING,
                              "Loaded preset uses tempo-synced LFOs but the host does not "
                              "provide tempo information.",
                              false);
        }
    }
}; // struct Loader

struct Consumer
{
    EditorHost &host;
    SpectrumWorxEditor &editor;

    using Module = Loader::Module;

    /// \note `ignoreExternalSample` has no subject while the side channel's file
    /// loader is compiled out -- presets.hpp does not ask a consumer about the
    /// sample at all in that configuration. 5.0 brings both back.
    Loader presetLoader(bool /*ignoreExternalSample*/) const { return {host, editor}; }

    Program &program() const { return host.core().program(); }

    void notifyHostAboutPresetChangeBegin() const { host.automation().presetChangeBegin(); }
    void notifyHostAboutPresetChangeEnd() const { host.automation().presetChangeEnd(); }
}; // struct Consumer

//------------------------------------------------------------------------------
} // anonymous namespace
//------------------------------------------------------------------------------

bool loadPreset(EditorHost &host, SpectrumWorxEditor &editor, juce::File const &presetFile,
                bool const ignoreExternalSample, juce::String *const comment,
                char const *const presetName)
{
    return SW::loadPreset(presetFile, ignoreExternalSample, comment, presetName,
                          Consumer{host, editor});
}

//------------------------------------------------------------------------------
} // namespace GUI
//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
