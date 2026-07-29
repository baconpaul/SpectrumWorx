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
#include "stubEditor.hpp"

#include "core/modules/factory.hpp"
// \note Order matters and is not alphabetical: finalImplementations.hpp defines
// Module::Impl<> and needs Module complete. factory.cpp includes them in this
// order for the same reason.
#include "core/modules/moduleDSPAndGUI.hpp"
#include "core/modules/finalImplementations.hpp"

// \note The interop templates are defined in .inl files that only their
// instantiating translation unit includes -- this one. They call through to a
// module's UI on a slot change, so the complete type is needed even though this
// plugin's gui() is always null until the editor is ported.
#include "gui/modules/moduleUI.hpp"

#include "core/host_interop/host2PluginImpl.inl"
#include "core/host_interop/plugin2HostImpl.inl"

#include <sst/plugininfra/version_information.h>

#include <algorithm>
#include <bit>
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

void SpectrumWorxCLAP::rebuildParameterIDs()
{
    /// \note The reserve in the constructor is what makes this callable from
    /// process(): the list changes when a slot's effect changes, which a host
    /// can do with an event in the middle of a block, and resizing within a
    /// reserved capacity does not allocate. maxNumberOfParameters is the bound
    /// the engine itself is built to.
    LE_ASSERT(numberOfParameters(&program()) <= parameterIDs_.capacity());
    parameterIDs_.resize(numberOfParameters(&program()));
    getParameterIDs({parameterIDs_.data(), parameterIDs_.size()}, &program());
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
    return numberOfParameters(&program());
}

bool SpectrumWorxCLAP::paramsInfo(std::uint32_t const index,
                                  clap_param_info *const info) const noexcept
{
    if (index >= parameterIDs_.size())
        return false;

    auto const id(parameterIDs_[index]);
    ParameterID const parameterID{id};

    Plugins::ParameterInformation<Protocol> properties;
    getParameterProperties(parameterID, properties, &program());

    std::memset(info, 0, sizeof(*info));
    info->id = id.value;
    info->min_value = properties.minimum();
    info->max_value = properties.maximum();
    info->default_value = properties.default_();
    info->cookie = nullptr;

    info->flags = CLAP_PARAM_IS_AUTOMATABLE;
    if (properties.isStepped())
        info->flags |= CLAP_PARAM_IS_STEPPED;
    /// \note A "meta" parameter is one whose value changes what the other
    /// parameters *are* -- which effect a slot holds, chiefly. Telling the host
    /// its value requires a rescan is exactly what this flag is for.
    if (properties.isMeta())
        info->flags |= CLAP_PARAM_REQUIRES_PROCESS;
    if (!properties.isAutomatable())
        info->flags = CLAP_PARAM_IS_READONLY;

    std::strncpy(info->name, properties.name(), CLAP_NAME_SIZE - 1);
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

bool SpectrumWorxCLAP::paramsValue(clap_id const id, double *const value) noexcept
{
    if (!isValidParamId(id))
        return false;
    *value = getParameter(ParameterID{Plugins::ParameterID{id}});
    return true;
}

bool SpectrumWorxCLAP::paramsValueToText(clap_id const id, double const value, char *const display,
                                         std::uint32_t const size) noexcept
{
    if (!isValidParamId(id))
        return false;

    std::array<char, 128> text{};
    auto const automationValue(static_cast<Plugins::AutomatedParameterValue>(value));
    getParameterDisplay(ParameterID{Plugins::ParameterID{id}}, {text.data(), text.size()},
                        &automationValue);

    std::array<char, 32> unit{};
    getParameterLabel(ParameterID{Plugins::ParameterID{id}}, {unit.data(), unit.size()},
                      &program());

    std::snprintf(display, size, "%s%s", text.data(), unit.data());
    return true;
}

bool SpectrumWorxCLAP::paramsTextToValue(clap_id const id, char const *const display,
                                         double *const value) noexcept
{
    /// \note The 2016 code never needed this: neither VST 2.4 nor AU asked a
    /// plugin to parse a typed-in value, so there is no printer inverse to call.
    /// A plain strtod covers the numeric parameters, which is what a user types
    /// into; enumerated ones fall back to the host's own list.
    if (!isValidParamId(id))
        return false;

    char *end{nullptr};
    auto const parsed(std::strtod(display, &end));
    if (end == display)
        return false;
    *value = parsed;
    return true;
}

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
    setParameter(parameterID, static_cast<Plugins::AutomatedParameterValue>(event->value));

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
    bool listChanged(false);
    for (std::uint32_t event(0); event < size; ++event)
        listChanged |= handleEvent(in->get(in, event));

    if (listChanged)
    {
        rebuildParameterIDs();
        requestRescan(CLAP_PARAM_RESCAN_INFO | CLAP_PARAM_RESCAN_TEXT);
    }

    flushUIEdits(out);
}

////////////////////////////////////////////////////////////////////////////////
// Processing
////////////////////////////////////////////////////////////////////////////////

clap_process_status SpectrumWorxCLAP::process(clap_process const *const process) noexcept
{
    bool listChanged(false);
    if (auto const *const in = process->in_events)
    {
        auto const size(in->size(in));
        for (std::uint32_t event(0); event < size; ++event)
            listChanged |= handleEvent(in->get(in, event));
    }

    if (listChanged)
    {
        rebuildParameterIDs();
        requestRescan(CLAP_PARAM_RESCAN_INFO | CLAP_PARAM_RESCAN_TEXT);
    }

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
    PluginHelper::onMainThread();
}

////////////////////////////////////////////////////////////////////////////////
// Edits made in the editor
////////////////////////////////////////////////////////////////////////////////

namespace
{
/// The module-chain parameter for a slot: "which effect is in this slot".
ParameterID moduleChainParameterID(std::uint8_t const slot)
{
    ParameterID parameterID;
    parameterID.binaryValue = 0;
    parameterID.value.type = ParameterID::ModuleChainParameter;
    parameterID.value._.moduleChain.moduleIndex = slot;
    return parameterID;
}
} // anonymous namespace

void SpectrumWorxCLAP::flushUIEdits(clap_output_events const *const out)
{
    auto dirty(uiEditedSlots_.exchange(0));
    while (dirty != 0)
    {
        auto const slot(static_cast<std::uint8_t>(std::countr_zero(dirty)));
        dirty &= static_cast<std::uint32_t>(dirty - 1);

        auto const parameterID(moduleChainParameterID(slot));

        clap_event_param_value event{};
        event.header.size = sizeof(event);
        event.header.time = 0;
        event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        event.header.type = CLAP_EVENT_PARAM_VALUE;
        event.header.flags = 0;
        event.param_id = parameterID.binaryValue;
        event.cookie = nullptr;
        event.note_id = -1;
        event.port_index = -1;
        event.channel = -1;
        event.key = -1;
        event.value = getParameter(parameterID);
        out->try_push(out, &event.header);
    }
}

/// \note The only way to change an effect from inside the plugin until the real
/// editor lands -- the stub editor drives it. It goes through setParameter like
/// any host automation would, so it exercises the same path.
void SpectrumWorxCLAP::cycleModuleFromUI(std::uint8_t const moduleIndex)
{
    if (moduleIndex >= Constants::maxNumberOfModules)
        return;

    auto const parameterID(moduleChainParameterID(moduleIndex));

    auto const current(static_cast<int>(getParameter(parameterID)));
    auto const next(static_cast<int>(current + 1) >=
                            static_cast<int>(Effects::Constants::numberOfEffects)
                        ? noModule
                        : current + 1);

    setParameter(parameterID, static_cast<Plugins::AutomatedParameterValue>(next));
    rebuildParameterIDs();
    uiEditedSlots_.fetch_or(std::uint32_t{1} << moduleIndex);

    if (_host.canUseParams())
    {
        _host.paramsRescan(CLAP_PARAM_RESCAN_INFO | CLAP_PARAM_RESCAN_TEXT);
        _host.paramsRequestFlush();
    }
}

/// \note Unreachable, and deliberately so: the only call site is guarded by
/// wantsManualDependentParameterNotifications(), which is false for CLAP. A host
/// hears about a dependent parameter moving from the CLAP_EVENT_PARAM_VALUE the
/// plugin queues into its output list, which is a better mechanism than the
/// 2016 one and does not need this. It exists because the interop template
/// names it.
///                                       (29.07.2026.) (SW port)
void SpectrumWorxCLAP::HostProxy::automatedParameterChanged(ParameterSelector,
                                                            Plugins::AutomatedParameterValue) const
{
    LE_ASSERT_MSG(false, "CLAP does not ask for manual dependent-parameter notifications.");
}

void SpectrumWorxCLAP::moduleChanged(std::uint8_t const /*moduleIndex*/,
                                     Engine::ModuleParameters const *) const
{
    //...mrmlj...const because the interop calls it from a const context; the
    //...mrmlj...rescan flag it sets is atomic, which is what makes that honest.
    const_cast<SpectrumWorxCLAP &>(*this).requestRescan(
        static_cast<clap_param_rescan_flags>(CLAP_PARAM_RESCAN_INFO | CLAP_PARAM_RESCAN_TEXT));
}

void SpectrumWorxCLAP::markCurrentProgramAsModified() const
{
    if (_host.canUseState())
        _host.stateMarkDirty();
}

std::int8_t SpectrumWorxCLAP::effectIn(std::uint8_t const slot) const
{
    return program().moduleChain().getParameterForIndex(slot);
}

void SpectrumWorxCLAP::requestRescanFromUI()
{
    rebuildParameterIDs();
    if (_host.canUseParams())
        _host.paramsRescan(CLAP_PARAM_RESCAN_INFO | CLAP_PARAM_RESCAN_TEXT |
                           CLAP_PARAM_RESCAN_VALUES);
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
    for (auto const &[id, value] : saved)
    {
        ParameterID const parameterID{Plugins::ParameterID{id}};
        if (parameterID.type() != ParameterID::ModuleChainParameter)
            setParameter(parameterID, static_cast<Plugins::AutomatedParameterValue>(value));
    }

    rebuildParameterIDs();

    // Already on the main thread here.
    if (_host.canUseParams())
        _host.paramsRescan(CLAP_PARAM_RESCAN_INFO | CLAP_PARAM_RESCAN_TEXT |
                           CLAP_PARAM_RESCAN_VALUES);
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Editor
////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<juce::Component> SpectrumWorxCLAP::createEditor()
{
    return std::make_unique<StubEditor>(*this);
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
