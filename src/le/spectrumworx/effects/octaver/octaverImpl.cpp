////////////////////////////////////////////////////////////////////////////////
///
/// octaverImpl.cpp
/// ---------------
///
/// Copyright (c) 2010 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "octaverImpl.hpp"

#include "le/spectrumworx/engine/channelData.hpp"
#include "le/spectrumworx/engine/setup.hpp"
#include "le/math/conversion.hpp"
#include "le/math/math.hpp"
#include "le/math/dft/domainConversion.hpp"
#include "le/math/vector.hpp"

#include "le/utility/stackBuffer.hpp"

namespace LE::SW::Effects
{

// Discussions:
// http://old.nabble.com/%22octave-up%22-algorithm--td16703666.html
//
// 3rd party end products:
// - commercial:
//   http://www.waves.com/content.aspx?id=327

////////////////////////////////////////////////////////////////////////////////
//
// Octaver static member definitions.
//
////////////////////////////////////////////////////////////////////////////////

char const Octaver::title[] = "Octaver";
char const Octaver::description[] = "Adds two octaves.";

////////////////////////////////////////////////////////////////////////////////
//
// OctaverImpl::setup()
// --------------------
//
////////////////////////////////////////////////////////////////////////////////

void OctaverImpl::setup(IndexRange const &workingRange, Engine::Setup const &engineSetup)
{
    ps_.setup(engineSetup);

    octaveParameters_[0].gain = Math::dB2NormalisedLinear(parameters().get<GainOctave1>());
    octaveParameters_[1].gain = Math::dB2NormalisedLinear(parameters().get<GainOctave2>());
    octaveParameters_[0].pitchScale =
        Math::octaves2Interval12TET(parameters().get<Octave1>().getValue() - 2);
    octaveParameters_[1].pitchScale =
        Math::octaves2Interval12TET(parameters().get<Octave2>().getValue() - 2);

    { // adjust cutoff:
        std::uint16_t cutoff(engineSetup.frequencyInHzToBin(parameters().get<CutoffFrequency>()));
        // put the cutoff within the working range:
        std::uint16_t const minimum(workingRange.begin());
        std::uint16_t const maximum(workingRange.end());
        cutoff = std::max(cutoff, minimum);
        cutoff = std::min(cutoff, maximum);
        // make the cutoff relative to the working range:
        cutoff -= minimum;
        cutoff_ = cutoff;
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// OctaverImpl::process()
// ----------------------
//
////////////////////////////////////////////////////////////////////////////////

void OctaverImpl::process(ChannelState &cs, Engine::ChannelData_AmPh2ReIm data,
                          Engine::Setup const &engineSetup) const
{
    using namespace Engine;
    using namespace Math;

    ChannelData_AmPh const &inputData(data.input.main());
    ChannelData_ReIm &outputData(data.output);

    // Transform into the "ReIm domain" to enable mixing:
    amph2ReIm(inputData.full().amps().begin(), inputData.full().phases().begin(),
              outputData.full().reals().begin(), outputData.full().imags().begin(),
              engineSetup.numberOfBins());

    {
        // Allocate temporary AmPh storage for mixing:
        auto const fftSize(engineSetup.fftSize<std::uint16_t>());
        LE_ALIGNED_STACK_BUFFER(pitchShiftedStorage, char,
                                ChannelData_AmPhStorage::requiredStorage(fftSize));
        ChannelData_AmPhStorage shiftedInput(fftSize, inputData.beginBin(), inputData.endBin(),
                                             pitchShiftedStorage);

        shiftAndMix(data, shiftedInput, engineSetup, cs.pv1, 0);
        shiftAndMix(data, shiftedInput, engineSetup, cs.pv2, 1);
    }

    LE_ASSERT(cutoff_ <= outputData.numberOfBins());
    clear(outputData.reals().begin() + cutoff_, outputData.reals().end());
    clear(outputData.imags().begin() + cutoff_, outputData.imags().end());
}

////////////////////////////////////////////////////////////////////////////////
//
// OctaverImpl::shiftAndMix()
// --------------------------
//
////////////////////////////////////////////////////////////////////////////////

void OctaverImpl::shiftAndMix(Engine::ChannelData_AmPh2ReIm &data,
                              Engine::ChannelData_AmPh &shiftedData,
                              Engine::Setup const &engineSetup,
                              PhaseVocoderShared::PitchShifter::ChannelState &pvState,
                              std::uint8_t const octave) const
{
    using namespace Engine;
    using namespace Math;

    OctaveSetup const &octaveParameters(octaveParameters_[octave]);
    if (is<1>(octaveParameters.pitchScale))
        return;

    ChannelData_AmPh const &inputData(data.input.main());
    ChannelData_ReIm &outputData(data.output);

    copy(inputData.full().jointView(), shiftedData.full().jointView());
    ps_.process(octaveParameters.pitchScale, pvState, std::forward<ChannelData_AmPh>(shiftedData),
                engineSetup);
    mix(shiftedData.amps(), shiftedData.phases(), outputData.reals(), outputData.imags(),
        octaveParameters.gain, 1);
}

} // namespace LE::SW::Effects
