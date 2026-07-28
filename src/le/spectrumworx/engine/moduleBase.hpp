////////////////////////////////////////////////////////////////////////////////
///
/// \file moduleBase.hpp
///
/// Copyright (c) 2011 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef moduleBase_hpp__3A6E3E31_35A3_400F_8749_D655D2C7B921
#define moduleBase_hpp__3A6E3E31_35A3_400F_8749_D655D2C7B921
//------------------------------------------------------------------------------
#include <le/spectrumworx/effects/baseParameters.hpp>
#include <le/utility/abi.hpp>

#include "le/utility/intrusivePtr.hpp"

#include <cstdint>
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace Parameters
{
class LFO;
struct RuntimeInformation;
} // namespace Parameters
//------------------------------------------------------------------------------
namespace SW
{
//------------------------------------------------------------------------------
namespace Engine
{
//------------------------------------------------------------------------------

/// \addtogroup Engine
/// @{

class ModuleProcessor;

////////////////////////////////////////////////////////////////////////////////
///
/// \class ModuleBase
///
/// \brief Base module interface
///
/// \details An abstract interface that can be used to control all effect types
/// through a single type. This is achieved by its API that uses index-based
/// parameter access and a single type (float) to pass parameter values.
/// Additionally it offers runtime access to parameter and effect metadata.
/// <BR> <BR>
/// Index for a given parameter is obtained by a call to the
/// Module::parameterIndex() member function template.
///
////////////////////////////////////////////////////////////////////////////////

class ModuleBase
{
  public:
#ifndef DOXYGEN_ONLY
    ModuleBase(ModuleBase const &) = delete; // makes non-copyable
    ModuleBase &operator=(ModuleBase const &) = delete;
#endif // DOXYGEN_ONLY

    /// \name Basic parameters included by all effects
    /// @{
    typedef Effects::BaseParameters::Parameters BaseParameters;

    typedef Effects::BaseParameters::Bypass Bypass;
    typedef Effects::BaseParameters::Gain Gain;
    typedef Effects::BaseParameters::Wet Wet;
    typedef Effects::BaseParameters::StartFrequency StartFrequency;
    typedef Effects::BaseParameters::StopFrequency StopFrequency;
    /// @}

  public:
    /// \name Hz based/non-normalised accessors for frequency range parameters
    /// @{
    void setStartFrequencyInHz(float frequency, ModuleProcessor const &processor);
    float getStartFrequencyInHz(ModuleProcessor const &processor) const;
    void setStartFrequencyInHz(float frequency, std::uint32_t sampleRate);
    float getStartFrequencyInHz(std::uint32_t sampleRate) const;

    void setStopFrequencyInHz(float frequency, ModuleProcessor const &processor);
    float getStopFrequencyInHz(ModuleProcessor const &processor) const;
    void setStopFrequencyInHz(float frequency, std::uint32_t sampleRate);
    float getStopFrequencyInHz(std::uint32_t sampleRate) const;
    /// @}

  public:
    typedef BaseParameters Parameters;
    typedef LE::Parameters::LFO LFO;

    Parameters &baseParameters();
    Parameters const &baseParameters() const
    {
        return const_cast<ModuleBase &>(*this).baseParameters();
    }

  public:
    /// \name Parameter runtime metadata
    /// @{

    typedef LE::Parameters::RuntimeInformation ParameterInfo;

    static std::uint8_t const numberOfBaseParameters =
        BaseParameters::static_size; ///< number of base parameters that all effects inherit
    static std::uint8_t const numberOfNonLFOBaseParameters = 1 /* Bypass */
        ; ///< number of parameters that cannot be LFO-ed \details (currently only one - Bypass)
    static std::uint8_t const numberOfLFOBaseParameters =
        numberOfBaseParameters - numberOfNonLFOBaseParameters;

    std::uint8_t numberOfParameters() const
    {
        return numberOfEffectSpecificParameters() + numberOfBaseParameters;
    } ///< total number of parameters (base + effect specific)
    std::uint8_t numberOfEffectSpecificParameters()
        const; ///< number of extra parameters specific to the instantiated effect
    std::uint8_t numberOfLFOControledParameters() const
    {
        return numberOfEffectSpecificParameters() + numberOfLFOBaseParameters;
    } ///< total number of parameters that can be LFO-ed

    ParameterInfo const &parameterInfo(std::uint8_t parameterIndex) const;

    /// @}

    /// \name Parameter accessors
    /// \brief The setters return a possibly adjusted <VAR>value</VAR> (e.g. if
    /// you try to set a fractional value to an integer parameter it will be
    /// rounded).
    /// @{

    float getParameter(std::uint8_t parameterIndex) const;
    float setParameter(std::uint8_t parameterIndex, float value);

    float getBaseParameter(std::uint8_t baseParameterIndex) const;
    float setBaseParameter(std::uint8_t baseParameterIndex, float value);

    float getEffectParameter(std::uint8_t effectParameterIndex) const;
    float setEffectParameter(std::uint8_t effectParameterIndex, float value);

    /// @}

    /// \name Parameter LFO accessors
    /// @{
    LFO &lfo(std::uint8_t parameterIndex);
    LFO const &lfo(std::uint8_t const parameterIndex) const
    {
        return const_cast<ModuleBase &>(*this).lfo(parameterIndex);
    }
    /// @}

    /// \name Effect runtime metadata
    /// @{
    char const *effectName() const;
    /// @}

  public:
    /// \name Factory function
    /// @{

    typedef LE::Utility::IntrusivePtr<ModuleBase>
        Ptr; ///< shared pointer to a mutable ModuleBase instance
    typedef LE::Utility::IntrusivePtr<ModuleBase const>
        CPtr; ///< shared pointer to a const ModuleBase instance

    ////////////////////////////////////////////////////////////////////////////
    /// \brief Creates a new module for <VAR>effectName</VAR>
    /// \details The <VAR>effectName</VAR> is expected to be the effect's
    /// exact type name (e.g. "TalkingWind"), not the effect's "title" displayed
    /// to the user in the SW GUI (e.g. "Talking Wind")
    /// \return A ModuleBase instance (or null in case of a memory allocation
    /// failure).
    ////////////////////////////////////////////////////////////////////////////

    static Ptr create(char const *effectName);

    /// @}

  protected: /// \internal
    ModuleBase() {}
    ~ModuleBase() {};
}; // class ModuleBase

typedef ModuleBase::Ptr ModulePtr;   ///< shared pointer to a mutable ModuleBase instance
typedef ModuleBase::CPtr ModuleCPtr; ///< shared pointer to a const ModuleBase instance

// LE::Utility::IntrusivePtr required details
void intrusive_ptr_add_ref(ModuleBase const *); ///< \internal
void intrusive_ptr_release(ModuleBase const *); ///< \internal

/// @} // group Engine

//------------------------------------------------------------------------------
} // namespace Engine
//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // moduleBase_hpp
