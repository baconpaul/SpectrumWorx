////////////////////////////////////////////////////////////////////////////////
///
/// \file parameters.hpp
/// --------------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef parameters_hpp__390E5B0C_2423_463D_BAF5_4222DF433830
#define parameters_hpp__390E5B0C_2423_463D_BAF5_4222DF433830
//------------------------------------------------------------------------------
#include "automatableParameters.hpp"

#include "le/parameters/linear/parameter.hpp"
#include "le/parameters/factoryMacro.hpp"

namespace LE
{
namespace SW
//------------------------------------------------------------------------------
{
namespace GlobalParameters // Automated parameters
//------------------------------------------------------------------------------
{
using FFTSize = Engine::FFTSize;
#if LE_SW_ENGINE_WINDOW_PRESUM
using WindowSizeFactor = Engine::WindowSizeFactor;
#endif // LE_SW_ENGINE_WINDOW_PRESUM
using OverlapFactor = Engine::OverlapFactor;
using WindowFunction = Engine::WindowFunction;

//LE_ENUMERATED_PARAMETER(StreamMode, Always, MIDITrigger, MIDIGate); // ...MIDI not supported yet

/// The parameter list is a comma separated one now, so a conditional member of
/// it carries its own separator.
#if LE_SW_ENGINE_WINDOW_PRESUM
#define LE_SW_WINDOW_SIZEFACTOR_PARAMETER() , WindowSizeFactor
#else
#define LE_SW_WINDOW_SIZEFACTOR_PARAMETER()
#endif // LE_SW_ENGINE_WINDOW_PRESUM

using LE::Parameters::Traits::Default;
using LE::Parameters::Traits::Maximum;
using LE::Parameters::Traits::Minimum;
using LE::Parameters::Traits::ValuesDenominator;

LE_DEFINE_PARAMETER(InputGain, LE::Parameters::LinearFloat, Minimum<1>, Maximum<2000>,
                    Default<1000>, ValuesDenominator<1000>);
LE_DEFINE_PARAMETER(OutputGain, InputGain);
LE_DEFINE_PARAMETER(MixPercentage, LE::Parameters::LinearFloat, Minimum<0>, Maximum<1>, Default<1>);

LE_DEFINE_PARAMETERS(InputGain, OutputGain, MixPercentage, FFTSize, OverlapFactor,
                     WindowFunction LE_SW_WINDOW_SIZEFACTOR_PARAMETER()
                     //, StreamMode // ...MIDI not supported yet
);

#undef LE_SW_WINDOW_SIZEFACTOR_PARAMETER
//------------------------------------------------------------------------------
} // namespace GlobalParameters
//------------------------------------------------------------------------------
} // namespace SW

namespace Parameters
{

////////////////////////////////////////////////////////////////////////////////
//
// Global parameters UI elements definitions.
//
////////////////////////////////////////////////////////////////////////////////

UI_NAME(SW::GlobalParameters::InputGain, "In")
UI_NAME(SW::GlobalParameters::OutputGain, "Out")
UI_NAME(SW::GlobalParameters::MixPercentage, "Mix")
//UI_NAME( SW::GlobalParameters::StreamMode, "Sampler streaming mode" ) // ...MIDI not supported yet

} // namespace Parameters

} // namespace LE

#endif // parameters_hpp
