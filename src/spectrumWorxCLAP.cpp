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
    clapJuceShim_ = std::make_unique<sst::clap_juce_shim::ClapJuceShim>(this);
    clapJuceShim_->setResizable(false);
}

SpectrumWorxCLAP::~SpectrumWorxCLAP() = default;

bool SpectrumWorxCLAP::init() noexcept { return true; }

bool SpectrumWorxCLAP::activate(double const sampleRate, std::uint32_t, std::uint32_t) noexcept
{
    sampleRate_ = sampleRate;
    return true;
}

void SpectrumWorxCLAP::deactivate() noexcept { sampleRate_ = 0; }

void SpectrumWorxCLAP::reset() noexcept {}

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
    info->in_place_pair = CLAP_INVALID_ID;

    if (isInput && index == 0)
    {
        info->id = mainInputPort;
        info->flags = CLAP_AUDIO_PORT_IS_MAIN;
        info->in_place_pair = mainOutputPort;
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
        info->in_place_pair = mainInputPort;
        std::strncpy(info->name, "Main Out", CLAP_NAME_SIZE - 1);
        return true;
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////////
// Parameters
////////////////////////////////////////////////////////////////////////////////

bool SpectrumWorxCLAP::isValidParamId(clap_id const id) const noexcept
{
    return parameters_.byID(id) != nullptr;
}

std::uint32_t SpectrumWorxCLAP::paramsCount() const noexcept { return parameters_.count(); }

bool SpectrumWorxCLAP::paramsInfo(std::uint32_t const index,
                                  clap_param_info *const info) const noexcept
{
    return parameters_.info(index, info);
}

bool SpectrumWorxCLAP::paramsValue(clap_id const id, double *const value) noexcept
{
    if (!parameters_.byID(id))
        return false;
    *value = parameters_.value(id);
    return true;
}

bool SpectrumWorxCLAP::paramsValueToText(clap_id const id, double const value, char *const display,
                                         std::uint32_t const size) noexcept
{
    return parameters_.valueToText(id, value, display, size);
}

bool SpectrumWorxCLAP::paramsTextToValue(clap_id const id, char const *const display,
                                         double *const value) noexcept
{
    return parameters_.textToValue(id, display, value);
}

bool SpectrumWorxCLAP::handleEvent(clap_event_header const *const header)
{
    if (header->space_id != CLAP_CORE_EVENT_SPACE_ID)
        return false;
    if (header->type != CLAP_EVENT_PARAM_VALUE)
        return false;

    auto const *const event(reinterpret_cast<clap_event_param_value const *>(header));
    return parameters_.setValue(event->param_id, event->value);
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
        requestRescan(CLAP_PARAM_RESCAN_INFO | CLAP_PARAM_RESCAN_TEXT);

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
        requestRescan(CLAP_PARAM_RESCAN_INFO | CLAP_PARAM_RESCAN_TEXT);

    if (process->out_events)
        flushUIEdits(process->out_events);

    // Stage 1 passes audio through untouched; the engine lands in stage 3.
    if (process->audio_inputs_count > 0 && process->audio_outputs_count > 0)
    {
        auto const &input(process->audio_inputs[0]);
        auto &output(process->audio_outputs[0]);
        auto const channels(std::min(input.channel_count, output.channel_count));
        for (std::uint32_t channel(0); channel < channels; ++channel)
        {
            if (!input.data32 || !output.data32)
                break;
            if (input.data32[channel] != output.data32[channel])
                std::memcpy(output.data32[channel], input.data32[channel],
                            process->frames_count * sizeof(float));
        }
        for (std::uint32_t channel(channels); channel < output.channel_count; ++channel)
            if (output.data32)
                std::memset(output.data32[channel], 0, process->frames_count * sizeof(float));
    }

    return CLAP_PROCESS_CONTINUE;
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

void SpectrumWorxCLAP::flushUIEdits(clap_output_events const *const out)
{
    auto dirty(uiEditedSlots_.exchange(0));
    while (dirty != 0)
    {
        auto const slot(static_cast<std::uint8_t>(std::countr_zero(dirty)));
        dirty &= static_cast<std::uint32_t>(dirty - 1);

        clap_event_param_value event{};
        event.header.size = sizeof(event);
        event.header.time = 0;
        event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        event.header.type = CLAP_EVENT_PARAM_VALUE;
        event.header.flags = 0;
        event.param_id = packParameterID(ParameterType::ModuleChain, slot, 0, 0);
        event.cookie = nullptr;
        event.note_id = -1;
        event.port_index = -1;
        event.channel = -1;
        event.key = -1;
        event.value = parameters_.value(event.param_id);
        out->try_push(out, &event.header);
    }
}

void SpectrumWorxCLAP::cycleModuleFromUI(std::uint8_t const moduleIndex)
{
    if (moduleIndex >= Skeleton::modules)
        return;

    // Main thread. The audio thread only reads these five doubles, and only to
    // copy them back out to the host, so a plain store is good enough here.
    parameters_.cycleEffectIn(moduleIndex);
    uiEditedSlots_.fetch_or(std::uint32_t{1} << moduleIndex);

    if (_host.canUseParams())
    {
        _host.paramsRescan(CLAP_PARAM_RESCAN_INFO | CLAP_PARAM_RESCAN_TEXT);
        _host.paramsRequestFlush();
    }
}

void SpectrumWorxCLAP::requestRescanFromUI()
{
    if (_host.canUseParams())
        _host.paramsRescan(CLAP_PARAM_RESCAN_INFO | CLAP_PARAM_RESCAN_TEXT |
                           CLAP_PARAM_RESCAN_VALUES);
}

////////////////////////////////////////////////////////////////////////////////
// State
////////////////////////////////////////////////////////////////////////////////

bool SpectrumWorxCLAP::stateSave(clap_ostream const *const stream) noexcept
{
    auto const &values(parameters_.values());
    auto const count(static_cast<std::uint32_t>(values.size()));

    if (!writeFully(stream, stateMagic, sizeof(stateMagic)))
        return false;
    if (!writeFully(stream, &count, sizeof(count)))
        return false;
    return writeFully(stream, values.data(), values.size() * sizeof(double));
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
    if (count != parameters_.count())
        return false;

    std::vector<double> values(count);
    if (!readFully(stream, values.data(), values.size() * sizeof(double)))
        return false;

    parameters_.setValues(values);

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
