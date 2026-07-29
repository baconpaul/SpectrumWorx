////////////////////////////////////////////////////////////////////////////////
///
/// spectrumWorxSharedImpl.inl
/// --------------------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "spectrumWorxSharedImpl.hpp"

#include "core/host_interop/host2PluginImpl.inl"
#include "core/host_interop/plugin2HostImpl.inl"

#include "le/math/conversion.hpp"
#include "le/plugins/plugin.hpp"
#ifdef __APPLE__
#include "le/plugins/au/tag.hpp"
#endif // __APPLE__
#include "le/plugins/vst/2.4/tag.hpp"
#include "le/utility/platformSpecifics.hpp"

#include "le/utility/assert.hpp"

#ifdef _DEBUG
#include <cstdio>
#endif // _DEBUG
#include <cstring>
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace SW
{
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// http://forum.cockos.com/showthread.php?p=538840
// http://www.kvraudio.com/forum/viewtopic.php?t=253666
// http://www.cockos.com/reaper/sdk/vst/vst_ext.php
//------------------------------------------------------------------------------

/// \note AUs must:
/// - allow access to and change of parameters and presets even while
///   uninitialised so we perform path initialisation early, in the constructor
/// - not change parameters in the process of initialisation so we have to avoid
///   the 'load last session on startup' procedure in that case.
///                                           (06.03.2013.) (Domagoj Saric)

template <class Impl, class Protocol>
SpectrumWorxSharedImpl<Impl, Protocol>::SpectrumWorxSharedImpl(
    typename PluginPlatform::ConstructionParameter const pluginBaseParam)
    : PluginPlatform(pluginBaseParam)
#ifndef LE_SW_FMOD
#ifdef __APPLE__
      ,
      Base(std::is_same<typename PluginPlatform::Protocol, Plugins::Protocol::AU>::value)
#else
      ,
      Base(false)
#endif // __APPLE__
#endif // LE_SW_FMOD
{
#if defined(__APPLE__) && !defined(LE_SW_FMOD)
    if (std::is_same<typename PluginPlatform::Protocol, Plugins::Protocol::AU>::value)
        LE_VERIFY(GUI::initializePaths());
#endif // __APPLE__ && !LE_SW_FMOD
}

template <class Impl, class Protocol>
typename Plugins::ErrorCode<Protocol>::value_type
SpectrumWorxSharedImpl<Impl, Protocol>::initialise()
{
#ifdef __APPLE__
    if (!std::is_same<typename PluginPlatform::Protocol, Plugins::Protocol::AU>::value)
#endif // __APPLE__
        LE_VERIFY(GUI::initializePaths());
    return Base::initialise() ? Plugins::ErrorCode<Protocol>::Success
                              : Plugins::ErrorCode<Protocol>::OutOfMemory;
}

template <class Impl, class Protocol>
void SpectrumWorxSharedImpl<Impl, Protocol>::process(float const *const *const inputs,
                                                     float **const outputs,
                                                     std::uint32_t const samples)
{
    if (!updateTimingInformation())
        this->updatePosition(samples);

    Base::process(inputs, outputs, samples);
}

template <class Impl, class Protocol>
bool SpectrumWorxSharedImpl<Impl, Protocol>::updateTimingInformation()
{
    using namespace Plugins::Protocol;
    using TimingInfo = typename PluginPlatform::TimingInformation;

    std::uint32_t const wantedFields(TimingInfo::NumberOfBeats | TimingInfo::BPM |
                                     TimingInfo::TimeSignature);
    auto const timeInfo(this->host().template getTimeInfo<wantedFields>());
    if (!timeInfo.template hasFields<wantedFields>())
        return false;

    LE_ASSERT_MSG(
        (!timeInfo.template hasField<TimingInfo::SampleRate>()) ||
            (timeInfo.sampleRate() == this->uncheckedEngineSetup().template sampleRate<float>()),
        "Inconsistent TimingInfo and EngineSetup sampleRate.");

    double const beatsPerBar(Math::convert<double>(timeInfo.timeSignatureNumerator()));
    double const beatDuration(60 / timeInfo.bpm());

    double const positionInBars(timeInfo.numberOfBeats() / beatsPerBar);
    double const barDuration(beatsPerBar * beatDuration);

    this->handleTimingInformationChange(this->updatePositionAndTimingInformation(
        static_cast<float>(positionInBars), static_cast<float>(barDuration),
        timeInfo.timeSignatureNumerator()));

    return true;
}

//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
