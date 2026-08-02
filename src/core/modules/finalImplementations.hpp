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
#include "le/parameters/parametersUtilities.hpp"
#include "le/parameters/parametersUtilities.hpp"
#include "le/spectrumworx/effects/effects.hpp"
#include "core/modules/moduleDSPAndGUI.hpp"
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

/// \note `template <class Effect> class ModuleWidgets` stood here, and
/// `Module::Impl<Effect>` inherited it -- so every module the factory allocated
/// carried the JUCE widget storage for its effect inline, and `sw-dsp` needed
/// `juce_gui_basics` to know how big that was. It is
/// `GUI::WidgetsFor<Effect>` in gui/modules/moduleWidgets.cpp now, owned by the
/// region that draws it and chosen by effect index rather than by type.
///                                           (02.08.2026.) (SW port)

////////////////////////////////////////////////////////////////////////////////
///
/// \class Module::Impl<>
///
////////////////////////////////////////////////////////////////////////////////

template <class Effect> class Module::Impl final : public Engine::ModuleEffectImpl<Effect, Module>
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
