////////////////////////////////////////////////////////////////////////////////
///
/// \file moduleWidgets.hpp
/// -----------------------
///
///   The controls for one effect's parameters, owned by the region that draws
/// them.
///
/// \note They used to be owned by the *module*: `Module::Impl<Effect>` inherited
/// `ModuleWidgets<Effect>`, so every module the factory `malloc`ed carried the
/// JUCE widget storage for its effect inline, sized at compile time -- and
/// `sw-dsp`, the target whose own comment says "the engine, the effects and
/// everything they need. No host.", linked `juce_gui_basics` because of it.
///
///   Which effect's widgets to build is an *index* at runtime, so the per-effect
/// instantiation happens behind one table here rather than through a pair of
/// function pointers planted in each module at construction.
///
/// See doc/tech/threading_model.md.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef moduleWidgets_hpp__5C81A93E_47D2_4B60_9F18_A3E620D7C154
#define moduleWidgets_hpp__5C81A93E_47D2_4B60_9F18_A3E620D7C154
//------------------------------------------------------------------------------
#include "le/utility/cstdint.hpp"

#include <memory>

namespace LE::SW::GUI
{

class ModuleUI;

////////////////////////////////////////////////////////////////////////////////
///
/// \class ModuleWidgets
///
/// \brief One effect's parameter controls, type-erased.
///
/// \note The widgets are children of the ModuleUI and are destroyed by
/// `ParameterWidgets::destroy()` in the derived destructor, so this must outlive
/// nothing and be destroyed before the region it built into.
///
////////////////////////////////////////////////////////////////////////////////

class ModuleWidgets
{
  public:
    virtual ~ModuleWidgets() = default;

  protected:
    ModuleWidgets() = default;

    ModuleWidgets(ModuleWidgets const &) = delete; // makes non-copyable
    ModuleWidgets &operator=(ModuleWidgets const &) = delete;
}; // class ModuleWidgets

/// \brief Builds \p effectIndex's controls into \p region, and names it.
///
/// \return null if the index is not one of the shipped effects.
std::unique_ptr<ModuleWidgets> createModuleWidgets(std::uint8_t effectIndex, ModuleUI &region);

} // namespace LE::SW::GUI

#endif // moduleWidgets_hpp
