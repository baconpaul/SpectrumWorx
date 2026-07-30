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

#include <sst/plugininfra/cpufeatures.h>
#include <sst/plugininfra/version_information.h>

#include <algorithm>
#include <cstring>
//------------------------------------------------------------------------------
namespace LE::SW
{
//------------------------------------------------------------------------------
namespace
{
constexpr clap_id mainInputPort{0};
constexpr clap_id sideChainInputPort{1};
constexpr clap_id mainOutputPort{2};

constexpr char stateMagic[4]{'S', 'W', 'X', '1'};

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

bool readFully(clap_istream const *const stream, void *const data, std::size_t size)
{
    auto *cursor(static_cast<char *>(data));
    while (size > 0)
    {
        auto const read(stream->read(stream, cursor, size));
        if (read <= 0)
            return false;
        cursor += read;
        size -= static_cast<std::size_t>(read);
    }
    return true;
}
} // namespace

clap_plugin_descriptor const *descriptor()
{
    static char const *features[]{CLAP_PLUGIN_FEATURE_AUDIO_EFFECT, CLAP_PLUGIN_FEATURE_STEREO,
                                  "spectral", nullptr};
    static clap_plugin_descriptor const description{
        CLAP_VERSION,
        "com.littleendian.spectrumworx",
        "SpectrumWorx",
        "Little Endian Ltd",
        "https://github.com/baconpaul/SpectrumWorx",
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
    Plugins::ParameterInformation<Protocol> ranges;
    liveRanges(parameterID, ranges);

    /// \note The printer works in the effect's own units, so the host's value
    /// comes back off the 0..1 edge before it is formatted. This is what keeps a
    /// normalised parameter readable: the range the host sees is meaningless, and
    /// the text is where the real number -- or the enumerated name -- shows up.
    auto const automationValue(CLAPEdge::fromHost(parameterID, ranges, value));
    std::array<char, 128> text{};
    getParameterDisplay(parameterID, {text.data(), text.size()}, &automationValue);

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

    runEngine(process);

    return CLAP_PROCESS_CONTINUE;
}

void SpectrumWorxCLAP::runEngine(clap_process const *const process) noexcept
{
    if ((process->audio_inputs_count == 0) || (process->audio_outputs_count == 0))
        return;

    auto const &input(process->audio_inputs[0]);
    auto &output(process->audio_outputs[0]);
    if (!input.data32 || !output.data32)
        return; // 64 bit hosts get silence rather than a crash until 5.7.

    auto const channels(engineSetup().numberOfChannels());
    if ((input.channel_count < channels) || (output.channel_count < channels))
        return;

    // The engine reads a side channel whenever the input mode calls for one, and
    // does not check that the host actually connected the port.
    auto const *const sideChannels(
        ((process->audio_inputs_count > 1) && process->audio_inputs[1].data32)
            ? process->audio_inputs[1].data32
            : input.data32);

    if (!engineRunning_)
        return;

    SpectrumWorxCore::process(input.data32, sideChannels, output.data32, 1.0f,
                              process->frames_count);

    // Ports beyond what the engine is configured for are the host's to see as
    // silence, not as whatever was in the buffer.
    for (std::uint32_t channel(channels); channel < output.channel_count; ++channel)
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
    plugin_._host.paramsRequestFlush();
}

void SpectrumWorxCLAP::HostProxy::automatedParameterBeginEdit(
    ParameterSelector const parameter) const
{
    plugin_.uiEdits_.push({parameter.value, 0, UIEdits::Kind::GestureBegin});
    plugin_._host.paramsRequestFlush();
}

void SpectrumWorxCLAP::HostProxy::automatedParameterEndEdit(ParameterSelector const parameter) const
{
    plugin_.uiEdits_.push({parameter.value, 0, UIEdits::Kind::GestureEnd});
    plugin_._host.paramsRequestFlush();
}

/// \note A whole program is about to be swapped in, so the host should expect
/// every value to move at once. Nothing to do until presets exist; the rescan
/// at the end of stateLoad() is the equivalent for the state path.
void SpectrumWorxCLAP::HostProxy::presetChangeBegin() const {}

void SpectrumWorxCLAP::HostProxy::presetChangeEnd() const
{
    if (plugin_._host.canUseParams())
        plugin_._host.paramsRescan(CLAP_PARAM_RESCAN_VALUES | CLAP_PARAM_RESCAN_TEXT);
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
void SpectrumWorxCLAP::markCurrentProgramAsModified() const
{
    if (!_host.canUseState())
        return;

    auto &plugin(const_cast<SpectrumWorxCLAP &>(*this));

    if (_host.isMainThread())
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

/// \note Not the preset format, and not stage 5.6. This writes the current
/// parameter list as (id, value) pairs, which is enough to survive a session
/// and a reload. What it is not is *durable*: nothing here is versioned against
/// a changing effect list, and the real thing goes through the preset
/// serialisation that LE_NO_PRESETS still compiles out of the engine. Stage 8
/// splits that out; until then this beats forgetting everything.
///                                       (29.07.2026.) (SW port)

/// \note Values here are the engine's own, not CLAPEdge's 0..1 -- deliberately.
/// The edge exists because a *host* may not see a range move; a file has no such
/// problem, and storing natural units means the state does not encode the edge
/// policy and so survives a change to it. Save and load use the same pair of
/// accessors, so the two stay consistent by construction.
bool SpectrumWorxCLAP::stateSave(clap_ostream const *const stream) noexcept
{
    rebuildParameterIDs();

    auto const count(static_cast<std::uint32_t>(parameterIDs_.size()));
    if (!writeFully(stream, stateMagic, sizeof(stateMagic)))
        return false;
    if (!writeFully(stream, &count, sizeof(count)))
        return false;

    for (auto const id : parameterIDs_)
    {
        auto const value(static_cast<double>(getParameter(ParameterID{id})));
        if (!writeFully(stream, &id.value, sizeof(id.value)))
            return false;
        if (!writeFully(stream, &value, sizeof(value)))
            return false;
    }
    return true;
}

bool SpectrumWorxCLAP::stateLoad(clap_istream const *const stream) noexcept
{
    char magic[sizeof(stateMagic)]{};
    if (!readFully(stream, magic, sizeof(magic)))
        return false;
    if (std::memcmp(magic, stateMagic, sizeof(magic)) != 0)
        return false;

    std::uint32_t count{0};
    if (!readFully(stream, &count, sizeof(count)))
        return false;

    std::vector<std::pair<Plugins::ParameterID::value_type, double>> saved(count);
    for (auto &entry : saved)
    {
        if (!readFully(stream, &entry.first, sizeof(entry.first)))
            return false;
        if (!readFully(stream, &entry.second, sizeof(entry.second)))
            return false;
    }

    /// \note Slot selectors first, and in a second pass everything else: a
    /// module's parameters do not exist until its effect does, so applying them
    /// in file order would drop every one that belongs to a slot the load has
    /// not filled yet.
    for (auto const &[id, value] : saved)
    {
        ParameterID const parameterID{Plugins::ParameterID{id}};
        if (parameterID.type() == ParameterID::ModuleChainParameter)
            setParameter(parameterID, static_cast<Plugins::AutomatedParameterValue>(value));
    }
    /// \note And skipping, in that second pass, whatever no effect owns -- the
    /// saved file holds all 286 IDs, most of them belonging to slots that are
    /// empty in the state being restored. Writing one of those is the same
    /// out-of-range write handleEvent() guards against; see the note there.
    for (auto const &[id, value] : saved)
    {
        ParameterID const parameterID{Plugins::ParameterID{id}};
        if (parameterID.type() == ParameterID::ModuleChainParameter)
            continue;

        Plugins::ParameterInformation<Protocol> ranges;
        if (!liveRanges(parameterID, ranges))
            continue;

        setParameter(parameterID, static_cast<Plugins::AutomatedParameterValue>(value));
    }

    // Already on the main thread here.
    if (_host.canUseParams())
        _host.paramsRescan(CLAP_PARAM_RESCAN_INFO | CLAP_PARAM_RESCAN_TEXT |
                           CLAP_PARAM_RESCAN_VALUES);
    return true;
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
