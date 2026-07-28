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

#include <boost/noncopyable.hpp>
#include <boost/smart_ptr/intrusive_ptr.hpp>

#include <cstdint>

#include <boost/config/abi_prefix.hpp>
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
#ifndef DOXYGEN_ONLY
    : public boost::noncopyable
#endif // DOXYGEN_ONLY
{
  public:
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
    LE_NOTHROW void setStartFrequencyInHz(float frequency, ModuleProcessor const &processor);
    LE_NOTHROWNOALIAS float getStartFrequencyInHz(ModuleProcessor const &processor) const;
    LE_NOTHROW void setStartFrequencyInHz(float frequency, std::uint32_t sampleRate);
    LE_NOTHROWNOALIAS float getStartFrequencyInHz(std::uint32_t sampleRate) const;

    LE_NOTHROW void setStopFrequencyInHz(float frequency, ModuleProcessor const &processor);
    LE_NOTHROWNOALIAS float getStopFrequencyInHz(ModuleProcessor const &processor) const;
    LE_NOTHROW void setStopFrequencyInHz(float frequency, std::uint32_t sampleRate);
    LE_NOTHROWNOALIAS float getStopFrequencyInHz(std::uint32_t sampleRate) const;
    /// @}

  public:
    typedef BaseParameters Parameters;
    typedef LE::Parameters::LFO LFO;

    LE_NOTHROWNOALIAS Parameters &baseParameters();
    LE_NOTHROWNOALIAS Parameters const &baseParameters() const
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

    LE_NOTHROW LE_PURE_FUNCTION std::uint8_t numberOfParameters() const
    {
        return numberOfEffectSpecificParameters() + numberOfBaseParameters;
    } ///< total number of parameters (base + effect specific)
    LE_NOTHROW LE_PURE_FUNCTION std::uint8_t numberOfEffectSpecificParameters()
        const; ///< number of extra parameters specific to the instantiated effect
    LE_NOTHROW LE_PURE_FUNCTION std::uint8_t numberOfLFOControledParameters() const
    {
        return numberOfEffectSpecificParameters() + numberOfLFOBaseParameters;
    } ///< total number of parameters that can be LFO-ed

    LE_NOTHROWNOALIAS ParameterInfo const &parameterInfo(std::uint8_t parameterIndex) const;

    /// @}

    /// \name Parameter accessors
    /// \brief The setters return a possibly adjusted <VAR>value</VAR> (e.g. if
    /// you try to set a fractional value to an integer parameter it will be
    /// rounded).
    /// @{

    LE_NOTHROWNOALIAS float getParameter(std::uint8_t parameterIndex) const;
    LE_NOTHROW float setParameter(std::uint8_t parameterIndex, float value);

    LE_NOTHROWNOALIAS float getBaseParameter(std::uint8_t baseParameterIndex) const;
    LE_NOTHROW float setBaseParameter(std::uint8_t baseParameterIndex, float value);

    LE_NOTHROWNOALIAS float getEffectParameter(std::uint8_t effectParameterIndex) const;
    LE_NOTHROW float setEffectParameter(std::uint8_t effectParameterIndex, float value);

    /// @}

    /// \name Parameter LFO accessors
    /// @{
    LE_NOTHROWNOALIAS LFO &lfo(std::uint8_t parameterIndex);
    LE_NOTHROWNOALIAS LFO const &lfo(std::uint8_t const parameterIndex) const
    {
        return const_cast<ModuleBase &>(*this).lfo(parameterIndex);
    }
    /// @}

    /// \name Effect runtime metadata
    /// @{
    LE_NOTHROWRESTRICTNOALIAS char const *effectName() const;
    /// @}

  public:
    /// \name Factory function
    /// @{

    typedef boost::intrusive_ptr<ModuleBase>
        Ptr; ///< shared pointer to a mutable ModuleBase instance
    typedef boost::intrusive_ptr<ModuleBase const>
        CPtr; ///< shared pointer to a const ModuleBase instance

    ////////////////////////////////////////////////////////////////////////////
    /// \brief Creates a new module for <VAR>effectName</VAR>
    /// \details The <VAR>effectName</VAR> is expected to be the effect's
    /// exact type name (e.g. "TalkingWind"), not the effect's "title" displayed
    /// to the user in the SW GUI (e.g. "Talking Wind")
    /// \return A ModuleBase instance (or null in case of a memory allocation
    /// failure).
    ////////////////////////////////////////////////////////////////////////////

    static LE_NOTHROWNOALIAS Ptr create(char const *effectName);

    /// @}

  protected: /// \internal
    LE_NOTHROW ModuleBase() {}
    LE_NOTHROW ~ModuleBase(){};
}; // class ModuleBase

typedef ModuleBase::Ptr ModulePtr;   ///< shared pointer to a mutable ModuleBase instance
typedef ModuleBase::CPtr ModuleCPtr; ///< shared pointer to a const ModuleBase instance

// boost::intrusive_ptr required details
LE_NOTHROWNOALIAS void intrusive_ptr_add_ref(ModuleBase const *); ///< \internal
LE_NOTHROW void intrusive_ptr_release(ModuleBase const *);        ///< \internal

/// @} // group Engine

//------------------------------------------------------------------------------
} // namespace Engine
//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#include <boost/config/abi_suffix.hpp>
#endif // moduleBase_hpp
