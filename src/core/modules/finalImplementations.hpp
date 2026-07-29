////////////////////////////////////////////////////////////////////////////////
///
/// \file finalImplementations.hpp
///
/// Implementations of different module interfaces for a given effect
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef finalImplementations_hpp__A4E1EEF8_DCF0_4AB1_8A68_6767C135FDC6
#define finalImplementations_hpp__A4E1EEF8_DCF0_4AB1_8A68_6767C135FDC6
//------------------------------------------------------------------------------
#include "le/parameters/boolean/tag.hpp"
#include "le/parameters/enumerated/tag.hpp"
#include "le/parameters/linear/tag.hpp"
#include "le/parameters/symmetric/tag.hpp"
#include "le/parameters/trigger/tag.hpp"
#include "le/parameters/fusionAdaptors.hpp"
#include "le/parameters/parametersUtilities.hpp"
#include "le/spectrumworx/effects/effects.hpp"
#include "le/spectrumworx/engine/moduleImpl.hpp"
#include "le/spectrumworx/engine/moduleParameters.hpp"
#include "le/utility/cstdint.hpp"
#include "le/utility/platformSpecifics.hpp"

#include <array>
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace SW
{
//------------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
///
/// \class ModuleWidgets
///
////////////////////////////////////////////////////////////////////////////////

template <class Effect> class LE_NOVTABLE ModuleWidgets
{
  public: // Module GUI interface implementation.
    void create(GUI::ModuleUI &uiBase)
    {
        LE_ASSERT(!uiBase.module().gui());
        uiBase.setUpForEffect(Effect::title, Effect::description);
        parameterWidgets_.construct(uiBase);
    }

    void destroy()
    {
        //LE_ASSERT( !uiBase.module().gui() );
        parameterWidgets_.destroy();
    }

  private:
    using Parameters = typename Effect::Parameters;
    using ParameterWidgets = GUI::ParameterWidgets<Parameters>;

  private:
    ParameterWidgets parameterWidgets_;
}; // class ModuleWidgets


////////////////////////////////////////////////////////////////////////////////
///
/// \class Module::Impl<>
///
////////////////////////////////////////////////////////////////////////////////

template <class Effect>
class Module::Impl final : public Engine::ModuleEffectImpl<Effect, Module>,
                           public ModuleWidgets<Effect>
{
  public:
    template <typename EffectTypeIndex>
    Impl(EffectTypeIndex) : Impl::ModuleEffectImpl(EffectTypeIndex(), this)
    {
    }
}; // class Module::Impl


//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // finalImplementations_hpp
