////////////////////////////////////////////////////////////////////////////////
///
/// \file moduleDSPAndGUI.hpp
///
///    SW plugin module interface and implementation.
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef moduleDSPAndGUI_hpp__C1E04A83_2733_44D6_A484_9F03A08AD6CD
#define moduleDSPAndGUI_hpp__C1E04A83_2733_44D6_A484_9F03A08AD6CD
//------------------------------------------------------------------------------
#include "automatedModuleImpl.hpp"

#include "le/spectrumworx/engine/module.hpp"
#include "le/utility/cstdint.hpp"

namespace LE::SW
{

////////////////////////////////////////////////////////////////////////////////
///
/// \class Module
///
/// \note It held its own editor region -- a `std::optional<GUI::ModuleUI>`
/// member, plus a `ParameterWidgetsVTable` base carrying a pair of function
/// pointers to build and destroy that effect's widgets. So the engine's module
/// class was a `juce::Component` container, `sw-dsp` linked `juce_gui_basics`,
/// and every module the factory `malloc`ed carried the widget storage for its
/// effect inline.
///
///   The region belongs to the editor now and holds a counted reference *to* the
/// module -- the other way round -- so nothing here knows the interface exists.
/// See gui/modules/moduleUI.hpp and doc/tech/threading_model.md.
///                                           (02.08.2026.) (SW port)
///
////////////////////////////////////////////////////////////////////////////////

class LE_NOVTABLE Module : public Engine::ModuleDSP, public AutomatedModuleImpl<Module>
{
  public:
    float setParameterValueFromUI(std::uint8_t parameterIndex, float value);

  public:
    template <class Effect> class Impl;

  protected:
    template <class Effect, typename... T>
    Module(Impl<Effect> *, T &&...args) : ModuleDSP(std::forward<T>(args)...)
    {
    }

  public: //...mrmlj...(delete pModule)...
    ~Module();

  private:
    /// \note Four overrides stood here -- `set{Base,Effect}Parameter` and
    /// `set{Base,Effect}ParameterFromLFO` -- and every one of them existed to push
    /// a value into a `juce::Slider`. The last two ran once per block per enabled
    /// LFO, from the audio thread, which is the stack in
    /// doc/tech/threading_model.md §1; the first two put a `juce::String`
    /// there whenever the moved parameter's control happened to be the active one.
    ///
    ///   Nothing replaces them in the engine. The plugin publishes what the LFOs
    /// did into the ValueMailbox after the block, and reports a host's parameter
    /// event on the ToUI ring, both of which it can do because it is the thing
    /// that knows about both sides. `dsp.cmake` predicted this: those setters were
    /// virtual only because of the interface, and being virtual is what gave the
    /// engine two ABIs.
    ///                                       (02.08.2026.) (SW port)
    friend class AutomatedModuleImpl<Module>;
}; // class Module

} // namespace LE::SW

#endif // moduleDSPAndGUI_hpp
