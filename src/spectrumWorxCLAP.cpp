////////////////////////////////////////////////////////////////////////////////
///
/// \file spectrumWorxCLAP.cpp
/// -------------------------
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "spectrumWorxCLAP.hpp"
#include "gui/editor/spectrumWorxEditor.hpp"

#include "core/modules/factory.hpp"
// \note Order matters and is not alphabetical: finalImplementations.hpp defines
// Module::Impl<> and needs Module complete. factory.cpp includes them in this
// order for the same reason.
#include "core/modules/moduleDSPAndGUI.hpp"
#include "core/modules/finalImplementations.hpp"

// \note The interop templates are defined in .inl files that only their
// instantiating translation unit includes -- this one. They call through to a
// module's UI on a slot change, so the complete type is needed.
#include "gui/modules/moduleUI.hpp"

#include "core/host_interop/clapParameterEdge.hpp"
#include "core/host_interop/host2PluginImpl.inl"
#include "core/host_interop/plugin2HostImpl.inl"

#include "core/threading/threadCheck.hpp"

#include "gui/gui.hpp" // warningMessageBox()

// The state format: GUI::loadPreset() takes the editor as a pointer precisely so
// that this can call it with none, and savePreset() is the writer at the far end
// of it. See doc/tech/streaming_format.md.
#include "gui/editor/presetLoading.hpp"
#include "le/spectrumworx/presets.hpp"

#include "le/math/vector.hpp" // Math::copy(), for the sample's wrap

#include <sst/plugininfra/cpufeatures.h>
#include <sst/plugininfra/version_information.h>

#include <algorithm>
#include <cstring>
#include <mutex>
//------------------------------------------------------------------------------
namespace LE::SW
{
//------------------------------------------------------------------------------
namespace
{
constexpr clap_id mainInputPort{0};
constexpr clap_id sideChainInputPort{1};
constexpr clap_id mainOutputPort{2};

/// \note `stateMagic`, the four bytes `SWX1`, stood here in front of a
/// `(uint32 id, double value)` array. Dropped with the blob it introduced rather
/// than kept as a fallback: nothing has shipped, so the only sessions holding
/// one are development sessions in this tree, and a permanent second reader for
/// a format no user has is dead weight from the day it is written. A stream that
/// does not begin with `<` fails the parse, which is how one is refused.
///                                           (02.08.2026.) (SW port)

bool writeFully(clap_ostream const *const stream, void const *const data, std::size_t size)
{
    auto const *cursor(static_cast<char const *>(data));
    while (size > 0)
    {
        auto const written(stream->write(stream, cursor, size));
        if (written <= 0)
            return false;
        cursor += written;
        size -= static_cast<std::size_t>(written);
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////
///
/// \brief Reads \p stream to its end into a NUL-terminated, writable buffer.
///
/// \note The whole stream before any of it is parsed, because the document is
/// not self-delimiting: a host may hand back any number of bytes and the reader
/// has to see all of them. `readFully` into a fixed size, which is what the
/// binary blob used, cannot express that.
///
/// \note A `read` of 0 is the end and a negative is an error, and the two are
/// not the same answer -- a truncated read that reported failure would be
/// indistinguishable from an empty state. The buffer grows geometrically; the
/// shape is `sst::plugininfra::patch_support::inStreamToPatch`, which does the
/// same job for the other Surge Synth Team plugins.
///
////////////////////////////////////////////////////////////////////////////////

std::optional<std::vector<char>> readWholeStream(clap_istream const *const stream)
{
    constexpr std::size_t chunk{1u << 12};

    std::vector<char> buffer;
    std::size_t used(0);
    for (;;)
    {
        buffer.resize(used + chunk);
        auto const read(stream->read(stream, buffer.data() + used, chunk));
        if (read < 0)
            return std::nullopt;
        if (read == 0)
            break;
        used += static_cast<std::size_t>(read);
    }

    buffer.resize(used + 1);
    buffer[used] = '\0'; // the parse is destructive and wants a terminator
    return buffer;
}

////////////////////////////////////////////////////////////////////////////////
//
// sampleChunk()
// -------------
//
////////////////////////////////////////////////////////////////////////////////
///
/// \brief One block of a looped sample channel, advancing \p position past it.
///
/// \return the sample's own data where a whole block is contiguously available,
/// which is every block but the one that wraps; \p workBuffer, filled, where it
/// is not. So the common case costs nothing and only the wrap copies.
///
/// \note LE::SW::getChannelDataChunk in the 2016 plugin class, moved here with
/// its shape intact -- it is the whole of what feeding the engine from a file
/// amounts to.
///
////////////////////////////////////////////////////////////////////////////////

float const *LE_RESTRICT sampleChunk(Sample::ChannelData const &channelData,
                                     std::uint32_t &position, std::uint32_t chunkSize,
                                     float *LE_RESTRICT const workBuffer)
{
    auto const dataSize(static_cast<std::uint32_t>(channelData.size()));
    LE_ASSERT(position <= dataSize);
    if (dataSize > (position + chunkSize))
    {
        auto const *const pChunk(&channelData[position]);
        position += chunkSize;
        return pChunk;
    }

    auto *workBufferPosition(workBuffer);
    while (chunkSize)
    {
        if (position == dataSize)
            position = 0;
        auto const amountToCopy(std::min<std::uint32_t>(dataSize - position, chunkSize));
        Math::copy(&channelData[position], workBufferPosition, amountToCopy);
        workBufferPosition += amountToCopy;
        position += amountToCopy;
        chunkSize -= amountToCopy;
    }
    return workBuffer;
}
} // namespace

/// \note Every string here comes from the build (src/CMakeLists.txt), because
/// the bundle identifiers are made of the same ones and a second copy is how
/// they drift. SW_CLAP_ID in particular is the plugin's identity in three
/// places at once: the CLAP id, the `.clap`/`.vst3`/`.component` bundle
/// identifiers, and -- by way of a SHA-1 in clap-wrapper -- the VST3 class id.
///                                           (01.08.2026.) (SW port)
clap_plugin_descriptor const *descriptor()
{
    static char const *features[]{CLAP_PLUGIN_FEATURE_AUDIO_EFFECT, CLAP_PLUGIN_FEATURE_STEREO,
                                  "spectral", nullptr};
    static clap_plugin_descriptor const description{
        CLAP_VERSION,
        SW_CLAP_ID,
        PRODUCT_NAME,
        SW_VENDOR,
        SW_VENDOR_URL,
        "",
        "",
        sst::plugininfra::VersionInformation::project_version_and_hash,
        "Modular spectral effects",
        &features[0]};
    return &description;
}

clap_plugin const *createPlugin(clap_host const *const host)
{
    return (new SpectrumWorxCLAP(host))->clapPlugin();
}

SpectrumWorxCLAP::SpectrumWorxCLAP(clap_host const *const host) : PluginHelper(descriptor(), host)
{
    setProgram(program_);
    parameterIDs_.reserve(ParameterCounts::maxNumberOfParameters);

    clapJuceShim_ = std::make_unique<sst::clap_juce_shim::ClapJuceShim>(this);
    clapJuceShim_->setResizable(false);
}

SpectrumWorxCLAP::~SpectrumWorxCLAP() = default;

bool SpectrumWorxCLAP::init() noexcept
{
    /// \note `clap_plugin::init` is `[main-thread]` by contract, so the thread
    /// running it is the answer for the life of the plugin -- and it is the only
    /// answer available, `clap.thread-check` being an optional extension a host
    /// need not offer. See core/threading/threadCheck.hpp.
    Threading::markMainThread();

    // The host may ask for the parameter list before activate(), and does.
    rebuildParameterIDs();
    return true;
}

bool SpectrumWorxCLAP::activate(double const sampleRate, std::uint32_t,
                                std::uint32_t const maxFrames) noexcept
{
    sampleRate_ = sampleRate;

    // Stereo in, stereo out. Anything else waits for 5.7 and the input-mode
    // parameter; the engine supports far more, the port list above does not.
    setNumberOfChannels(2, 2);
    setSampleRate(static_cast<float>(sampleRate));

    // The host promises never to exceed maxFrames, and SpectrumWorxCore asserts
    // exactly that against its own buffers. A shorter final block is fine.
    setBlockSize(maxFrames);

    if (!initialise())
        return false;

    resume();
    engineRunning_ = true;
    latencyInSamples_ = engineSetup().latencyInSamples();

    /// \note A sample is decoded to the engine's rate, and a session can be
    /// restored -- sample and all -- before the host has said what that rate is.
    /// Re-read it here when they disagree; the 2016 build did not, and played
    /// the sample at the wrong pitch for the rest of the session.
    ///                                       (01.08.2026.) (SW port)
    if (sample_ && (sample_.sampleRate() != static_cast<unsigned int>(sampleRate)))
        setNewSample(sample_.sampleFile());

    /// \note An editor that opened before this point built its module knobs
    /// against an engine with no sample rate, so the ranges that quantise to a
    /// step time or a bin width could not be derived and were left alone. Now
    /// they can be. Nothing else re-ranges them -- the editor's own
    /// updateForEngineSetupChanges() was wired only to the four settings
    /// combo boxes -- and restoring a session before activate() is exactly the
    /// order a standalone starts in.
    ///                                       (29.07.2026.) (SW port)
    if (pEditor_)
        pEditor_->updateForEngineSetupChanges();

    return true;
}

void SpectrumWorxCLAP::deactivate() noexcept
{
    if (engineRunning_)
    {
        suspend();
        engineRunning_ = false;
    }
    sampleRate_ = 0;
}

void SpectrumWorxCLAP::reset() noexcept
{
    if (engineRunning_)
        SpectrumWorxCore::reset();
}

////////////////////////////////////////////////////////////////////////////////
// Audio ports
////////////////////////////////////////////////////////////////////////////////

std::uint32_t SpectrumWorxCLAP::audioPortsCount(bool const isInput) const noexcept
{
    return isInput ? 2 : 1;
}

bool SpectrumWorxCLAP::audioPortsInfo(std::uint32_t const index, bool const isInput,
                                      clap_audio_port_info *const info) const noexcept
{
    std::memset(info, 0, sizeof(*info));
    info->channel_count = 2;
    info->port_type = CLAP_PORT_STEREO;
    /// \note Deliberately never an in-place pair. With an input gain of exactly
    /// one SpectrumWorxCore hands the host's own input pointers straight to
    /// Engine::Processor::process, and the WOLA path has not been audited for
    /// aliasing input and output. Revisit under 5.7 with a test, not by
    /// inspection.
    info->in_place_pair = CLAP_INVALID_ID;

    if (isInput && index == 0)
    {
        info->id = mainInputPort;
        info->flags = CLAP_AUDIO_PORT_IS_MAIN;
        std::strncpy(info->name, "Main In", CLAP_NAME_SIZE - 1);
        return true;
    }
    if (isInput && index == 1)
    {
        info->id = sideChainInputPort;
        info->flags = 0;
        std::strncpy(info->name, "Side Chain", CLAP_NAME_SIZE - 1);
        return true;
    }
    if (!isInput && index == 0)
    {
        info->id = mainOutputPort;
        info->flags = CLAP_AUDIO_PORT_IS_MAIN;
        std::strncpy(info->name, "Main Out", CLAP_NAME_SIZE - 1);
        return true;
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////////
// Parameters
////////////////////////////////////////////////////////////////////////////////

/// \brief Every parameter the engine can ever have, once, at init().
///
/// \note Passing nullptr rather than the Program is the whole point: it asks
/// for the *maximal* list -- every slot's full complement of module and LFO
/// parameters -- instead of the list the current program happens to have.
///
///   The list has to be maximal because CLAP does not let a plugin change its
/// parameter count while it is active. ext/params.h is explicit: adding or
/// removing parameters means calling clap_host->restart() and waiting for
/// deactivate() before CLAP_PARAM_RESCAN_ALL. Doing it from process() or
/// flush() -- which is what following the Program does, since a host can swap a
/// slot's effect with an event mid-block -- is not something a host has to cope
/// with, and the ones that do not simply keep the count they first read. That
/// is why an empty session showed eleven parameters: six globals and five slot
/// selectors, with nothing for any module because no slot held an effect yet.
///
///   Nothing is lost by declaring them all. A parameter belonging to a slot
/// whose effect does not have it reads as N/A rather than as an unknown ID, so
/// a host's automation lane stays attached across an effect swap -- which is
/// the behaviour isValidParamId() was already written for.
void SpectrumWorxCLAP::rebuildParameterIDs()
{
    parameterIDs_.resize(numberOfParameters(nullptr));
    getParameterIDs({parameterIDs_.data(), parameterIDs_.size()}, nullptr);
    LE_ASSERT(parameterIDs_.size() == ParameterCounts::maxNumberOfParameters);
}

bool SpectrumWorxCLAP::isValidParamId(clap_id const id) const noexcept
{
    /// \note Every ParameterID that decodes is valid: the model answers "N/A"
    /// for a slot whose effect does not have that parameter rather than
    /// pretending the ID is unknown, which is what keeps a host's automation
    /// lane attached across an effect swap.
    ParameterID const parameterID{Plugins::ParameterID{id}};
    return parameterID.type() <= ParameterID::LFOParameter;
}

std::uint32_t SpectrumWorxCLAP::paramsCount() const noexcept
{
    /// \note The list built at init(), not numberOfParameters(&program()). See
    /// rebuildParameterIDs(): this count is fixed for the plugin's lifetime.
    return static_cast<std::uint32_t>(parameterIDs_.size());
}

bool SpectrumWorxCLAP::paramsInfo(std::uint32_t const index,
                                  clap_param_info *const info) const noexcept
{
    if (index >= parameterIDs_.size())
        return false;

    auto const id(parameterIDs_[index]);
    ParameterID const parameterID{id};

    /// \note Two queries, and which one answers what is the whole contract.
    ///
    ///   The *fixed* description -- the maximal one, over a null Program, as in
    /// rebuildParameterIDs -- supplies every number and every flag a host may
    /// not see move: min_value, max_value, is_stepped. ext/params.h lists those
    /// three together under CLAP_PARAM_RESCAN_ALL, which is legal only while
    /// deactivated, and a slot's effect changes mid-block.
    ///
    ///   The *live* one supplies only what RESCAN_INFO explicitly covers: the
    /// name, the module path, and whether the parameter is currently used at
    /// all. Those may follow whichever effect the slot holds.
    Plugins::ParameterInformation<Protocol> fixed;
    getParameterRanges(parameterID, fixed, nullptr);

    Plugins::ParameterInformation<Protocol> live;
    getParameterProperties(parameterID, live, &program());

    std::memset(info, 0, sizeof(*info));
    info->id = id.value;
    info->cookie = nullptr;
    info->flags = CLAP_PARAM_IS_AUTOMATABLE;

    /// \note A parameter belonging to a slot whose effect does not have it --
    /// most of the list on an empty instance. The model spells that as an empty
    /// range and "not automatable", which was enough while the list was dynamic
    /// and such a parameter was simply absent from it. This list is fixed, so
    /// the host sees them, and CLAP_PARAM_IS_HIDDEN says just what is meant:
    /// "not shown, because it is currently not used". It is a RESCAN_INFO flag,
    /// so it may come and go as slots are filled.
    if (!live.isAutomatable() || !CLAPEdge::isPresent(live))
        info->flags |= CLAP_PARAM_IS_HIDDEN;

    /// \note A "meta" parameter is one whose value changes what the other
    /// parameters *are* -- which effect a slot holds, chiefly. Telling the host
    /// its value requires a rescan is exactly what this flag is for. Taken from
    /// the fixed description so that filling a slot does not flip it.
    if (fixed.isMeta())
        info->flags |= CLAP_PARAM_REQUIRES_PROCESS;

    if (CLAPEdge::isNormalised(parameterID))
    {
        /// \note The 0..1 edge over a natural range that belongs to the effect.
        /// No CLAP_PARAM_IS_STEPPED either, for the same reason there is no real
        /// range: a step count is a property of the effect in the slot, and the
        /// flag is in the same RESCAN_ALL list. The enumerated ones still read
        /// as their names through paramsValueToText.
        info->min_value = 0;
        info->max_value = 1;
        info->default_value = CLAPEdge::defaultToHost(parameterID, fixed);
    }
    else
    {
        // Global and slot-selector parameters: ranges the plugin owns, and which
        // therefore never move. They keep their real values and their steps.
        info->min_value = fixed.minimum();
        info->max_value = fixed.maximum();
        info->default_value = fixed.default_();
        if (fixed.isStepped())
            info->flags |= CLAP_PARAM_IS_STEPPED;
    }

    std::strncpy(info->name, live.name(), CLAP_NAME_SIZE - 1);
    modulePathFor(parameterID, info->module);
    return true;
}

void SpectrumWorxCLAP::modulePathFor(ParameterID const parameterID,
                                     char (&path)[CLAP_PATH_SIZE]) const noexcept
{
    switch (parameterID.type())
    {
    case ParameterID::GlobalParameter:
        std::strncpy(path, "Global", CLAP_PATH_SIZE - 1);
        break;
    case ParameterID::ModuleChainParameter:
        std::snprintf(path, CLAP_PATH_SIZE, "Slot %u",
                      parameterID.value._.moduleChain.moduleIndex + 1u);
        break;
    case ParameterID::ModuleParameter:
        std::snprintf(path, CLAP_PATH_SIZE, "Slot %u", parameterID.value._.module.moduleIndex + 1u);
        break;
    case ParameterID::LFOParameter:
        std::snprintf(path, CLAP_PATH_SIZE, "Slot %u/LFO",
                      parameterID.value._.lfo.moduleIndex + 1u);
        break;
    }
}

/// \note The *live* range, not the fixed one paramsInfo advertises: normalising
/// is exactly the act of expressing a value that belongs to the effect currently
/// in the slot on an edge that does not.
///
///   A local rather than a member, at each of the four call sites below, because
/// they run on three different threads -- the host's main thread, the audio
/// thread and the UI thread -- and one shared scratch description between them
/// would be a race. It costs a clear() and a dispatch; neither allocates nor
/// formats a string, which is what getParameterRanges() is for.
bool SpectrumWorxCLAP::liveRanges(ParameterID const parameterID,
                                  Plugins::ParameterInformation<Protocol> &ranges) const
{
    getParameterRanges(parameterID, ranges, &program());
    if (CLAPEdge::isPresent(ranges))
        return true;

    /// \note An empty slot has no range at all -- the model spells that as a
    /// degenerate 0..0 -- so fall back to the maximal description, which is also
    /// the one paramsInfo advertised. Nothing sensible can be normalised against
    /// 0..0, and a caller still needs a scale to work on.
    getParameterRanges(parameterID, ranges, nullptr);
    return false;
}

bool SpectrumWorxCLAP::paramsValue(clap_id const id, double *const value) noexcept
{
    if (!isValidParamId(id))
        return false;

    ParameterID const parameterID{Plugins::ParameterID{id}};
    Plugins::ParameterInformation<Protocol> ranges;

    /// \note A parameter no effect currently owns has no value of its own, and
    /// what the engine answers for one is not the default it was advertised with.
    /// It reads as that advertised default instead -- `ranges` is the maximal
    /// description by then, the same one paramsInfo used, so the two agree by
    /// construction. A host checks exactly this at init (param-default-values).
    if (!liveRanges(parameterID, ranges))
    {
        *value = CLAPEdge::defaultToHost(parameterID, ranges);
        return true;
    }

    *value = CLAPEdge::toHost(parameterID, ranges, getParameter(parameterID));
    return true;
}

bool SpectrumWorxCLAP::paramsValueToText(clap_id const id, double const value, char *const display,
                                         std::uint32_t const size) noexcept
{
    if (!isValidParamId(id))
        return false;

    ParameterID const parameterID{Plugins::ParameterID{id}};

    /// \note Renders the parameter's *own* value and ignores \p value, which is
    /// not what CLAP asks for and is deliberate until the printer can do better.
    ///
    ///   Asking it to render a supplied value means
    /// AutomatedParameterPrinter's `Linear` arm, and that arm default-constructs
    /// the parameter to assign the value to: `Parameter parameterValue;`
    /// (printer.hpp). Some of these parameters do not have a range of their own to
    /// be valid against -- an LFO's bounds and its period scale are
    /// DynamicRangeParameterTag, and find their limits by walking from their own
    /// address to the LFO that owns them (LFOImpl::snapPeriodScaleFromAutomation
    /// does it explicitly). A detached temporary has no owner, so it validates
    /// against whatever that walk lands on. Assigning the lower bound of a
    /// 20..2000 Hz target then asserts, in a throwaway object, having corrupted
    /// nothing -- but in a checked build the assertion ends the host, which is
    /// what a debug plugin did as soon as a rescan made a host read the list.
    ///
    ///   The `Internal` arm has none of that: it prints `parameter.getValue()` on
    /// the real parameter, which is the arm every other format has always used --
    /// Windows asserts outright that this is the only one it ever takes. So a host
    /// asking "what would 0.25 read as" is told what the parameter reads as now.
    /// Wrong for an automation lane's tooltip, right for the common case of
    /// rendering the current value, and it cannot assert.
    ///
    /// \todo Give the printer an arm that takes the value *and* the live
    /// parameter, so a dynamic range has an owner to ask. That is the same
    /// machinery paramsTextToValue needs below, and worth doing once for both.
    ///                                       (30.07.2026.) (SW port)
    std::array<char, 128> text{};
    getParameterDisplay(parameterID, {text.data(), text.size()}, nullptr);

    std::array<char, 32> unit{};
    getParameterLabel(parameterID, {unit.data(), unit.size()}, &program());

    std::snprintf(display, size, "%s%s", text.data(), unit.data());
    return true;
}

/// \brief Declined, deliberately, until the printers can be run backwards.
///
/// \note The 2016 code never needed this -- neither VST 2.4 nor AU asks a plugin
/// to parse a typed-in value -- so nothing in the parameter system inverts a
/// display transform. `Parameters::DisplayValueTransformer` has `transform` and
/// no counterpart, and the effect-specific printers go through
/// `AutomatedParameterPrinter` the same one way.
///
///   The previous implementation here ran `strtod` over the text and returned the
/// result as if display units were storage units. For anything with a transform
/// that is simply a wrong value: clap-validator caught the input gain going
/// `0.001` -> `"-60dB"` -> `-60.0` -> `"nandB"`, a NaN written straight into the
/// engine. Returning false is the honest answer -- `text_to_value` is optional,
/// and a host falls back to its own editing -- and it cannot corrupt state.
///
/// \todo Give `DisplayValueTransformer` an `inverse` alongside `transform` (four
/// specialisations: two dB, two percentage) and teach the effect-specific
/// printers the same, then parse here and invert per parameter.
bool SpectrumWorxCLAP::paramsTextToValue(clap_id, char const *, double *) noexcept { return false; }

bool SpectrumWorxCLAP::handleEvent(clap_event_header const *const header)
{
    if (header->space_id != CLAP_CORE_EVENT_SPACE_ID)
        return false;
    if (header->type != CLAP_EVENT_PARAM_VALUE)
        return false;

    auto const *const event(reinterpret_cast<clap_event_param_value const *>(header));
    if (!isValidParamId(event->param_id))
        return false;

    ParameterID const parameterID{Plugins::ParameterID{event->param_id}};
    Plugins::ParameterInformation<Protocol> ranges;
    /// \note Dropped rather than stored when no effect in that slot owns it. The
    /// list is maximal (see rebuildParameterIDs), so a host can and does write to
    /// every ID in it, including a slot's tenth parameter while the slot holds a
    /// two-parameter effect. Reading one is safe --
    /// Automation::getAutomatedLFOParameter answers with the default -- but
    /// *writing* one is not: setAutomatedLFOParameter has no matching guard and
    /// indexes straight into module.lfo(), past the end. clap-validator's
    /// param-set-events and state-reproducibility tests both walk into it.
    ///
    ///   Dropping is also the right answer rather than merely the safe one: the
    /// value has nowhere to live, and filling the slot later brings the new
    /// effect's own default -- which is what paramsValue() reports for it in the
    /// meantime.
    ///                                       (29.07.2026.) (SW port)
    if (!liveRanges(parameterID, ranges))
        return false;

    setParameter(parameterID, CLAPEdge::fromHost(parameterID, ranges, event->value));

    /// \note Only a module-chain parameter changes what the *other* parameters
    /// are: it decides which effect a slot holds, and so how many parameters
    /// that slot has and what they are called. Everything else is just a value.
    return parameterID.type() == ParameterID::ModuleChainParameter;
}

void SpectrumWorxCLAP::requestRescan(clap_param_rescan_flags const flags)
{
    // One callback per outstanding batch: a block that swaps every slot would
    // otherwise ask the host five times over.
    if (pendingRescan_.fetch_or(flags) == 0)
        _host.requestCallback();
}

void SpectrumWorxCLAP::paramsFlush(clap_input_events const *const in,
                                   clap_output_events const *const out) noexcept
{
    auto const size(in->size(in));
    bool effectChanged(false);
    for (std::uint32_t event(0); event < size; ++event)
        effectChanged |= handleEvent(in->get(in, event));

    if (effectChanged)
        requestRescan(CLAP_PARAM_RESCAN_INFO | CLAP_PARAM_RESCAN_TEXT | CLAP_PARAM_RESCAN_VALUES);

    flushUIEdits(out);
}

////////////////////////////////////////////////////////////////////////////////
// Processing
////////////////////////////////////////////////////////////////////////////////

clap_process_status SpectrumWorxCLAP::process(clap_process const *const process) noexcept
{
    /// \note Stage 4.2, and the reason it is one line here rather than a fix to
    /// `Math::FPUDisableDenormalsGuard`: **nothing was flushing denormals at
    /// all**, on any platform. The engine's own two guards are inside
    /// `#ifdef LE_SW_SDK_BUILD`, which nothing defines, and the third is in
    /// `SpectrumWorx::process` — the 2016 host-facing class, which the CLAP does
    /// not call. The audio path is this function, `runEngine()` and
    /// `SpectrumWorxCore::process()`. So there was no working guard to rekey off
    /// the long-dead `BOOST_SIMD_HAS_SSE_SUPPORT`; there was no guard.
    ///
    ///   `FPUStateGuard` covers x86-64 (FTZ and DAZ via MXCSR) and aarch64 (FZ
    ///   via FPCR) and restores the caller's state on the way out, which a host
    ///   is entitled to expect. Here rather than in `runEngine()` because event
    ///   handling and `flushUIEdits()` convert parameter values, and because
    ///   the whole callback is the unit a host cares about. All four formats
    ///   funnel through here: clap-wrapper drives the VST3, AUv2 and standalone
    ///   off this same entry point.
    ///                                   (29.07.2026.) (SW port)
    sst::plugininfra::cpufeatures::FPUStateGuard const denormalGuard;

    /// \note Makes `Threading::isAudioThread()` true for everything below, and
    /// opens a RealtimeSanitizer realtime region so that an allocation, a lock or
    /// a syscall reached from anywhere under here is reported with a stack. Both
    /// compile away without `-fsanitize=realtime`. See cmake/sw-sanitizers.cmake.
    Threading::ScopedAudioCallback const audioCallback;

    bool effectChanged(false);
    if (auto const *const in = process->in_events)
    {
        auto const size(in->size(in));
        for (std::uint32_t event(0); event < size; ++event)
            effectChanged |= handleEvent(in->get(in, event));
    }

    /// \note Names, module paths and displayed values change; the parameter
    /// *list* does not, so this never needs CLAP_PARAM_RESCAN_ALL -- which
    /// would be illegal here, an active plugin having to go through
    /// clap_host->restart() first. See rebuildParameterIDs().
    if (effectChanged)
        requestRescan(CLAP_PARAM_RESCAN_INFO | CLAP_PARAM_RESCAN_TEXT | CLAP_PARAM_RESCAN_VALUES);

    if (process->out_events)
        flushUIEdits(process->out_events);

    updateLFOTiming(process);

    runEngine(process);

    return CLAP_PROCESS_CONTINUE;
}

/// \brief Moves the LFO clock forward by one block.
///
/// \note Nothing did. Every LFO reads its phase off Engine::Processor's one
/// LFO::Timer, and the only code that ever moved that timer was
/// `SpectrumWorxSharedImpl::process()` -- the 2016 host-facing layer this class
/// stands in for and does not inherit (see the note on the class). So
/// `currentTimeInBars()` held 0 for the plugin's lifetime, and every symptom
/// followed from that one fact: each waveform returned its value at position 0
/// forever, so enabling an LFO pinned its target to one end of the range instead
/// of sweeping it; no period boundary was ever crossed, so the per-period
/// waveforms (RandomHold, RandomSlide, Dirac) never retriggered; and
/// `hasTempoInformation()` stayed false, which is what greys out the editor's
/// N/T/D sync buttons, prints the period in milliseconds rather than note
/// ratios, and defaults every new LFO to Free.
///
///   Three cases where 2016 had two, because a CLAP transport can be present and
/// parked:
///
///   - Playing, on a beats timeline: follow the host. An LFO is then phase-locked
///     to song position and rides a locate or a loop rather than drifting from it.
///   - Tempo known but stopped -- also a host that reports a tempo and no beats
///     timeline: keep the host's tempo and meter, and carry the phase forward
///     from where the timer already stands. An LFO keeps running, at the right
///     rate, with the transport parked, which is what Six Sines and surge-xt2 do
///     and what auditioning a patch without pressing play calls for. Continuing
///     from the timer's own position rather than a counter of our own is what
///     makes the handover in either direction seamless.
///   - No transport at all, or a tempo we cannot use: free run at the engine's
///     assumed 120 BPM 4/4, which is exactly what `updatePosition()` is.
///
/// \note Both `updatePosition()` and the three-argument
/// `updatePositionAndTimingInformation()` call `handleTimingInformationChange()`
/// themselves. The 2016 callers wrapped them in a second call of their own
/// (`SpectrumWorxSharedImpl::process()`, `SpectrumWorx::updatePosition()`), which
/// ran the period resnap twice for one change; not repeated here.
///                                       (30.07.2026.) (SW port)
void SpectrumWorxCLAP::updateLFOTiming(clap_process const *const process) noexcept
{
    auto const sampleRate(getSampleRate());
    if (sampleRate <= 0) [[unlikely]]
        return; // Not activated; nothing sensible to advance by.

    auto const *const transport(process->transport);

    constexpr std::uint32_t tempoAndMeter(CLAP_TRANSPORT_HAS_TEMPO |
                                          CLAP_TRANSPORT_HAS_TIME_SIGNATURE);

    /// \note tsig_num reaches the engine as the measure numerator, a std::uint8_t
    /// it divides by -- so a zero or an out-of-range one is not a tempo we can use.
    bool const usableTempo(transport && ((transport->flags & tempoAndMeter) == tempoAndMeter) &&
                           (transport->tempo > 0) && (transport->tsig_num >= 1) &&
                           (transport->tsig_num <= 255));
    if (!usableTempo)
    {
        updatePosition(process->frames_count);
        return;
    }

    auto const beatsPerBar(static_cast<double>(transport->tsig_num));
    auto const barDuration(beatsPerBar * 60 / transport->tempo);

    constexpr std::uint32_t playingOnBeats(CLAP_TRANSPORT_HAS_BEATS_TIMELINE |
                                           CLAP_TRANSPORT_IS_PLAYING);

    double positionInBars;
    if ((transport->flags & playingOnBeats) == playingOnBeats)
    {
        positionInBars =
            (static_cast<double>(transport->song_pos_beats) / CLAP_BEATTIME_FACTOR) / beatsPerBar;
        // A count-in puts the song before its own start; the timer asserts >= 0.
        if (positionInBars < 0)
            positionInBars = 0;
    }
    else
    {
        auto const seconds(process->frames_count / static_cast<double>(sampleRate));
        positionInBars = lfoTimer().currentTimeInBars() + (seconds / barDuration);
    }

    updatePositionAndTimingInformation(static_cast<float>(positionInBars),
                                       static_cast<float>(barDuration),
                                       static_cast<std::uint8_t>(transport->tsig_num));
}

void SpectrumWorxCLAP::runEngine(clap_process const *const process) noexcept
{
    if ((process->audio_inputs_count == 0) || (process->audio_outputs_count == 0))
        return;

    auto const &input(process->audio_inputs[0]);
    auto &output(process->audio_outputs[0]);
    if (!input.data32 || !output.data32)
        return; // 64 bit hosts get silence rather than a crash until 5.7.

    /// \note Unchecked, for the same reason getSampleRate() is: the channel
    /// count is not one of the fields isEngineSetupUpToDate() compares, and this
    /// runs on the audio thread before SpectrumWorxCore::process() takes the
    /// processing lock. See the note on getSampleRate().
    auto const channels(uncheckedEngineSetup().numberOfChannels());
    if ((input.channel_count < channels) || (output.channel_count < channels))
        return;

    if (!engineRunning_)
        return;

    // The engine reads a side channel whenever the input mode calls for one, and
    // does not check that the host actually connected the port.
    float const *const *sideChannels(
        ((process->audio_inputs_count > 1) && process->audio_inputs[1].data32)
            ? process->audio_inputs[1].data32
            : input.data32);

    ////////////////////////////////////////////////////////////////////////////
    // An external audio file, when one is loaded, in place of the port.
    //
    // \note The lock, and why it is a try_lock and why it is held past the call
    // below. The message thread swaps the decoded data under this one, so the
    // pointers below have to be read and used with it held -- and it is the
    // same recursive lock SpectrumWorxCore::process() takes, so holding it here
    // is what stops that call from dropping the block. If another thread has it
    // (a load, a preset, an FFT-size change) this block plays the host's side
    // chain instead of the sample, which is a block of the wrong source rather
    // than a block of silence or a wait on the audio thread.
    //
    //   2016 got the same effect from taking its try_lock at the top of its own
    // process() and reading the sample inside it. Every part of this belongs to
    // the threading redesign; it is written this way because that is what is
    // there now.
    //                                        (01.08.2026.) (SW port)
    ////////////////////////////////////////////////////////////////////////////

    /// \note A Sample is always stereo, so a wider engine configuration -- which
    /// nothing produces today; activate() asks for 2 x 2 -- keeps the port.
    float const *sampleChannels[Sample::numberOfChannels];
    std::unique_lock<Utility::CriticalSection> const sampleLock(processCriticalSection_,
                                                                std::try_to_lock);
    if (sampleLock.owns_lock() && sample_ && (channels <= std::size(sampleChannels)) &&
        (buffers().numberOfSideChannels() >= channels) &&
        (process->frames_count <= buffers().blockSize()))
    {
        auto const startingPosition(sample_.samplePosition());
        std::uint32_t position(startingPosition);
        for (std::uint8_t channel(0); channel < channels; ++channel)
        {
            // Every channel reads the same span, so each starts where the last
            // one did and the advance is taken once.
            position = startingPosition;
            sampleChannels[channel] =
                sampleChunk(sample_.channel(channel), position, process->frames_count,
                            buffers().sideChannel(channel).begin());
        }
        sample_.samplePosition() = position;
        sideChannels = sampleChannels;
    }

    /// \note A false return is the engine declining the block: another thread
    /// holds the processing lock, and waiting for it here is the one thing the
    /// audio thread may not do. Silence rather than the input, because this
    /// plugin reports latency -- so the input is not the dry signal, it is the
    /// dry signal arriving `latencyInSamples_` early, and a host that delay-
    /// compensates would put it where nothing belongs. A gap is honest; a burst
    /// of misaligned full-level dry is not.
    ///
    ///   §2.1b is what this was: the buffer was left untouched, so the host
    /// played back whatever the previous plugin in the chain had written into
    /// it. Every preset load did this.
    ///                                       (01.08.2026.) (SW port)
    auto const processed(SpectrumWorxCore::process(input.data32, sideChannels, output.data32, 1.0f,
                                                   process->frames_count));

    // Ports beyond what the engine is configured for are the host's to see as
    // silence, not as whatever was in the buffer -- and if nothing ran, that is
    // every port.
    for (std::uint32_t channel(processed ? channels : 0); channel < output.channel_count; ++channel)
        std::memset(output.data32[channel], 0, process->frames_count * sizeof(float));
}

void SpectrumWorxCLAP::onMainThread() noexcept
{
    auto const flags(pendingRescan_.exchange(0));
    if (flags && _host.canUseParams())
        _host.paramsRescan(static_cast<clap_param_rescan_flags>(flags));

    // What the audio thread was not allowed to do itself.
    if (pendingMarkDirty_.exchange(false) && _host.canUseState())
        _host.stateMarkDirty();

    PluginHelper::onMainThread();
}

////////////////////////////////////////////////////////////////////////////////
// Edits made in the editor
////////////////////////////////////////////////////////////////////////////////

bool SpectrumWorxCLAP::UIEdits::push(Edit const &edit) const
{
    auto const written(written_.load(std::memory_order_relaxed));
    // The consumer only ever advances read_, so a stale value here just makes
    // the queue look fuller than it is -- never emptier.
    if ((written - read_.load(std::memory_order_acquire)) >= capacity)
        return false;
    edits_[written & mask] = edit;
    written_.store(written + 1, std::memory_order_release);
    return true;
}

bool SpectrumWorxCLAP::UIEdits::pop(Edit &edit)
{
    auto const read(read_.load(std::memory_order_relaxed));
    if (read == written_.load(std::memory_order_acquire))
        return false;
    edit = edits_[read & mask];
    read_.store(read + 1, std::memory_order_release);
    return true;
}

void SpectrumWorxCLAP::flushUIEdits(clap_output_events const *const out)
{
    UIEdits::Edit edit;
    while (uiEdits_.pop(edit))
    {
        clap_event_param_gesture gesture{};
        clap_event_param_value value{};
        clap_event_header *pHeader{nullptr};

        if (edit.kind == UIEdits::Kind::Value)
        {
            value.header.size = sizeof(value);
            value.header.time = 0;
            value.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
            value.header.type = CLAP_EVENT_PARAM_VALUE;
            value.header.flags = 0;
            value.param_id = edit.id;
            value.cookie = nullptr;
            value.note_id = value.port_index = value.channel = value.key = -1;
            value.value = edit.value;
            pHeader = &value.header;
        }
        else
        {
            gesture.header.size = sizeof(gesture);
            gesture.header.time = 0;
            gesture.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
            gesture.header.type = (edit.kind == UIEdits::Kind::GestureBegin)
                                      ? CLAP_EVENT_PARAM_GESTURE_BEGIN
                                      : CLAP_EVENT_PARAM_GESTURE_END;
            gesture.header.flags = 0;
            gesture.param_id = edit.id;
            pHeader = &gesture.header;
        }

        out->try_push(out, pHeader);
    }
}

////////////////////////////////////////////////////////////////////////////////
// What the editor tells the host.
//
// \note All of it queues rather than calls. These run on the UI thread, and a
// host takes parameter changes only through the output event list it hands to
// process() or flush().
////////////////////////////////////////////////////////////////////////////////

/// \note The editor works in the effect's own units throughout -- a knob knows
/// its parameter's real range -- so an edit it made is normalised here, on the
/// way out, and nowhere else.
void SpectrumWorxCLAP::HostProxy::automatedParameterChanged(
    ParameterSelector const parameter, Plugins::AutomatedParameterValue const value) const
{
    ParameterID const parameterID{parameter};
    Plugins::ParameterInformation<Protocol> ranges;
    plugin_.liveRanges(parameterID, ranges);

    plugin_.uiEdits_.push({parameter.value,
                           static_cast<Plugins::AutomatedParameterValue>(
                               CLAPEdge::toHost(parameterID, ranges, value)),
                           UIEdits::Kind::Value});

    /// \note The same rescan handleEvent() asks for when the *host* fills a slot.
    /// A slot selector is the one parameter whose value changes what the others
    /// are called and what they mean, and it can be moved from either side; the
    /// rescan was only wired to the host's side, so a module added from the
    /// plugin's own UI left every one of that slot's parameters showing the name
    /// it was first read with.
    ///                                       (29.07.2026.) (SW port)
    if (parameterID.type() == ParameterID::ModuleChainParameter)
        const_cast<SpectrumWorxCLAP &>(plugin_).requestRescan(
            CLAP_PARAM_RESCAN_INFO | CLAP_PARAM_RESCAN_TEXT | CLAP_PARAM_RESCAN_VALUES);

    plugin_.markCurrentProgramAsModified();
    plugin_.requestParameterFlush();
}

void SpectrumWorxCLAP::HostProxy::automatedParameterBeginEdit(
    ParameterSelector const parameter) const
{
    plugin_.uiEdits_.push({parameter.value, 0, UIEdits::Kind::GestureBegin});
    plugin_.requestParameterFlush();
}

void SpectrumWorxCLAP::HostProxy::automatedParameterEndEdit(ParameterSelector const parameter) const
{
    plugin_.uiEdits_.push({parameter.value, 0, UIEdits::Kind::GestureEnd});
    plugin_.requestParameterFlush();
}

/// \note The `canUseParams()` guard is not optional and these three had it
/// missing -- the same bug as markCurrentProgramAsModified()'s thread check, at
/// three more sites, found by the audit that note recommends rather than by a
/// test. `clap_host_params` is an *optional* extension; clap-helpers'
/// `paramsRequestFlush()` is `assert( canUseParams() ); _hostParams->request_flush( … );`.
/// A host that offers no parameters at all still gets the queued edit; it simply
/// does not get told to come and collect it, which is all it could do with the
/// news anyway.
///                                           (01.08.2026.) (SW port)
void SpectrumWorxCLAP::requestParameterFlush() const
{
    if (!_host.canUseParams())
        return;
    const_cast<SpectrumWorxCLAP &>(*this)._host.paramsRequestFlush();
}

/// \note A whole program is about to be swapped in, so the host should expect
/// every value to move at once. Nothing to announce up front -- CLAP has no
/// "hold on" call, and the rescan at the other end is what a host acts on.
void SpectrumWorxCLAP::HostProxy::presetChangeBegin() const {}

/// \note INFO as well as VALUES and TEXT. A preset replaces the module chain,
/// so what the parameters are *called* and which module path they sit under
/// both move, not only what they read -- which is RESCAN_INFO's own case. The
/// count does not move (see rebuildParameterIDs), so this is legal while the
/// plugin is active, unlike RESCAN_ALL.
///
/// \note `[main-thread]`: reached from the editor's preset browser, and the
/// editor runs on the main thread.
void SpectrumWorxCLAP::HostProxy::presetChangeEnd() const
{
    if (plugin_._host.canUseParams())
        plugin_._host.paramsRescan(CLAP_PARAM_RESCAN_INFO | CLAP_PARAM_RESCAN_VALUES |
                                   CLAP_PARAM_RESCAN_TEXT);
    plugin_.markCurrentProgramAsModified();
}

bool SpectrumWorxCLAP::HostProxy::reportNewLatencyInSamples(unsigned int const latency) const
{
    auto &plugin(const_cast<SpectrumWorxCLAP &>(plugin_));
    plugin.latencyInSamples_ = latency;
    if (!plugin_._host.canUseLatency())
        return false;
    plugin._host.latencyChanged();
    return true;
}

/// \note `clap_host_state.mark_dirty` is `[main-thread]`, and this is reached
/// from both threads: the editor calls it on the UI thread, and a host parameter
/// event calls it from process() -- the interop layer marks the program modified
/// for *any* automated change, without knowing where the change came from.
/// clap-validator fails six of its parameter tests on exactly that.
///
///   So the audio thread only records that it wants to; onMainThread() does it.
/// The same deferral the rescan flags already use, for the same reason.
///
/// \note `canUseThreadCheck()` is not decoration either. `clap.thread-check` is
/// an *optional* extension, and clap-helpers' `HostProxy::isMainThread()` is
/// `assert( canUseThreadCheck() ); return _hostThreadCheck->is_main_thread( … );`
/// -- so asking a host that does not offer it is an assertion in a checked build
/// and a null dereference in a shipping one, on a path every parameter write
/// reaches. A host that cannot say which thread this is gets the deferral, which
/// is correct from either: `request_callback` is `[thread-safe]` and
/// `mark_dirty` then happens where it is allowed to.
///                                           (01.08.2026.) (SW port)
void SpectrumWorxCLAP::markCurrentProgramAsModified() const
{
    if (!_host.canUseState())
        return;

    auto &plugin(const_cast<SpectrumWorxCLAP &>(*this));

    if (_host.canUseThreadCheck() && _host.isMainThread())
    {
        plugin._host.stateMarkDirty();
        return;
    }

    if (!pendingMarkDirty_.exchange(true))
        plugin._host.requestCallback();
}

////////////////////////////////////////////////////////////////////////////////
// State
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
///
///   The preset serialisation, plus a `<dawExtraState>` block. Not a format of
/// its own, which is what this was until 02.08.2026: `SWX1` followed by 286
/// `(uint32 id, double value)` pairs, keyed on `SW::ParameterID` -- which means
/// "slot 3's 4th parameter" and not "Convolver's Wet". That survived a reload
/// and nothing else. It could not be versioned against a changing effect list,
/// and it could not hold anything that is not a parameter, which is why the
/// sample a session had loaded did not come back.
///
///   What a preset already solved and this now inherits: keys that are names,
/// so an effect list that moves does not silently re-point them; a `Format`
/// stamp; a reader for every file the plugin has ever written; and `Sample`,
/// which has been in the format since 2011.
///
/// \note Natural units, not CLAPEdge's 0..1 -- as before, and now because the
/// preset format says so rather than because this code chose it. The edge exists
/// because a *host* may not see a range move; a file has no such problem, and
/// storing natural units means the state does not encode the edge policy and so
/// survives a change to it.
///
////////////////////////////////////////////////////////////////////////////////

bool SpectrumWorxCLAP::stateSave(clap_ostream const *const stream) noexcept
{
    /// \note `withDawExtraState`, which a `.swp` does not get. The block is
    /// empty today -- see installDawExtraStateHooks() -- and written anyway, so
    /// that "a session is a preset plus somewhere to put the rest" is a property
    /// of the bytes rather than a plan.
    auto const dawExtraState(sessionState());
    auto const state(savePreset(currentSampleFile(), juce::String(), program_, &dawExtraState));

    /// \note The terminator goes into the stream, because loadFrom() parses a
    /// C string and a host is free to hand back exactly what it was given with
    /// nothing after it.
    return writeFully(stream, state.c_str(), state.size() + 1);
}

bool SpectrumWorxCLAP::stateLoad(clap_istream const *const stream) noexcept
{
    auto state(readWholeStream(stream));
    if (!state)
        return false;

    /// \note `pEditor_`, which is null unless a window happens to be open. A
    /// host restores state before it ever shows an editor, and with the window
    /// shut for the rest of the session; the same call serves both because the
    /// consumer takes the editor as a pointer.
    ///
    /// \note And `ignoreExternalSample` false: the browser's toggle is a
    /// question about somebody else's preset, and this is the session's own
    /// state, where the sample is exactly the thing that has been getting lost.
    auto const dawExtraState(sessionState());
    if (!GUI::loadPreset(*this, pEditor_, state->data(), false /*ignoreExternalSample*/, nullptr,
                         nullptr, &dawExtraState))
        return false;

    // Already on the main thread here.
    if (_host.canUseParams())
        _host.paramsRescan(CLAP_PARAM_RESCAN_INFO | CLAP_PARAM_RESCAN_TEXT |
                           CLAP_PARAM_RESCAN_VALUES);
    return true;
}

////////////////////////////////////////////////////////////////////////////////
///
/// SpectrumWorxCLAP::sessionState()
/// --------------------------------
///
////////////////////////////////////////////////////////////////////////////////
///
/// \brief Where session state that is not a parameter goes.
///
///   Empty, and deliberately so. The mechanism is what item 4 owed; the payload
/// is a list that will grow one bullet at a time, and guessing at it now would
/// be inventing a schema for settings nobody has asked to persist yet.
///
///   The first three candidates, all `[main-thread]` and none of them
/// parameters:
///
///   - `loadLastSession_`, whose own note on the declaration says the session
///     state a host hands back is a better home for it than the settings file
///     this plugin does not have;
///   - the preset browser's location and selection -- week_two.md §2.7's "the
///     browser does not remember where it was", for the session case;
///   - the interface settings (opacity, mouse-over reaction, LFO update
///     behaviour, hide-cursor-on-knob-drag), which the CLAP build persists
///     nowhere at all. Those are arguably user preferences rather than session
///     state, and sst-plugininfra's userdefaults.h is the other candidate home;
///     the two are not exclusive and surge uses both.
///
////////////////////////////////////////////////////////////////////////////////

DawExtraState SpectrumWorxCLAP::sessionState() const
{
    return {[](TiXmlElement &) {}, [](TiXmlElement const &) {}};
}

////////////////////////////////////////////////////////////////////////////////
// Editor
////////////////////////////////////////////////////////////////////////////////

/// \note The shim owns what this returns and destroys it before this plugin.
/// The editor registers and deregisters itself through EditorHost, which is why
/// this does not have to wrap it -- SpectrumWorxEditor is final anyway.
std::unique_ptr<juce::Component> SpectrumWorxCLAP::createEditor()
{
    return std::make_unique<GUI::SpectrumWorxEditor>(*this);
}

void SpectrumWorxCLAP::editorOpened(GUI::SpectrumWorxEditor &editor) { pEditor_ = &editor; }
void SpectrumWorxCLAP::editorClosed() { pEditor_ = nullptr; }

////////////////////////////////////////////////////////////////////////////////
//
// SpectrumWorxCLAP::setNewSample()
// --------------------------------
//
////////////////////////////////////////////////////////////////////////////////
///
/// \note `[main-thread]`, and synchronous: it decodes the whole file here and
/// takes the process lock only to swap the result in. An MP3 of the size the
/// factory samples are is single-digit milliseconds; a long file the user picks
/// is not, and stalling the message thread is the cost of not having a loader
/// thread. That is a deliberate deferral -- see the note on the declaration and
/// doc/tech/week_two.md §1 item 3 -- and not something to fix here, because the
/// answer is a queue this plugin does not have yet.
///
///   Two things the 2016 worker did that are gone with the buffers they served:
/// InputBuffers::forceSideChannel() and a resize() around the load. activate()
/// asks for two main and two side channels outright, so the side buffers this
/// writes into are already there whether a sample is loaded or not.
///
////////////////////////////////////////////////////////////////////////////////

void SpectrumWorxCLAP::setNewSample(juce::File const &newSampleFile)
{
    if (newSampleFile == juce::File())
    {
        Utility::CriticalSectionLock const processingLock(getProcessingLock());
        sample_.clear();
        clearSideChannelData();
        return;
    }

    /// \note This plugin's own rate rather than the engine's, and zero is a
    /// legal answer: a host can restore a session -- sample and all -- before it
    /// has ever activated, and Sample::load() reads a file at its own rate when
    /// it is given no other. activate() then re-reads it. Refusing would be the
    /// alternative, and it would silently lose the sample.
    auto const *const pErrorMessage(sample_.load(
        newSampleFile, static_cast<unsigned int>(sampleRate_), processCriticalSection_));
    if (pErrorMessage)
        GUI::warningMessageBox("SpectrumWorx: error loading selected sample file.", pErrorMessage,
                               false);

    /// \note And now it *is* dirty. This said "deliberately no
    /// markCurrentProgramAsModified()" until 02.08.2026, because the state was
    /// `(id, value)` pairs and could not hold a file name, so telling a host the
    /// session had changed would have promised to remember something the format
    /// could not. State is the preset serialisation now and `<p n="Sample">` has
    /// been in that since 2011, so the promise is one this can keep.
    markCurrentProgramAsModified();
}

bool SpectrumWorxCLAP::registerOrUnregisterTimer(clap_id &id, int const milliseconds,
                                                 bool const registering)
{
    if (!_host.canUseTimerSupport())
        return false;
    if (registering)
        _host.timerSupportRegister(milliseconds, &id);
    else
        _host.timerSupportUnregister(id);
    return true;
}

bool SpectrumWorxCLAP::registerOrUnregisterPosixFd(int const fd, clap_posix_fd_flags_t const flags,
                                                   bool const registering)
{
    if (!_host.canUsePosixFdSupport())
        return false;
    return registering ? _host.posixFdSupportRegister(fd, flags)
                       : _host.posixFdSupportUnregister(fd);
}

//------------------------------------------------------------------------------
} // namespace LE::SW
//------------------------------------------------------------------------------
