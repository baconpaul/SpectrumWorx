////////////////////////////////////////////////////////////////////////////////
///
/// baseParametersUIElements.cpp
/// ----------------------------
///
///   The out-of-line half of baseParameters.hpp's UIElements: one function,
/// here because it reads the engine's Setup and the parameter layer does not
/// include the engine. Everything else the base parameters declare -- their
/// names, and which DisplayValueTransformer answers for which -- is in
/// baseParameters.hpp beside the parameters, where every translation unit that
/// builds the parameter table can see it.
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "baseParameters.hpp"

#include "le/spectrumworx/engine/setup.hpp"
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace Parameters
{
float DisplayValueTransformer<SW::Effects::BaseParameters::StartFrequency>::transform(
    float const &value, SW::Engine::Setup const &engineSetup)
{
    return engineSetup.normalisedFrequencyToHz(value);
}
} // namespace Parameters
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
