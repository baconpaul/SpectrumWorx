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
#include "spectrumWorxEditor.hpp"

#include "configuration/versionConfiguration.hpp" // MB_WARNING

#include "core/automatedModuleChain.hpp"
#include "core/host_interop/plugin2Host.hpp"
#include "core/modules/finalImplementations.hpp"
#include "core/modules/moduleDSPAndGUI.hpp"
#include "core/spectrumWorxCore.hpp"
#include "core/threading/publish.hpp"

#include "gui/gui.hpp" // warningMessageBox()

#include "le/parameters/parametersUtilities.hpp"
#include "le/spectrumworx/presetFile.hpp"

#include "le/utility/assert.hpp"

#include <string_view>
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
    /// Null when no window is open, which is the normal case for session state.
    SpectrumWorxEditor *pEditor;
    /// The preset browser's "ignore external samples" box.
    bool ignoreSampleFile;

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \brief Whether this pass is filling the main thread's Program rather than
    /// the engine's.
    ///
    /// \note Which is exactly what `onlySetParameters()` already meant, and the
    /// reason that branch is back rather than replaced. It was written for VST
    /// 2.4's 128 programs -- "this program is not live, so move the numbers and
    /// leave the engine alone" -- and the port retired it as a case that could no
    /// longer arise. It arises again for a different reason: there are two copies
    /// of one program now, and the main thread's is filled by a pass that
    /// reconfigures nothing, publishes nothing and loads no sample.
    ///                                       (06.08.2026.) (SW port)
    ///
    ////////////////////////////////////////////////////////////////////////////
    bool mainThreadCopy;

    /// The chain reaches for this typedef; see presets.hpp's loadPreset().
    using Module = SpectrumWorxCore::Module;

    Program &program() const { return mainThreadCopy ? host.programMain() : host.core().program(); }
    GlobalParameters::Parameters &targetGlobalParameters() const { return program().parameters(); }
    AutomatedModuleChain &targetChain() const { return program().moduleChain(); }

    Host2PluginInteropControler::AutomationBlocker automationBlocker() const
    {
        return {host.core()};
    }

    /// \note The one point at which a preset load touches the engine; everything
    /// before it built a chain nothing else could see. See
    /// Threading::publishChain() and doc/tech/threading_model.md §5.
    void publishChain(AutomatedModuleChain &newChain) const
    {
        Threading::publishChain(host.core(), host.toEngine(), newChain);
    }

    /// \note The DSP half alone. An `EditorModuleInitialiser` stood here, so
    /// that a preset which filled slots built their strips as it went; the rack
    /// follows the chain now, and `SpectrumWorxEditor::resyncModuleRack()` is
    /// what builds them once the chain is installed.
    SpectrumWorxCore::ModuleInitialiser moduleInitialiser() const
    {
        return host.core().moduleInitialiser();
    }

    bool onlySetParameters() const { return mainThreadCopy; }

    ////////////////////////////////////////////////////////////////////////////
    // The external audio file the preset names, if it names one.
    ////////////////////////////////////////////////////////////////////////////

    bool wantsSampleFile() const { return !ignoreSampleFile && !onlySetParameters(); }

    void setSample(std::string_view const sampleFileName) const
    {
        if (sampleFileName.empty())
            return;

        // Implementation note:
        //   Workaround for relative sample paths and Windows paths on OS X.
        //                                    (17.11.2011.) (Domagoj Saric)
        /// \note And the reason a factory sample is stored by bare name: that
        /// is the one spelling no separator can spoil, and Sample::load()
        /// resolves it against the embedded set when there is nothing on disk.
        auto const path(
            juce::String::fromUTF8(sampleFileName.data(), static_cast<int>(sampleFileName.size()))
#ifdef _WIN32
                .replaceCharacter('/', '\\')
#else
                .replaceCharacter('\\', '/')
#endif // _WIN32
        );

        host.setNewSample(juce::File::createFileWithoutCheckingPath(path));
        if (pEditor)
            pEditor->updateSampleNameAsync();
    }

    bool setNewGlobalParameters(GlobalParameters::Parameters const &newParameters) const
    {
        LE::Parameters::forEach(newParameters, GlobalParameterUpdater{host.core()});
        return true;
    }

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \note A `TempoSyncedLFOWithoutTempo` report stood here, raised when a
    /// preset used tempo-synced LFOs and the host had never said what the tempo
    /// was. It is gone, and so is the problem kind, because there is no problem:
    /// a host that reports no transport gets 120 BPM in four four, which is a
    /// defined answer and a common one -- the standalone is such a host, so every
    /// preset with a synced LFO opened there raised it.
    ///
    ///   The 2016 sources had already met it from the other side: `Timer::reset()`
    /// deliberately does not clear `hasTempoInformation_` because doing so
    /// produced "bogus sporadic" versions of this dialog while browsing presets
    /// in Live. A warning that needs a workaround to stop firing at the wrong
    /// time is describing something that is not wrong.
    ///
    ///   What the interface does say is better placed and stays: with no tempo
    /// the LFO panel greys its N/T/D buttons and prints periods in milliseconds
    /// rather than note ratios, so the state is visible where it matters without
    /// interrupting anybody. See pluginTests.cpp's "With no transport the LFO
    /// clock is 120 BPM in four four", which pins the assumption this rests on.
    ///                                       (02.08.2026.) (SW port)
    ///
    ////////////////////////////////////////////////////////////////////////////
    void moduleChainFinished(std::uint8_t const moduleCount, bool /*syncedLFOFound*/) const
    {
        if (pEditor)
            pEditor->setLastModulePosition(moduleCount);
    }
}; // struct Loader

struct Consumer
{
    EditorHost &host;
    SpectrumWorxEditor *pEditor;
    /// \see Loader::mainThreadCopy
    bool mainThreadCopy;

    using Module = Loader::Module;

    Loader presetLoader(bool const ignoreExternalSample) const
    {
        return {host, pEditor, ignoreExternalSample, mainThreadCopy};
    }

    Program &program() const { return mainThreadCopy ? host.programMain() : host.core().program(); }

    /// \note Silent on the main-thread pass: the host is told once, about the
    /// load as a whole, by the pass that reaches the engine.
    void notifyHostAboutPresetChangeBegin() const
    {
        if (!mainThreadCopy)
            host.automation().presetChangeBegin();
    }
    void notifyHostAboutPresetChangeEnd() const
    {
        if (!mainThreadCopy)
            host.automation().presetChangeEnd();
    }
}; // struct Consumer

//------------------------------------------------------------------------------
} // anonymous namespace
//------------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
///
/// \brief One dialog for a whole load, or none.
///
/// \note The preset layer counts now rather than raising a message box per
/// problem -- see PresetLoadReport. This is the caller that turns the count back
/// into something a user sees, and it is deliberately *this* caller: a user opened
/// a preset and is owed an answer. `SpectrumWorxCLAP::stateLoad` takes the same
/// report and says nothing, a session restore being nobody's business.
///
/// \note "Owed an answer" is not "owed a count". A missing parameter on its own
/// says nothing to a user: the effect grew that parameter after the preset was
/// written and the value defaulted, which is the format's forward compatibility
/// doing its job. 104 of the 303 shipped banks raise one, and it is the only kind
/// any of them raises -- so this used to interrupt one factory preset in three
/// with a number nobody could act on. See PresetLoadReport::worthTellingTheUser()
/// and presetReportTests.cpp, which is what actually watches that total.
///
///   It is still *mentioned* when something else has gone wrong, because "two
/// effects are missing and so are forty parameters" is a fuller account of the
/// same event.
///                                           (02.08.2026.) (SW port)
///
////////////////////////////////////////////////////////////////////////////////

void reportToTheUser(PresetLoadReport const &report)
{
    if (!report.worthTellingTheUser())
        return;

    juce::String message;
    if (report.failures)
        message << "The preset could not be read.\n";
    if (report.unknownEffects)
        message << static_cast<int>(report.unknownEffects)
                << " effect(s) in it are not in this build";
    if (report.unavailableEffects)
        message << static_cast<int>(report.unavailableEffects)
                << " effect(s) in it are not in this edition";
    if (report.unknownEffects || report.unavailableEffects)
        message << " (" << juce::String(report.firstDetail) << " and so on).\n";
    /// \note The one parameter case that is the user's business: the file carries
    /// a value and nothing in this build knows where to put it, so the preset
    /// will not sound the way it was saved. Named, because a name is the only
    /// thing that makes it reportable.
    if (report.unknownParameters)
        message << static_cast<int>(report.unknownParameters)
                << " value(s) in it could not be applied (" << juce::String(report.firstDetail)
                << ").\n";
    if (report.missingParameters)
        message << static_cast<int>(report.missingParameters)
                << " parameter(s) it does not mention were left at their defaults.\n";

    GUI::warningMessageBox(MB_WARNING, message.toRawUTF8(), false);
}

bool loadPreset(EditorHost &host, SpectrumWorxEditor *const pEditor, char *const inMemoryPreset,
                bool const ignoreExternalSample, juce::String *const comment,
                char const *const presetName, DawExtraState const *const pDawExtraState)
{
    Consumer const consumer{host, pEditor, false};

    // Whatever a previous load left uncollected is not this load's.
    takePresetLoadReport();

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \brief The main thread's copy first, in a pass of its own.
    ///
    /// \note Two passes over one file rather than one pass and a clone, because
    /// the parser is the only thing that knows how to turn a preset into a
    /// chain: cloning would have to copy every module's parameters through the
    /// automated interface, which cannot reach the two an LFO does not export
    /// and quantises everything it can reach. A second parse is exact by
    /// construction and costs a parse of a file that was just read off disk.
    ///
    ///   `ParametersLoader` cannot be rewound -- `loadModuleChain()` asserts it
    /// has not already switched to module parameters -- so each pass builds its
    /// own over the same buffer.
    ///
    /// \note This one's report is dropped. It sees exactly the problems the pass
    /// below sees, and counting them twice would double every number a user is
    /// shown; `presetReportTests.cpp` is what watches those totals.
    ///                                       (06.08.2026.) (SW port)
    ///
    ////////////////////////////////////////////////////////////////////////////
    {
        Consumer const mainThreadCopy{host, pEditor, true};
        SW::loadPreset(inMemoryPreset, true /*a sample is the engine's*/, nullptr, mainThreadCopy,
                       pDawExtraState);
        takePresetLoadReport();
    }

    /// \note The format layer speaks `std::string` -- it is below JUCE now, and
    /// a preset's comment is UTF-8 bytes whatever the interface's string type is.
    std::string commentBytes;
    consumer.notifyHostAboutPresetChangeBegin();
    bool const succeeded(SW::loadPreset(inMemoryPreset, ignoreExternalSample,
                                        comment ? &commentBytes : nullptr, consumer,
                                        pDawExtraState));
    if (comment)
        *comment =
            juce::String::fromUTF8(commentBytes.data(), static_cast<int>(commentBytes.size()));
    if (succeeded)
        copyPresetName(presetName, consumer.program().name());
    consumer.notifyHostAboutPresetChangeEnd();

    /// \note Only with a window open. With none this is a session being restored,
    /// which nobody asked for and which may not even have a message thread yet.
    if (pEditor)
        reportToTheUser(takePresetLoadReport());

    return succeeded;
}

bool loadPreset(EditorHost &host, SpectrumWorxEditor *const pEditor, juce::File const &presetFile,
                bool const ignoreExternalSample, juce::String *const comment,
                char const *const presetName, DawExtraState const *const pDawExtraState)
{
    auto const presetData(readPresetFile(presetFile));
    if (!presetData)
        return false;
    return loadPreset(host, pEditor, presetData.get(), ignoreExternalSample, comment, presetName,
                      pDawExtraState);
}

//------------------------------------------------------------------------------
} // namespace GUI
//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
