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

#include "gui/modules/moduleUI.hpp"

#include "le/spectrumworx/engine/module.hpp"
#include "le/utility/cstdint.hpp"
#include <optional>
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace SW
{
//------------------------------------------------------------------------------

namespace GUI
{
class SpectrumWorxEditor;
}

////////////////////////////////////////////////////////////////////////////////
///
/// \class Module
///
////////////////////////////////////////////////////////////////////////////////

class LE_NOVTABLE Module : public Engine::ModuleDSP,
                           public AutomatedModuleImpl<Module>,
                           private GUI::ParameterWidgetsVTable<Module>
{
  public: // GUI
    void createGUI(GUI::SpectrumWorxEditor &, std::uint8_t moduleIndex);
    bool destroyGUI();

  public:
    using OptionalUI = std::optional<GUI::ModuleUI>;

    OptionalUI &gui() { return ui_; }
    OptionalUI const &gui() const { return ui_; }

    float setParameterValueFromUI(std::uint8_t parameterIndex, float value);

    static Module &fromGUI(GUI::ModuleUI &);

  public:
    template <class Effect> class Impl;

  protected:
    template <class Effect, typename... T>
    Module(Impl<Effect> *const pImpl, T &&...args)
        : ModuleDSP(std::forward<T>(args)...), GUI::ParameterWidgetsVTable<Module>(*pImpl)
    {
    }

  public: //...mrmlj...(delete pModule)...
    ~Module();

  private:
    /// \note Four overrides stood here -- `set{Base,Effect}Parameter` and
    /// `set{Base,Effect}ParameterFromLFO` -- and every one of them existed to push
    /// a value into a `juce::Slider`. The last two ran once per block per enabled
    /// LFO, from the audio thread, which is the stack in
    /// doc/tech/correct_the_threading.md §1A; the first two put a `juce::String`
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

  private:
    OptionalUI ui_;
}; // class Module

//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // moduleDSPAndGUI_hpp
