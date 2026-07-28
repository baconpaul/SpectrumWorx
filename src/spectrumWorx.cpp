////////////////////////////////////////////////////////////////////////////////
///
/// spectrumWorx.cpp
/// ----------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "spectrumWorx.hpp"

#include "core/modules/moduleDSPAndGUI.hpp"
#include "gui/gui.hpp"

#include "le/math/math.hpp"
#include "le/math/vector.hpp"
#include "le/parameters/fusionAdaptors.hpp"
#include "le/parameters/uiElements.hpp"
#include "le/spectrumworx/presets.hpp"
#include "le/utility/parentFromMember.hpp"

// Boost sandbox
#include "boost/mmap/mappble_objects/file/utility.hpp"

#include "le/utility/stackBuffer.hpp"

#include "le/utility/assert.hpp"
#include "le/utility/ignoreUnused.hpp"
#include <boost/fusion/algorithm/iteration/for_each.hpp>

#ifdef __GNUC__
#include <cstdlib>
#include <iconv.h>
#endif

#include <algorithm>
#include <string_view>
#include "le/utility/span.hpp"
//------------------------------------------------------------------------------
#ifdef __APPLE__
extern void const *swDLLAddress;
#endif // __APPLE__
#ifdef _WIN32
extern "C" IMAGE_DOS_HEADER __ImageBase;
static void const *const swDLLAddress(&__ImageBase);
#endif // _WIN32
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace SW
{
//------------------------------------------------------------------------------

SpectrumWorx::SpectrumWorx(bool const runningAsAU)
    :
#ifndef LE_SW_DISABLE_SIDE_CHANNEL
      pListenerToNotifyWhenSampleLoaded_(nullptr),
#endif // LE_SW_DISABLE_SIDE_CHANNEL
      currentProgram_(0),
#if LE_SW_ENGINE_INPUT_MODE >= 2
      inputModeToSetOnRestart_(static_cast<InputMode::value_type>(-1)),
#endif // LE_SW_ENGINE_INPUT_MODE >= 2
      loadLastSessionOnStartup_(false)
#ifdef __APPLE__
      ,
      runningAsAU_(runningAsAU)
#endif // __APPLE__
{
#ifndef __APPLE__
    LE_ASSUME(runningAsAU == false);
#endif // __APPLE__

    for (auto &program : programs())
        std::strcpy(&program.name()[0], "Empty");

    SpectrumWorxCore::setProgram(programs()[getProgram()]);
}

SpectrumWorx::~SpectrumWorx()
{
    //...mrmlj...rethink this...
    if (GUI::havePathsBeenInitialised())
    {
#if LE_SW_ENGINE_INPUT_MODE >= 2
        if (inputModeToSetOnRestart_ != static_cast<InputMode::value_type>(-1))
            parameters().set<InputMode>(inputModeToSetOnRestart_);
#endif // LE_SW_ENGINE_INPUT_MODE >= 2

        try
        {
            savePreset(lastSessionPresetFile().getFullPathName(), currentSampleFile(),
                       juce::String(), program());
        }
        catch (...)
        {
        }

        saveSettings();
    }

    LE_TRACE_IF(gui(), "\tSW: host destroyed the plugin w/o closing the GUI.");

#ifndef LE_SW_DISABLE_SIDE_CHANNEL
    LE_ASSERT(!pListenerToNotifyWhenSampleLoaded_);
#endif // LE_SW_DISABLE_SIDE_CHANNEL
}

////////////////////////////////////////////////////////////////////////////////
//
// SpectrumWorx::process()
// -----------------------
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
float const *LE_RESTRICT getChannelDataChunk(Sample::ChannelData const &channelData,
                                             std::uint32_t &startingPosition,
                                             std::uint16_t chunkSize,
                                             float *LE_RESTRICT const workBuffer)
{
    auto const dataSize(static_cast<std::uint32_t>(channelData.size()));
    LE_ASSERT(startingPosition <= dataSize);
    if (dataSize > (startingPosition + chunkSize))
    {
        float const *const pChunk(&channelData[startingPosition]);
        startingPosition += chunkSize;
        return pChunk;
    }
    else
    {
        float *workBufferPosition(workBuffer);
        while (chunkSize)
        {
            if (startingPosition == dataSize)
                startingPosition = 0;
            auto const channelDataPosition(&channelData[startingPosition]);
            auto const amountToCopy(static_cast<std::uint16_t>(
                std::min<std::uint32_t>(dataSize - startingPosition, chunkSize)));
            Math::copy(channelDataPosition, workBufferPosition, amountToCopy);
            workBufferPosition += amountToCopy;
            startingPosition += amountToCopy;
            chunkSize -= amountToCopy;
        }
        return workBuffer;
    }
}
} // anonymous namespace

#pragma warning(push)
#pragma warning(disable : 4701) // Potentially uninitialized local variable 'samplePosition' used.

void SpectrumWorx::process /// \throws nothing
    (float const *const *const inputs, float **const outputs, std::uint32_t const samples)
{
    // Implementation note:
    //   A normal lock cannot be used here because that could cause the GUI
    // thread to block the processing thread (e.g. while processing is active a
    // user changes the preset, which requires locking the process critical
    // section, that in turn, for whatever reason, causes a message box to pop
    // up while the lock is still held) and this seems to freeze certain hosts
    // (e.g. Reaper 3.22). Wavelab 5, VST Scanner and SoundForge 9.0 were found
    // not to suffer from this problem.
    //                                        (05.02.2010.) (Domagoj Saric)
    if (!processCriticalSection_.try_lock())
        return;
    ProcessLockUnlocker const processingLockUnlocker(*this);

    Math::FPUDisableDenormalsGuard const disableDenormals;

    // We give higher priority to external samples loaded through SW rather than
    // side channel data provided by the host:
    float const *const *pSideChannels;
    if (hasExternalSample())
    {
        auto const numberOfExternalAudioChannels(
            std::min<uint8_t>(engineSetup().numberOfChannels(), 2U));
        /// \note External samples currently force-load as stereo always so we
        /// must allow buffers().numberOfSideChannels() to be larger than
        /// numberOfExternalAudioChannels (stereo > mono).
        ///                                   (20.03.2013.) (Domagoj Saric)
        LE_ASSERT(numberOfExternalAudioChannels <= buffers().numberOfSideChannels());
        LE_STACK_BUFFER(sideChannels, float const *, numberOfExternalAudioChannels);
        std::uint32_t samplePosition;
        for (std::uint8_t channel(0); channel < numberOfExternalAudioChannels; ++channel)
        {
            samplePosition = sample_.samplePosition();
            sideChannels[channel] =
                getChannelDataChunk(sample_.channel(channel), samplePosition, samples,
                                    buffers().sideChannel(channel).begin());
        }
        pSideChannels = &sideChannels[0];
        /// \todo Think of a smarter solution.
        ///                                   (08.02.2010.) (Domagoj Saric)
        LE_ASSERT(samplePosition != sample_.samplePosition());
        sample_.samplePosition() = samplePosition;
    }
    else if (engineSetup().hasSideChannel())
    {
        pSideChannels = &inputs[engineSetup().numberOfChannels()];
        LE_ASSERT(*pSideChannels);
    }
    else
    {
        pSideChannels = nullptr;
    }

    static float const outputGainScale(1);
    SpectrumWorxCore::process(inputs, pSideChannels, outputs, outputGainScale, samples);
}

#pragma warning(pop)

void SpectrumWorx::resume()
{
    // Implementation note:
    //   Tracktion seems to call this on the GUI thread (for example when
    // switching/loading presets) while the processing (thread) still has not
    // stopped which breaks our code (because the channel buffers get reset
    // while processing is still active). Because of this we need to hold a
    // process lock here.
    //                                        (24.06.2010.) (Domagoj Saric)
    Utility::CriticalSectionLock const processLock(getProcessingLock());
    sample_.restart();
    SpectrumWorxCore::resume();
}

bool SpectrumWorx::setNumberOfChannelsFromHost(std::uint8_t const numberOfInputChannels,
                                               std::uint8_t const numberOfOutputChannels)
{
    auto const changeSuccess(
        SpectrumWorxCore::setNumberOfChannels(numberOfInputChannels, numberOfOutputChannels));
    if (changeSuccess == IOChangeResult::Succeeded)
    {
        // Implementation note:
        // Changing the input mode from a side-channel mode to a
        // non-side-channel mode has the same effect as unloading the sample so
        // side channel data must also be cleared (the side channel data must
        // actually be cleared only if we are changing from a side-channel to a
        // non-side-channel mode and a sample is not loaded, as samples override
        // external side-chains).
        //                                    (14.01.2010.) (Domagoj Saric)
        clearSideChannelDataIfNoSideChannel();
#if LE_SW_ENGINE_INPUT_MODE >= 1
        updateGUIForGlobalParameterChange();
#endif // LE_SW_ENGINE_INPUT_MODE >= 2
    }
    return changeSuccess != IOChangeResult::Failed;
}

#if LE_SW_ENGINE_INPUT_MODE >= 2
bool SpectrumWorx::setNumberOfChannelsFromUser(std::uint8_t const numberOfInputChannels,
                                               std::uint8_t const numberOfOutputChannels)
{
    LE_ASSERT(checkChannelConfiguration(numberOfInputChannels, numberOfOutputChannels));

    std::uint8_t const currentNumberOfMainChannels(uncheckedEngineSetup().numberOfChannels());
    std::uint8_t const currentNumberOfSideChannels(uncheckedEngineSetup().numberOfSideChannels());

    std::uint8_t const numberOfMainChannels(numberOfOutputChannels);
    std::uint8_t const numberOfSideChannels(numberOfInputChannels - numberOfOutputChannels);

    if ((numberOfMainChannels == currentNumberOfMainChannels) &&
        (numberOfSideChannels == currentNumberOfSideChannels))
        return true;

    /// \note The process lock must be held during both the
    /// setNumberOfChannelsImpl() and hostTryIOConfigurationChange() calls,
    /// otherwise a process() call might be issued in between the
    /// setNumberOfChannelsImpl() and hostTryIOConfigurationChange() calls
    /// (which could lead to a crash because the host would send data for the
    /// previous IO setup).
    ///                                       (04.03.2013.) (Domagoj Saric)
    Utility::CriticalSectionLock const processLock(this->getProcessingLock());

    setReportedNumberOfChannels(numberOfMainChannels, numberOfSideChannels);
    bool const hostAllows(hostTryIOConfigurationChange(numberOfMainChannels, numberOfSideChannels));
    setReportedNumberOfChannels(currentNumberOfMainChannels, currentNumberOfSideChannels);
    if (!hostAllows)
    {
        bool const hostSupportsIOChanges(hostSupportsIOConfigurationChanges());
        if (gui())
        {
            std::string_view errorMessage;
            if (hostSupportsIOChanges)
            {
                errorMessage = "The host rejected the requested IO mode change.";
            }
            else
            {
                if (runningAsAU())
                {
                    errorMessage = "A preset tried to change the IO mode but this is something "
                                   "not generally supported by the AU protocol. Please adjust "
                                   "the bus configuration manually through the host as "
                                   "required (or use the VST version).";
                }
                else
                {
                    errorMessage = "An attempt was made to change the current input-output mode "
                                   "but this host does not report that it supports on-the-fly "
                                   "channel configuration changes. To avoid crashing it, the "
                                   "change will be made the next time the plugin is started.";
                    //...mrmlj...
                    InputMode const currentIOMode(parameters().get<InputMode>());
                    updateInputModeForIOConfig(numberOfMainChannels, numberOfSideChannels);
                    setInputModeToSetOnRestart(parameters().get<InputMode>());
                    parameters().set<InputMode>(currentIOMode);
                }
                LE_ASSERT(engineSetup().numberOfChannels() == currentNumberOfMainChannels);
                LE_ASSERT(engineSetup().numberOfSideChannels() == currentNumberOfSideChannels);
            }
            GUI::warningMessageBox(MB_ERROR, errorMessage, false);
            return false;
        }
        else
        {
            /// \note If the host is "ioChanged() agnostic" and we have no GUI
            /// to inform the user of an unsuccessful change, allow the change
            /// if it is decreasing the number of channels (this should be safe
            /// crash-wise).
            ///                               (19.03.2013.) (Domagoj Saric)
            if (hostSupportsIOChanges || /*...mrmlj...*/ runningAsAU())
                return false;
            else if ((numberOfMainChannels > currentNumberOfMainChannels) ||
                     (numberOfSideChannels > currentNumberOfSideChannels))
                return false;

            LE_TRACE("\tSW: blindly accepting a 'downsized' IO mode change.");
        }
    }

    bool const changeSuccessful(
        SpectrumWorxCore::setNumberOfChannelsImpl(numberOfMainChannels, numberOfSideChannels));
    if (!changeSuccessful)
    {
        LE_VERIFY(hostTryIOConfigurationChange(currentNumberOfMainChannels,
                                               currentNumberOfSideChannels) ||
                  !hostSupportsIOConfigurationChanges());
        return false;
    }

    // Implementation note:
    //   Changing the input mode from a side-channel mode to a non-side-channel
    // mode has the same effect as unloading the sample so side channel data
    // must also be cleared (the side channel data must actually be cleared only
    // if we are changing from a side-channel to a non-side-channel mode and a
    // sample is not loaded, as samples override external side-chains).
    //                                        (14.01.2010.) (Domagoj Saric)
    clearSideChannelDataIfNoSideChannel();
    return true;
}
#endif // LE_SW_ENGINE_INPUT_MODE >= 2

////////////////////////////////////////////////////////////////////////////////
///
/// Programs and presets
///
////////////////////////////////////////////////////////////////////////////////

// VST Preset Program Change Suggestions
// http://forum.cockos.com/showthread.php?p=384102
bool SpectrumWorx::loadProgramState(std::uint8_t const programIndex, char const *const pProgramName,
                                    void const *const pData, std::uint32_t const dataSize)
{
    /// \note We have to copy the state data because of RapidXML's destructive
    /// parsing.
    ///                                       (18.03.2013.) (Domagoj Saric)
    LE_ALIGNED_SCOPED_STACK_BUFFER(preset, char, dataSize);
    std::memcpy(preset.begin(), pData, dataSize);
    if (!loadPreset(preset.begin(), false, nullptr, programIndex))
        return false;
    setProgramName(programIndex, pProgramName);
    return true;
}

unsigned int SpectrumWorx::saveProgramState(std::uint8_t const programIndex, void *const pStorage,
                                            std::uint32_t const storageSize) const
{
    unsigned int const bytesWritten(savePreset(static_cast<char *>(pStorage), currentSampleFile(),
                                               juce::String(), programs()[programIndex]));
    LE_ASSERT(bytesWritten < storageSize);
    LE::Utility::ignoreUnused(storageSize);
    return bytesWritten;
}

void SpectrumWorx::getProgramName(std::uint8_t const program,
                                  LE::Utility::Span<char> const name) const
{
    copyToBuffer(&programs()[program].name()[0], name);
}
void SpectrumWorx::getProgramName(LE::Utility::Span<char> const name) const
{
    getProgramName(getProgram(), name);
}

void SpectrumWorx::setProgramName(std::uint8_t const program, char const *const programName)
{
    copyToBuffer(programName, programs()[program].name());
}
void SpectrumWorx::setProgramName(char const *const programName)
{
    setProgramName(getProgram(), programName);
}

namespace
{
class GlobalParameterUpdater : public SpectrumWorx
{
  public:
    using result_type = void;

    template <class Parameter> result_type operator()(Parameter const &parameter) const
    {
        LE_VERIFY((setGlobalParameter<Parameter, SpectrumWorx>(
            const_cast<GlobalParameterUpdater &>(*this), parameter.getValue())));
    }

#if LE_SW_ENGINE_INPUT_MODE >= 2
    using InputMode = GlobalParameters::InputMode;
    result_type operator()(InputMode const &inputMode) const
    {
        /// \note Changing the actual number of channels based on the input
        /// mode saved in the preset/program makes no sense (we can't/don't want
        /// to actually modify the format of the track into which SW is
        /// inserted). Instead we only update the side channel mode.
        ///                                   (18.03.2013.) (Domagoj Saric)
        bool enableSideChannel;
        switch (inputMode.getValue())
        {
        case InputMode::MonoSideChain:
        case InputMode::StereoSideChain:
            enableSideChannel = true;
            break;
        default:
            enableSideChannel = false;
            break;
        }

        SpectrumWorx &effect(const_cast<GlobalParameterUpdater &>(*this));
        InputMode::value_type const currentInputMode(effect.parameters().get<InputMode>());
        InputMode::value_type newInputMode;

        if (enableSideChannel)
        {
#ifdef LE_SW_DISABLE_SIDE_CHANNEL
            GUI::warningMessageBox(MB_WARNING,
                                   "Loaded preset uses side channel audio which is not "
                                   "supported by this edition of SpectrumWorx.",
                                   false);
            return /*false*/;
#else
            switch (currentInputMode)
            {
            case InputMode::Mono:
                newInputMode = InputMode::MonoSideChain;
                break;
            case InputMode::Stereo:
                newInputMode = InputMode::StereoSideChain;
                break;
            default:
                newInputMode = inputMode;
                break;
            }
#endif // LE_SW_DISABLE_SIDE_CHANNEL
        }
        else
        {
            switch (currentInputMode)
            {
            case InputMode::MonoSideChain:
                newInputMode = InputMode::Mono;
                break;
            case InputMode::StereoSideChain:
                newInputMode = InputMode::Stereo;
                break;
            default:
                newInputMode = inputMode;
                break;
            }
        }
        LE_TRACE_IF(newInputMode != inputMode, "Rejecting exact InputMode from preset");
        LE_VERIFY(setGlobalParameter<InputMode>(effect, newInputMode) || runningAsAU());
    }
#endif // LE_SW_ENGINE_INPUT_MODE >= 2

  private:
    GlobalParameterUpdater();
    void operator=(GlobalParameterUpdater const &);
}; // class GlobalParameterUpdater
} // anonymous namespace

void SpectrumWorx::resetForGlobalParameters(Parameters const &parameters)
{
    //...mrmlj...this can possibly cause multiple engine setup updates/memory reallocations...
    //...mrmlj...no error reporting...
    boost::fusion::for_each(parameters, static_cast<GlobalParameterUpdater &>(*this));
}

bool SpectrumWorx::canParameterBeAutomated(ParameterID const parameter,
                                           Program const *LE_RESTRICT const pProgram) const
{
    bool const staticParameterList(pProgram == nullptr);
    if (staticParameterList)
        return true;

    switch (parameter.type())
    {
    case ParameterID::GlobalParameter:
    case ParameterID::ModuleChainParameter:
        return true;
    }

    auto indices(parameter.value._.lfo);
    switch (parameter.type())
    {
    case ParameterID::LFOParameter:
        ++indices.moduleParameterIndex; // Bypass
    case ParameterID::ModuleParameter:
    {
        auto const pModule(pProgram->moduleChain().module(indices.moduleIndex));
        if (!pModule)
            return false;
        if (indices.moduleParameterIndex >= pModule->numberOfParameters())
            return false;
        return true;
    }

        LE_DEFAULT_CASE_UNREACHABLE();
    }
}

bool SpectrumWorx::ModuleInitialiser::operator()(Module &module,
                                                 std::uint8_t const moduleIndex) const
{
    if (dspInitialiser(module, moduleIndex))
    {
        if (pEditor)
        {
            if (module.gui())
                module.gui()->moveToSlot(moduleIndex);
            else
                module.createGUI(*pEditor, moduleIndex);
        }
        return true;
    }
    return false;
}

SpectrumWorx::ModuleInitialiser SpectrumWorx::moduleInitialiser()
{
    return {SpectrumWorxCore::moduleInitialiser(), gui().operator->()};
}

#pragma warning(push)
#pragma warning(disable : 4510) // Default constructor could not be generated.
#pragma warning(disable                                                                            \
                : 4610) // Class can never be instantiated - user-defined constructor required.

struct SpectrumWorx::PresetLoader
{
    SpectrumWorx &effect;
    std::uint8_t const targetProgram;

    using Module = ModuleInitialiser::Module;

    Program &program() { return effect.programs()[targetProgram]; }
    GlobalParameters::Parameters &targetGlobalParameters() { return program().parameters(); }
    AutomatedModuleChain &targetChain() { return program().moduleChain(); }

    AutomationBlocker automationBlocker() const { return {effect}; }
    Utility::CriticalSectionLock processingLock() const { return effect.getProcessingLock(); }
    ModuleInitialiser moduleInitialiser() { return effect.moduleInitialiser(); }

    bool onlySetParameters() const { return targetProgram != effect.getProgram(); }
    bool setNewGlobalParameters(GlobalParameters::Parameters const &newParameters)
    {
        effect.resetForGlobalParameters(newParameters);
        return true;
    }

    void moduleChainFinished(std::uint8_t const moduleCount, bool const syncedLFOFound)
    {
        LE_ASSERT(!onlySetParameters());
        if (effect.gui())
            effect.gui()->setLastModulePosition(moduleCount);
        if (syncedLFOFound && !LFO::Timer::hasTempoInformation() &&
            /// \note Some hosts (Renoise 2.8) provide tempo information
            /// lazily, after the first time they call process() so we also
            /// check if the transport has started in order to avoid false
            /// warnings.
            ///                               (03.07.2012.) (Domagoj Saric)
            effect.lfoTimer().currentTimeInBars())
        {
            GUI::warningMessageBox(MB_WARNING,
                                   "Loaded preset uses tempo-synced LFOs but the host does not "
                                   "provide tempo information.",
                                   false);
        }
    }

#ifndef LE_SW_DISABLE_SIDE_CHANNEL
    bool wantsSampleFile() const { return !ignoreSampleFile && !onlySetParameters(); }
    void setSample(std::string_view const sampleFileName)
    {
        LE_ASSERT(!onlySetParameters());
        LE_ASSERT(!ignoreSampleFile);
        //   The sample has to be loaded before calling
        // gui()->updateSampleNameAsync().
        //                                    (15.12.2011.) (Domagoj Saric)
        /// \todo  Clean up this spaghetti.
        ///                                   (15.12.2011.) (Domagoj Saric)
        if (sampleFileName.empty())
            return;
        effect.setNewSample(
            // Implementation note:
            //   Workaround for relative sample paths and Windows paths
            // on OS X.
            //                                (17.11.2011.) (Domagoj Saric)
            juce::File::createFileWithoutCheckingPath(
                juce::String::fromUTF8(sampleFileName.begin(),
                                       static_cast<unsigned int>(sampleFileName.size()))
#ifdef _WIN32
                    .replaceCharacter('/', '\\')
#else
                    .replaceCharacter('\\', '/')
#endif // __APPLE__
                    ));
        /// \todo Think of a cleaner solution.
        ///                                   (03.02.2010.) (Domagoj Saric)
        if (effect.gui())
            effect.gui()->updateSampleNameAsync();
    }
    bool const ignoreSampleFile;
#endif // LE_SW_DISABLE_SIDE_CHANNEL
}; // struct SpectrumWorx::PresetLoader

struct SpectrumWorx::PresetConsumer
{
    using Module = PresetLoader::Module;

    PresetLoader presetLoader(bool const ignoreExternalSample) const
    {
        return {effect, targetProgram, ignoreExternalSample};
    }

    Program &program() { return effect.programs()[targetProgram]; }
    void notifyHostAboutPresetChangeBegin() const
    {
        LE_ASSERT(targetProgram == effect.getProgram());
        effect.presetChangeBegin();
    }
    void notifyHostAboutPresetChangeEnd() const
    {
        LE_ASSERT(targetProgram == effect.getProgram());
        effect.presetChangeEnd();
    }
    SpectrumWorx &effect;
    std::uint8_t const targetProgram;
}; // struct SpectrumWorx::PresetConsumer

#pragma warning(push)

bool SpectrumWorx::loadPreset(char *const inMemoryPreset, bool const ignoreExternalSample,
                              juce::String *const pComment, std::uint8_t const program)
{
    LE_ASSERT(!presetLoadingInProgress());

    return SW::loadPreset(inMemoryPreset, ignoreExternalSample, pComment,
                          PresetConsumer{*this, program});
}

bool SpectrumWorx::loadPreset(juce::File const &file, bool const ignoreExternalSample,
                              juce::String *const pComment, char_t const *const presetName)
{
    return SW::loadPreset(file, ignoreExternalSample, pComment, presetName,
                          PresetConsumer{*this, getProgram()});
}

#ifndef LE_SW_DISABLE_SIDE_CHANNEL
bool SpectrumWorx::setNewSampleWorker(juce::File const &newSampleFile)
{
    bool succeeded(true);
    if (newSampleFile.existsAsFile())
    {
#ifndef NDEBUG
        bool const samplePreviouslyLoaded(sample_);
        juce::File const previousSample(sample_.sampleFile());
#endif // NDEBUG

        //...mrmlj...
        bool bufferAllocationSucceeded;
        {
            Utility::CriticalSectionLock const processingLock(getProcessingLock());
            buffers().forceSideChannel(true);
            bufferAllocationSucceeded =
                buffers().resize(buffers().blockSize(), engineSetup().numberOfChannels(),
                                 engineSetup().numberOfSideChannels());
        }

        if (bufferAllocationSucceeded)
        {
            char const *const pErrorMessage(sample_.load(
                newSampleFile, engineSetup().sampleRate<unsigned int>(), processCriticalSection_));
            if (pErrorMessage)
            {
                GUI::warningMessageBox("SpectrumWorx: error loading selected sample file.",
                                       pErrorMessage, true);

                // Implementation note:
                //   Verify that the 'sample loaded' state has not changed if an
                // error occurred.
                //                                (08.07.2010.) (Domagoj Saric)
                LE_ASSERT(!samplePreviouslyLoaded == !sample_);
                LE_ASSERT(previousSample == sample_.sampleFile());
                succeeded = false;
            }
        }
        buffers().forceSideChannel(hasExternalSample());
    }
    else
    {
        Utility::CriticalSectionLock const processingLock(getProcessingLock());
        sample_.clear();
        // Implementation note:
        //   Because of hosts like FL Studio 9 that disallow the changing of the
        // input mode which then possibly gets locked to 4in2out and which then
        // send null pointers for channels above stereo we have to clear the
        // side channel data here whenever a sample is unloaded, even if the
        // InputMode is set to a side channel mode, because we might get null
        // pointers for the 'side channels' which the rest of the code will
        // interpret as no-side-channel data and will then check that the side
        // channel data was properly cleared.
        //                                    (15.06.2010.) (Domagoj Saric)
        //...mrmlj...reinvestigate whether the assumption that the 'rest of the
        //code' will properly handle null side channel pointers...
        //clearSideChannelDataIfNoSideChannel();
        clearSideChannelData();

        //...mrmlj...
        buffers().forceSideChannel(false);

        succeeded = (newSampleFile == juce::File());
    }
    return succeeded;
}

void SpectrumWorx::setNewSample(juce::File const &newSampleFile)
{
    // Implementation note:
    //   If the requested sample file does not exist we look for it in the
    // default samples folder (this is required to make our pre-installed
    // presets, that use the pre-installed samples, work even if the user
    // chooses a non-default installation folder).
    //                                        (12.07.2010.) (Domagoj Saric)
    pendingSampleToLoad_ =
        newSampleFile.exists()
            ? newSampleFile
            : GUI::rootPath().getChildFile("Samples").getChildFile(newSampleFile.getFileName());
    if (!isSampleLoadInProgress())
    {
        LE_VERIFY(
            (sampleLoadingThread_.start<SpectrumWorx, &SpectrumWorx::sampleLoadingLoop>(*this)));
        sampleLoadingThread_.setDebugName("Sample thread");
    }
}

bool SpectrumWorx::isSampleLoadInProgress() const { return sampleLoadingThread_.isRunning(); }

void SpectrumWorx::registerSampleLoadedListener(Editor &listenerToRegister)
{
    //...mrmlj...reconsider this...
    LE_ASSERT(!pListenerToNotifyWhenSampleLoaded_ ||
              (pListenerToNotifyWhenSampleLoaded_ == &listenerToRegister));
    pListenerToNotifyWhenSampleLoaded_ = &listenerToRegister;
}

void SpectrumWorx::deregisterSampleLoadedListener(Editor const &listenerToDeregister)
{
    LE_ASSERT(!pListenerToNotifyWhenSampleLoaded_ ||
              (pListenerToNotifyWhenSampleLoaded_ == &listenerToDeregister));
    LE::Utility::ignoreUnused(listenerToDeregister);
    pListenerToNotifyWhenSampleLoaded_ = nullptr;
}

void SpectrumWorx::sampleLoadingLoop()
{
    while ((pendingSampleToLoad_ != sample_.sampleFile()) &&
           setNewSampleWorker(pendingSampleToLoad_))
    {
    }

    pendingSampleToLoad_ = juce::File();

    if (pListenerToNotifyWhenSampleLoaded_)
    {
        GUI::postMessage(*this, [](GUI::SpectrumWorxEditor &gui) {
            gui.sampleArea_.setVisible();
            gui.updateSampleName();
            return true;
        });
    }
    pListenerToNotifyWhenSampleLoaded_ = nullptr;

    sampleLoadingThread_.markAsDone();
}
#endif // LE_SW_DISABLE_SIDE_CHANNEL

bool SpectrumWorx::updateEngineSetup()
{
    if (SpectrumWorxCore::updateEngineSetup())
    {
        updateGUIForEngineSetupChanges();
        return true;
    }
    return false;
}

void SpectrumWorx::updatePosition(std::uint32_t const deltaSamples)
{
    handleTimingInformationChange(updatePositionAndTimingInformation(deltaSamples));
}

bool SpectrumWorx::initialise()
{
    //if ( !SpectrumWorxCore::initialise() )
    //    return false;
    { //...mrmlj...copy pasted core version for different setNumberOfChannels
        //...mrmlj...and updateEngineSetup versions...clean this up...
        bool success;
        // Update/create the initial Engine::Setup and shared storage with the
        // default and/or so far partially set parameters.
        //...mrmlj...check if channel configuration has already been set up and skip
        //...mrmlj...the parameters-engine setup synchronization in that case
        //...mrmlj...a custom io mode might have been set and this would override it
        //...mrmlj...clean this up...
        if (!currentStorageFactors().numberOfChannels)
        {
#if LE_SW_ENGINE_INPUT_MODE >= 1
            auto const ioChannelsConfig(ioChannels(parameters().get<InputMode>()));
            //...mrmlj...see the note in SpectrumWorxVST24::initialise()...
            success =
                (SpectrumWorxCore::setNumberOfChannels(
                     ioChannelsConfig.first, ioChannelsConfig.second) != IOChangeResult::Failed);
            reinterpret_cast<Plugin2HostInteropControler &>(*this). //...ugh...mrmlj...
                hostTryIOConfigurationChange(
                    engineSetup().numberOfChannels(),
                    engineSetup().numberOfSideChannels()); //...ugh...mrmlj...
#else                                                      //...mrmlj...
            success = SpectrumWorxCore::setNumberOfChannels(1, 1);
#endif                                                     // LE_SW_ENGINE_INPUT_MODE
        }
        else
        {
            LE_ASSERT(currentStorageFactors().numberOfChannels == engineSetup().numberOfChannels());
            success = true;
        }
        //...mrmlj...AU...LE_ASSERT_MSG( !!buffers(), "Input buffers not initialised." );
        success &= updateEngineSetup();
        LE_ASSERT(success);
        if (!success)
            return false;
    }

    Math::rngSeed();

    loadSettings();

    /// \note Report the actual latency after initialisation. Without this the
    /// host would be left assuming the maximum latency reported in the VST2.4
    /// constructor.
    ///                                       (24.04.2013.) (Domagoj Saric)
    latencyChanged();

    return true;
}

////////////////////////////////////////////////////////////////////////////
// Settings
////////////////////////////////////////////////////////////////////////////

namespace
{
struct Settings : GUI::Theme::Settings
{
    Settings() : loadLastSessionOnStartup(false) {}
    Settings(GUI::Theme::Settings const &guiSettings, bool const loadLastSessionOnStartupParam)
        : GUI::Theme::Settings(guiSettings), loadLastSessionOnStartup(loadLastSessionOnStartupParam)
    {
    }

    bool loadLastSessionOnStartup;
}; // struct Settings
} // namespace

void SpectrumWorx::loadSettings()
{
    try
    {
        using namespace boost;

        mmap::basic_read_only_mapped_view const mappedSettingsFile(
            mmap::map_read_only_file(settingsFile().getFullPathName().getCharPointer()));
        //...mrmlj...rethink this assertion...
        //LE_ASSERT( ( mappedSettingsFile || !this->settingsFile().existsAsFile() ) && "Unable to open existing settings file." );
        if (mappedSettingsFile.size() != sizeof(Settings))
        {
            LE_TRACE("\tSW: unrecognized settings file.");
            return;
        }

        Settings const &settings(*reinterpret_cast<Settings const *>(mappedSettingsFile.begin()));
        GUI::Theme::settings() = settings;
        shouldLoadLastSessionOnStartup(settings.loadLastSessionOnStartup);
        if (shouldLoadLastSessionOnStartup())
        {
            juce::File const lastSessionFile(lastSessionPresetFile());
            if (lastSessionFile.existsAsFile())
            {
                //LE_VERIFY( loadPreset( lastSessionFile, false, nullptr, _T( "Last session" ) ) );
                //...mrmlj...avoid notifyHostAboutPresetChange()
                auto const pPresetData(Preset::loadIntoMemory(lastSessionFile));
                LE_VERIFY(pPresetData.get() &&
                          loadPreset(pPresetData.get(), false, nullptr, getProgram()));
                setProgramName("Last session");
            }
        }
    }
    catch (...)
    {
    }
}

void SpectrumWorx::saveSettings()
{
    using namespace boost;

    // Settings file

    mmap::basic_mapped_view const mappedSettingsFile(
        mmap::map_file(settingsFile().getFullPathName().getCharPointer(), sizeof(Settings)));
    LE_ASSERT_MSG(mappedSettingsFile, "Unable to create settings file.");
    if (mappedSettingsFile.empty())
    {
        GUI::warningMessageBox(MB_ERROR, "Failed to save settings.", false);
        return;
    }

    Settings &onDiskSettings(*reinterpret_cast<Settings *>(mappedSettingsFile.begin()));
    Settings const currentSettings(GUI::Theme::settings(), loadLastSessionOnStartup_);
    onDiskSettings = currentSettings;

#ifndef LE_SW_FMOD
    // Paths file

    juce::String const &rootPath(GUI::rootPath().getFullPathName());
    juce::String const &presetsFolder(GUI::presetsFolder().getFullPathName());

    unsigned int const rootLength(rootPath.length());
    unsigned int const presetsLength(presetsFolder.length());

    boost::mmap::basic_mapped_view const pathsFile(
        GUI::mapPathsFile(rootLength + sizeof('\n') + presetsLength));
    LE_ASSERT_MSG(pathsFile, "Unable to update the paths file.");
    if (!pathsFile)
        return;

#ifdef __APPLE__
    // Implementation note:
    //   See the Mac specific note in GUI::initializePaths().
    //                                        (01.12.2010.) (Domagoj Saric)
    //...mrmlj...NEW_JUCE...UNICODE
    rootPath.copyToUTF8(&pathsFile[0], rootLength + 1);
    pathsFile[rootLength] = '\n';
#else
    LE_ASSERT(std::memcmp(pathsFile.begin(), GUI::rootPath().getFullPathName().toUTF8(),
                          rootLength * sizeof(char)) == 0);
#endif // __APPLE__
    LE_ASSERT(pathsFile[rootLength] == '\n');
    //...mrmlj...+1 because copyToX() wants to append the null terminator...
    presetsFolder.copyToUTF8(&pathsFile[rootLength + sizeof('\n')], presetsLength + 1);
#endif // LE_SW_FMOD
}

#if LE_SW_ENGINE_INPUT_MODE >= 2
void SpectrumWorx::setInputModeToSetOnRestart(InputMode const pendingInputMode)
{
    inputModeToSetOnRestart_ = pendingInputMode;
    shouldLoadLastSessionOnStartup(true);
}
#endif // LE_SW_ENGINE_INPUT_MODE >= 2

bool SpectrumWorx::runningAsAU() const
{
#ifdef __APPLE__
    return runningAsAU_;
#else
    return false;
#endif // __APPLE__
}

juce::File SpectrumWorx::lastSessionPresetFile()
{
    return defaultPresetsFolder().getChildFile("__LastSession.swp");
}

juce::File SpectrumWorx::defaultPresetsFolder() { return GUI::rootPath().getChildFile("Presets"); }

juce::File SpectrumWorx::settingsFile() { return GUI::rootPath().getChildFile("SpectrumWorx.dat"); }

void SpectrumWorx::clearSideChannelData()
{
    LE_ASSERT(!sample_);
    SpectrumWorxCore::clearSideChannelData();
}

void SpectrumWorx::clearSideChannelDataIfNoSideChannel()
{
    if (!haveSideChannel())
        clearSideChannelData();
}

bool SpectrumWorx::haveSideChannel() const
{
    return sample_ || SpectrumWorxCore::haveSideChannel();
}

void SpectrumWorx::handleTimingInformationChange(
    LFO::Timer::TimingInformationChange const timingInformationChange)
{
    if (timingInformationChange.timingInfoChanged())
    {
        /// \todo The host should also be notified about LFO parameters changed
        /// due to tempo and/or measure changes.
        ///                                   (23.02.2011.) (Domagoj Saric)
        //...mrmlj...Processor::updateModuleLFOs() should have already been called...
        if (gui())
            gui()->updateForNewTimingInfo();
    }
}

SpectrumWorx const &SpectrumWorx::fromEngineSetup(Engine::Setup const &engineSetup)
{
    return static_cast<SpectrumWorx const &>(SpectrumWorxCore::fromEngineSetup(engineSetup));
}

bool SpectrumWorx::blockAutomation() const { return SpectrumWorxCore::blockAutomation(); }

////////////////////////////////////////////////////////////////////////////////
///
/// Programs and presets
///
////////////////////////////////////////////////////////////////////////////////

void SpectrumWorx::setProgram(std::uint8_t const newProgramIndex)
{
    std::uint8_t const currentProgramIndex(getProgram());
    if (newProgramIndex != currentProgramIndex)
    {
        Program &currentProgram(programs()[currentProgramIndex]);
        Program &newProgram(programs()[newProgramIndex]);
        {
            // Implementation note:
            //   Here we update only ourselves and expect the host to call
            // saveProgramState() after it calls setProgram() (Wavelab and
            // Reaper for example behave as expected while Ableton Live does not
            // call saveProgramState() but still seems to remain in a consistent
            // state, probably because of its internal caching).
            //                                (09.07.2010.) (Domagoj Saric)
            Utility::CriticalSectionLock const processLock(getProcessingLock());
            //...mrmlj...
            Parameters &newParametersSlot(newProgram.parameters());
            Parameters const newParameters(newParametersSlot);
            newParametersSlot = currentProgram.parameters();
            currentProgram_ = newProgramIndex;
            SpectrumWorxCore::setProgram(newProgram);
            resetForGlobalParameters(newParameters);
        }

#ifndef LE_SW_FMOD //...mrmlj...
        // Implementation note:
        //   Wavelab 5.0 creates a new instance when it wants to 'iterate'
        // over the presets of a plugin (when you press the "Presets"
        // button) and it does not create the GUI for that instance. Because
        // of that, here we must first check if the GUI was created.
        //                                    (22.10.2009.) (Domagoj Saric)
        // Implementation note:
        //   Cubase calls setProgram() from a non-GUI thread. A simple
        // solution of just blocking the GUI thread does not play nice in
        // Cubase.
        //                                    (17.01.2011.) (Domagoj Saric)
        GUI::postOrExecuteMessage(*this, [&](GUI::SpectrumWorxEditor &gui) {
            auto &previousChain(currentProgram.moduleChain());
            auto &newChain(newProgram.moduleChain());
            LE_ASSUME(&previousChain);
            gui.destroyChainGUIs(previousChain);
            gui.createChainGUIs(newChain);
            // Implementation note:
            //   The module arrow does not get cleared/erased (when it gets
            // moved to the left, i.e. the number of modules is smaller in
            // the set newProgram) automatically in Cubase 5 so we must
            // manually do a repaint (theoretically only the old module
            // arrow area needs to be repainted).
            //                            (19.01.2011.) (Domagoj Saric)
            /// \todo Investigate why is this required/why doesn't it happen
            /// automatically.
            ///                           (19.01.2011.) (Domagoj Saric)
            gui.repaint();
            return true;
        });
#endif // LE_SW_FMOD
    }
}

char const *SpectrumWorx::currentProgramName() const
{
    // Skip the (possible) leading asterisk...
    char const *LE_RESTRICT const pCurrentProgramName(&program().name()[0]);
    return &pCurrentProgramName[(pCurrentProgramName[0] == '*')];
}

bool SpectrumWorx::createGUI()
{
    LE_ASSUME(!editor_);
    try
    {
        editor_.emplace();
        LE_ASSUME(editor_.has_value());
        return true;
    }
    catch (...)
    {
        LE_ASSUME(!editor_);
        return false;
    }
}

void SpectrumWorx::destroyGUI()
{
    // Implementation note:
    //   Wavelab calls effEditClose even if effEditOpen failed so we
    // cannot assert( editor_.has_value() ) here.
    //                                        (23.11.2009.) (Domagoj Saric)
    editor_.reset();
}

void SpectrumWorx::updateGUIForGlobalParameterChange()
{
    if (!presetLoadingInProgress() &&
        GUI::
            isThisTheGUIThread()) //...mrmlj...ughly quick-hack to detect user initiated changes and avoid calling back the GUI...
        return;
    GUI::postMessage(*this, [](GUI::SpectrumWorxEditor &gui) {
        gui.updateForGlobalParameterChange();
        return true;
    });
}

void SpectrumWorx::updateGUIForEngineSetupChanges()
{
    if (!presetLoadingInProgress() &&
        GUI::
            isThisTheGUIThread()) //...mrmlj...ughly quick-hack to detect user initiated changes and avoid calling back the GUI...
        return;
    GUI::postMessage(*this, [](GUI::SpectrumWorxEditor &gui) {
        gui.updateForGlobalParameterChange();
        gui.updateForEngineSetupChanges();
        return true;
    });
}

SpectrumWorx &SpectrumWorx::effect(Editor &editor)
{
    return Utility::ParentFromOptionalMember<SpectrumWorx, Editor, &SpectrumWorx::editor_, false>()(
        editor);
}

////////////////////////////////////////////////////////////////////////////////
//
// Parameters
//
////////////////////////////////////////////////////////////////////////////////

bool SpectrumWorx::setGlobalParameter(FFTSize &parameter, FFTSize::param_type const newValue)
{
    bool const result(SpectrumWorxCore::setGlobalParameter(parameter, newValue));
    if (result)
    {
        /// \note Latency depends on the window size
        /// ( FFT size / zero padding * window size factor ) so notify the host
        /// when any of the relevant parameters change.
        ///                                   (23.05.2012.) (Domagoj Saric)
        /*LE_VERIFY*/ (latencyChanged());
        updateGUIForEngineSetupChanges();
    }
    return result;
}

bool SpectrumWorx::setGlobalParameter(OverlapFactor &parameter,
                                      OverlapFactor::param_type const newValue)
{
    bool const result(SpectrumWorxCore::setGlobalParameter(parameter, newValue));
    if (result)
        updateGUIForEngineSetupChanges();
    return result;
}

#if LE_SW_ENGINE_INPUT_MODE >= 2
bool SpectrumWorx::setGlobalParameter(InputMode &parameter, InputMode::param_type const newValue)
{
    auto const ioChannelsConfig(
        ioChannels(static_cast<SpectrumWorxCore::InputMode::value_type>(newValue)));

    bool const success(
        setNumberOfChannelsFromUser(ioChannelsConfig.first, ioChannelsConfig.second));
    LE_VERIFY((parameter.getValue() == newValue) || !success);
    if (success)
        updateGUIForEngineSetupChanges();
    return success;
}
#endif // LE_SW_ENGINE_INPUT_MODE >= 2

#if LE_SW_ENGINE_WINDOW_PRESUM
bool SpectrumWorx::setGlobalParameter(WindowSizeFactor &parameter,
                                      WindowSizeFactor::param_type const newValue)
{
    bool const result(SpectrumWorxCore::setGlobalParameter(parameter, newValue));
    if (result)
    {
        /// \note See the note in the FFTSize overload.
        ///                                   (23.05.2012.) (Domagoj Saric)
        /*LE_VERIFY*/ (latencyChanged());
        updateGUIForEngineSetupChanges();
    }
    return result;
}
#endif // LE_SW_ENGINE_WINDOW_PRESUM

#if 0 //...mrmlj...alex leftovers...

#pragma warning(push)
#pragma warning(disable : 4702) // Unreachable code.

float const * SpectrumWorx::ProceedSampler( unsigned int /*const ccBuffer*/ )
{
    //...mrmlj...temporarily commented out Alex's code until it is harvested for
    //knowledge and future ideas...
    LE_UNREACHABLE_CODE();
    //const bool loaded = samplebank.get() ? samplebank->Load() : false;
	//if (loaded)
	//{
    //       switch( parameters().get<StreamMode>().getValue() )
    //       {
    //           case StreamMode::Always:
	//		    samplebank->FillBuffer( sampleChannel_.begin(), ccBuffer );
    //               break;
    //           case StreamMode::MIDITrigger: // noteon play (reset)
	//	    {
	//		    ReallocStream( blockSize );
	//		    if (retrigger) samplebank->FillBuffer( sampleChannel_.begin(), ccBuffer );
	//		    if (lastbuffer)
	//		    {
	//			    samplebank->FillBuffer( sampleChannel_.begin(), ccBuffer );
	//			    if (samplebank->getFade()==0) lastbuffer = false;
	//		    }
    //               break;
	//	    }
    //           case StreamMode::MIDIGate: // noteon play(reset)-noteoff stop
	//	    {
	//		    ReallocStream( blockSize );
	//		    if (retrigger) samplebank->FillBuffer( sampleChannel_.begin(), ccBuffer );
	//		    if (lastbuffer)
	//		    {
	//			    samplebank->FillBuffer( sampleChannel_.begin(), ccBuffer );
	//			    if (samplebank->getFade()==0) lastbuffer = false;
	//		    }
    //               break;
	//	    }
    //           default: assert( false );
    //       }
    //      return sampleChannel_.begin();
	//} // sampler
    return nullptr;
}


bool SpectrumWorx::processMIDIEvent( ::VstMidiEvent const & /*event*/ )
{
    /// \todo Properly reimplement.
    ///                                       (25.05.2010.) (Domagoj Saric)

    //...mrmlj...temporarily commented out Alex's code until it is harvested for
    //knowledge and future ideas...
    LE_UNREACHABLE_CODE();
    return false;

	//const float Norm = 1.f / 127.f;

    //assert( event.type == kVstMidiType );

	//char const * const midiData( event.midiData );

	//BYTE const status( midiData[0] & 0xF0 ); // ignoring channel

	//// TODO
	///*brothomStates: trigger / gate
	//brothomStates: trigger always just restarts the sample
	//brothomStates: gate restarts and stops
	//brothomStates: trigger = note on only
	//brothomStates: gate = note on[trig], note off[stop]*/
	//// TODO: capture special controllers  like allnotes off//all sounds off

	//if ( status == 0x90 )	// capture NoteOn
	//{
	//	//uint8  note = midiData[1] & 0x7F;
	//	//uint8  velo = midiData[2] & 0x7F;
	//	if ( parameters().get<StreamMode>() == 1 )
	//	{
	//		//if (samplebank->getFade()==0)
	//		//{
	//		//	retrigger = true;
	//		//	if (firstfade)
	//		//		samplebank->StartFade(1);
	//		//	else
	//		//	{
	//		//		samplebank->StartFade(3);
	//		//		firstfade  = true;
	//		//	}
	//		//}
	//	}
	//	if ( parameters().get<StreamMode>() == 2 )
	//	{
	//		if (retrigger) return 1;//important
	//		//if (samplebank->getFade()==0)
	//		//{
	//		//	samplebank->StartFade(3);
	//		//	retrigger = true;
	//		//}
	//	}
	//}

	//if (status == 0x90)	// capture NoteOff = NoteOn with Velocity 0
	//{
	//	//uint8  note = midiData[1] & 0x7F;
	//	//if (lastbuffer) return 1;
	//	BYTE velo = midiData[2] & 0x7F;
	//	if (velo == 0)
	//	{
	//		//Noteoff
	//		if ( parameters().get<StreamMode>() == 2 )
	//		{
	//			//if (samplebank->getFade()==0)
	//			{
	//				//samplebank->StartFade(2);
	//				retrigger = false;
	//				lastbuffer = true;
	//			}
	//		}
	//	}
	//}

	//if (status == 0x80)	// capture NoteOff
	//{
	//	//uint8  note = midiData[1] & 0x7F;
	//	//uint8  velo = midiData[2] & 0x7F;
	//	//if (lastbuffer) return 1;
	//	if ( parameters().get<StreamMode>() == 2 )
	//	{
	//		//if (samplebank->getFade()==0)
	//		{
	//			//samplebank->StartFade(2);
	//			retrigger = false;
	//			lastbuffer = true;
	//		}
	//	}
	//}

	//if (status == 0xB0)	// capture controllers
	//{
	//	currentDelta = event.deltaFrames;
	//	BYTE  controller =  midiData[1] & 0x7F;
	//	float  value      = (midiData[2] & 0x7F) * Norm;
    //       assert( !"New code does not seem to support MIDI yet." );
	//	//mainchain->processCC(controller, value);
	//}

	//return 1;	// want more
}

#pragma warning(pop)

#endif //...mrmlj...alex leftovers...

//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
