////////////////////////////////////////////////////////////////////////////////
///
/// \file moduleUI.hpp
/// ------------------
///
/// Module UI related functionality.
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef moduleUI_hpp__8228E5F3_535E_4B08_9AD0_072C9fA7AD93
#define moduleUI_hpp__8228E5F3_535E_4B08_9AD0_072C9fA7AD93
//------------------------------------------------------------------------------
#include "gui/gui.hpp"
#include "gui/modules/moduleControl.hpp"

#include "le/math/conversion.hpp"
#include "le/parameters/linear/parameter.hpp"
#include "le/parameters/boolean/tag.hpp"
#include "le/parameters/enumerated/tag.hpp"
#include "le/parameters/powerOfTwo/tag.hpp"
#include "le/parameters/trigger/tag.hpp"
#include "le/parameters/symmetric/tag.hpp"
#include "le/parameters/parametersUtilities.hpp"
#include "le/parameters/printer_fwd.hpp"
#include "le/utility/cstdint.hpp"
#include "le/utility/platformSpecifics.hpp"
#include "le/utility/tchar.hpp"

#include "le/utility/polymorphicDowncast.hpp"

#include <juce_gui_basics/juce_gui_basics.h>
#include <optional>
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace Utility
{
class CriticalSectionLock;
}
//------------------------------------------------------------------------------
namespace SW
{
//------------------------------------------------------------------------------

class Module;
class ModuleGUI;

namespace Effects
{
namespace Detail
{
struct EmptyParameters;
}

namespace BaseParameters
{
class Bypass;
class Gain;
class Wet;
class StartFrequency;
class StopFrequency;
} // namespace BaseParameters
} // namespace Effects

namespace Engine
{
class Setup;
}

//------------------------------------------------------------------------------
namespace GUI
{
//------------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
///
/// \class ModuleKnob
///
////////////////////////////////////////////////////////////////////////////////

class ModuleUI;

class LE_NOVTABLE ModuleKnob : public Knob, public ModuleControl<ModuleKnob>
{
  protected:
    ModuleKnob(juce::Component &parent, unsigned int x, unsigned int y);

  public:
#pragma warning(push)
#pragma warning(                                                                                   \
    disable                                                                                        \
    : 4480) // Nonstandard extension used: specifying underlying type for enum 'SW::Effects::PhaseVocoderShared::pitchShiftAndScale::TransientBins'.
    enum Quantization : std::uint8_t
    {
        Fixed,
        FrequencyInHertz,
        TimeInMilliseconds
    };
#pragma warning(pop)

  private:
    using Hertz = LE::Parameters::UnitString<" Hz">;
    using Millisecond = LE::Parameters::UnitString<" ms">;

    template <typename Unit> struct QuantizationImpl;

  public:
    template <class Parameter>
    struct QuantizationFor : QuantizationImpl<typename LE::Parameters::Detail::GetTraitDefaulted<
                                 LE::Parameters::Traits::Tag::Unit, typename Parameter::Traits,
                                 typename Parameter::Defaults>::type>
    {
    }; // struct QuantizationFor

    void setupForParameter(juce::Image const &imageStrip, Quantization quantizationType,
                           std::uint8_t quantizationStep);

  private: // juce::Component overrides
    void mouseDown(juce::MouseEvent const &) noexcept override;
    void mouseUp(juce::MouseEvent const &) noexcept override;

    void valueChanged() noexcept override;

    void paint(juce::Graphics &) override;

  protected: // ModuleControl interface.
    void lfoStateChanged();

    void updateForEngineSetupChanges(Engine::Setup const &);

    void moduleControlActivated();
    void moduleControlDeactivated();

    double valueRangeMinimum() const { return getMinimum(); }
    double valueRangeMaximum() const { return getMaximum(); }
    double valueRangeQuantum() const { return getInterval(); }

    static bool const mouseClickCanGrabFocus = true;

  public:
    typedef Knob BaseWidget;

  private:
    void syncMouseWheelAndLFOState();

  private:
    Quantization quantization_;
    juce::Image const *LE_RESTRICT pImageStrip_;

  private:
    static unsigned int const marginForGlow = 4;
    static unsigned int const spaceForText = 18;
}; // class ModuleKnob

template <typename Unit> struct ModuleKnob::QuantizationImpl
{
    static Quantization const value = Fixed;
};
template <> struct ModuleKnob::QuantizationImpl<ModuleKnob::Hertz>
{
    static Quantization const value = FrequencyInHertz;
};
template <> struct ModuleKnob::QuantizationImpl<ModuleKnob::Millisecond>
{
    static Quantization const value = TimeInMilliseconds;
};

////////////////////////////////////////////////////////////////////////////////
///
/// \class ModuleLEDTextButton
///
////////////////////////////////////////////////////////////////////////////////

class LE_NOVTABLE ModuleLEDTextButton : public LEDTextButton,
                                        public ModuleControl<ModuleLEDTextButton>
{
  protected:
    ModuleLEDTextButton(juce::Component &parent, unsigned int x, unsigned int y);

  private: // juce::Component overrides
    void mouseDown(juce::MouseEvent const &) override;
    void paintButton(juce::Graphics &, bool isMouseOverButton, bool isButtonDown) override;

  protected: // ModuleControl interface.
    void focusChanged() { repaint(); }

    // Implementation note:
    //   We allow a smooth LFO range control for boolean parameters.
    //                                        (21.07.2011.) (Domagoj Saric)
    static double valueRangeQuantum() { return 0; }

    using BitmapButton::getTextFromValue;
    char const *getValueText() const { return getTextFromValue(getValue()); }

  public:
    typedef BitmapButton BaseWidget;

  private:
    void clicked() override;
}; // class ModuleLEDTextButton

////////////////////////////////////////////////////////////////////////////////
///
/// \class TriggerButton
///
////////////////////////////////////////////////////////////////////////////////

class LE_NOVTABLE TriggerButton : public BitmapButton, public ModuleControl<TriggerButton>
{
  protected:
    TriggerButton(juce::Component &parent, unsigned int x, unsigned int y);

  public:
    value_type getValue() const { return isDown(); }
    void setValue(param_type);

  protected: // ModuleControl interface.
    void lfoStateChanged() { setValue(false); }
    void focusChanged() { repaint(); }

    // Implementation note:
    //   We allow a smooth LFO range control for boolean parameters.
    //                                        (21.07.2011.) (Domagoj Saric)
    static double valueRangeQuantum() { return 0; }

    using BitmapButton::getTextFromValue;
    char const *getValueText() const { return getTextFromValue(getValue()); }

  public:
    typedef TriggerButton BaseWidget;

  private: // juce::Component overrides
    void mouseDown(juce::MouseEvent const &) override;
    void mouseUp(juce::MouseEvent const &) noexcept override;

    void paintButton(juce::Graphics &, bool isMouseOverButton, bool isButtonDown) override;
}; // class TriggerButton

////////////////////////////////////////////////////////////////////////////////
///
/// \class DiscreteParameter
///
/// \brief Module UI widget for parameters with special discrete values.
///
////////////////////////////////////////////////////////////////////////////////

class LE_NOVTABLE DiscreteParameter : public ComboBox, public ModuleControl<DiscreteParameter>
{
  protected:
    DiscreteParameter(juce::Component &parent, unsigned int x, unsigned int y);

  private:
    void mouseDown(juce::MouseEvent const &) override;

  protected: // ModuleControl interface.
    void focusChanged();

    juce::String const &getTextFromValue(value_type const valueIndex) const
    {
        return getItemText(valueIndex);
    }
    juce::String const &getValueText() const { return getSelectedItemText(); }

    double valueRangeMaximum() const { return Math::convert<double>(numberOfItems() - 1); }

  public:
    typedef ComboBox BaseWidget;

  private:
    static unsigned int const horizontalMargin = 8;
    static unsigned int const textHeight = 11;
}; // class DiscreteParameter

////////////////////////////////////////////////////////////////////////////////
///
/// \class ModuleUI
///
////////////////////////////////////////////////////////////////////////////////

namespace Detail
{
template <typename ParameterTag> struct WidgetForParameterAux
{
    typedef ModuleKnob type;
};
template <> struct WidgetForParameterAux<Parameters::BooleanParameterTag>
{
    typedef ModuleLEDTextButton type;
};
template <> struct WidgetForParameterAux<Parameters::EnumeratedParameterTag>
{
    typedef DiscreteParameter type;
};
template <> struct WidgetForParameterAux<Parameters::TriggerParameterTag>
{
    typedef TriggerButton type;
};

template <class Parameter>
using WidgetForParameter = WidgetForParameterAux<typename Parameter::Tag>;
} // namespace Detail

class SharedModuleControls;
class SpectrumWorxEditor;

class ModuleUI final : public WidgetBase<>, private juce::Button::Listener
{
  public:
    enum ParameterChangeSource
    {
        AutomationOrPreset,
        LFOValue
    }; // enum ParameterChangeSource

    void setBaseParameter(std::uint8_t sharedParameterIndex, float parameterValue,
                          ParameterChangeSource);
    void setEffectParameter(std::uint8_t effectParameterIndex, float parameterValue,
                            ParameterChangeSource);
    void setParameter(std::uint8_t parameterIndex, float parameterValue, ParameterChangeSource);

    void setBypass(bool);

    void updateForEngineSetupChanges(Engine::Setup const &);

    void updateLFOParameter(std::uint8_t parameterIndex, std::uint8_t lfoParameterIndex,
                            float /*Parameters::RuntimeInformation::value_type*/ value);

  public:
    juce::String const &description() const { return description_; }

    SpectrumWorxEditor &editor();
    SpectrumWorxEditor const &editor() const;
    SharedModuleControls &sharedControls();

    Utility::CriticalSectionLock getProcessingLock() const; //...mrmlj...quick-fix...

    //...mrmlj...quick-fix...
    void holdSharedControls(bool doHold) const;
    bool sharedControlsLocked() const;

    typedef SW::Module Module;

    Module &module();
    Module const &module() const;

    /// \note Was `static ModuleUI *selectedModule()` over a file-scope pointer,
    /// with a 2011 note arguing that a static was safe "even if there are multiple
    /// effect editor instances open" because no two windows can have focus at
    /// once, and a `\todo Verify this on the Mac` under it. Focus is not the
    /// question: two instances shared one pointer, so the second editor to select
    /// a module silently deselected the first one's, and an editor closing left
    /// the other holding a pointer into freed storage. It is the editor's now.
    ///                                       (02.08.2026.) (SW port)
    bool selected() const;

  public:
    /// \note The editor is held rather than recovered from the component
    /// hierarchy. `editor()` used to walk `getParentComponent()`, which meant it
    /// could only be asked once the region had been parented -- and
    /// `Module::createGUI` pushes every parameter value into the widgets *before*
    /// `addToParentAndShow`. That worked only because everything reached from
    /// there went through process-wide statics instead of through the editor.
    /// Those statics are the editor's members now, so the reference has to be
    /// there from the first line of the constructor.
    ///                                       (02.08.2026.) (SW port)
    explicit ModuleUI(SpectrumWorxEditor &);
    ~ModuleUI();

    void setUpForEffect(char const *effectName, char const *effectDescription);

    void moveToSlot(std::uint8_t slotIndex);

    ModuleControlBase &effectSpecificParameterControl(std::uint8_t parameterIndex);
    ModuleControlBase const &effectSpecificParameterControl(std::uint8_t parameterIndex) const;

  private:
    friend class SpectrumWorxEditor;
    friend class SharedModuleControls; //...mrmlj...
    void activate();
    void deactivate();
    bool selectionTracksMouseMovements() const;

  private: // JUCE Component overrides.
    void paint(juce::Graphics &) override;

    void mouseDrag(juce::MouseEvent const &) override;
    void mouseEnter(juce::MouseEvent const &) override;
    void mouseExit(juce::MouseEvent const &) noexcept override;
    void mouseUp(juce::MouseEvent const &) noexcept override;

    void focusGained(FocusChangeType) override;
    void focusLost(FocusChangeType) override;
    void focusOfChildComponentChanged(FocusChangeType) override;

  private: // JUCE ButtonListener overrides.
    void buttonClicked(juce::Button *) override;

  private:
    /// \note First, and a reference: the widgets below are built in the
    /// constructor body and reach through it.
    SpectrumWorxEditor &editor_;

    BitmapButton bypass_;
    BitmapButton eject_;

    juce::String description_;

  public:
    static std::uint8_t const horizontalOffset = 213;
    static std::uint8_t const verticalOffset = 9;
    static std::uint16_t const height = 358;
    static std::uint8_t const width = 68;
    static std::uint8_t const distance = 0;
    static std::uint8_t const border = 4;

    static std::uint8_t const baseWidgets = 2;

  private:
    static ModuleUI *pSelectedModule_;
}; // class ModuleUI

namespace Detail ///< \internal
{
////////////////////////////////////////////////////////////////////////////
///
/// \class ModuleWidgetConstructionState
///
////////////////////////////////////////////////////////////////////////////

struct ModuleWidgetConstructionState
{
  public:
    ModuleWidgetConstructionState(ModuleUI &parent);

    ModuleUI &parent;
    mutable std::uint16_t yOffset;
    mutable std::uint8_t parameterIndex;

  private:
    ModuleWidgetConstructionState(ModuleWidgetConstructionState const &);
    void operator=(ModuleWidgetConstructionState const &);
}; // struct ModuleWidgetConstructionState

template <class Widget> struct ModuleWidgetHolder
{
    ModuleWidgetHolder(ModuleWidgetConstructionState &);

    ModuleControlImpl<Widget> widget;
}; // struct ModuleWidgetHolder

/// \note Was `#ifdef __clang__ //...mrmlj...ambiguity compilation errors...`, and
/// the ambiguity is real rather than a Clang quirk. `WidgetsStorage` below folds
/// one base class per parameter onto the chain, so an effect with two parameters
/// of the same widget type — and most have several knobs — inherits
/// `ModuleWidgetHolder<ModuleKnob>` twice. Converting to a base that appears
/// twice is ambiguous, full stop; MSVC accepted it (hence the 4584 suppression
/// on WidgetsStorage) and resolved to whichever it saw first. Interposing a
/// holder keyed on the *parameter* makes every base distinct, which is the fix
/// for all three compilers rather than for one.
///                                           (29.07.2026.) (SW port)
template <typename Parameter>
struct ParameterWidgetHolder : ModuleWidgetHolder<typename WidgetForParameter<Parameter>::type>
{
    ParameterWidgetHolder(ModuleWidgetConstructionState &state)
        : ModuleWidgetHolder<typename WidgetForParameter<Parameter>::type>(state)
    {
    }
};

template <typename Parameter> struct ParameterWidget
{
    typedef ParameterWidgetHolder<Parameter> type;
}; // struct ParameterWidget

////////////////////////////////////////////////////////////////////////////
///
/// \class WidgetInitialiser
///
////////////////////////////////////////////////////////////////////////////

struct WidgetInitialiser
{
    template <class Parameter, class Widget> static void setup(Widget const &) {}

    template <class Parameter> static void setup(ModuleControlImpl<DiscreteParameter> &comboBox)
    {
        fillComboBoxForParameter<Parameter>(comboBox);
    }

    template <class Parameter> static void setup(ModuleControlImpl<ModuleKnob> &knob)
    {
        knob.setupForParameter(
            std::is_base_of<LE::Parameters::SymmetricParameterTag, typename Parameter::Tag>::value
                ? resourceBitmap<SymmetricKnobStrip>()
                : resourceBitmap<ModuleKnobStrip>(),
            ModuleKnob::QuantizationFor<Parameter>::value, Parameter::discreteValueDistance);
    }
}; // struct WidgetInitialiser

////////////////////////////////////////////////////////////////////////////
///
/// \class EmptyWidgets
///
////////////////////////////////////////////////////////////////////////////

struct EmptyWidgets
{
    EmptyWidgets(ModuleWidgetConstructionState const &);
    static void setup(WidgetInitialiser const &) {}

    static void *operator new(std::size_t const count, void *LE_RESTRICT const pStorage)
    {
        (void)count;
        LE_ASSUME(pStorage);
        return pStorage;
    }
    static void operator delete(void *LE_RESTRICT const /*pObject*/,
                                void *LE_RESTRICT const /*pStorage*/)
    {
    }
}; // struct EmptyWidgets

////////////////////////////////////////////////////////////////////////////
///
/// \class WidgetsStorage
///
////////////////////////////////////////////////////////////////////////////

#pragma warning(push)
#pragma warning(disable : 4584) // base-class <> is already a base-class of WidgetsStorage
template <typename PreviousWidgets, typename Parameter>
struct WidgetsStorage : PreviousWidgets, ParameterWidget<Parameter>::type
{
    WidgetsStorage(ModuleWidgetConstructionState &state)
        : PreviousWidgets(state), ParameterWidget<Parameter>::type(state)
    {
    }

    void setup(WidgetInitialiser const &initialiser)
    {
        PreviousWidgets::setup(initialiser);
        initialiser.setup<Parameter>(ParameterWidget<Parameter>::type::widget);
    }
}; // struct WidgetsStorage
#pragma warning(pop)

////////////////////////////////////////////////////////////////////////////
///
/// \struct FoldWidgets
/// \internal
/// \brief One WidgetsStorage per parameter, each deriving from the last.
///
////////////////////////////////////////////////////////////////////////////
// Implementation note:
//   Was boost::mpl::fold< Parameters, EmptyWidgets, WidgetsStorage<_1, _2> >
// over the Fusion-adapted parameter container. The placeholder expression is
// the only thing MPL was contributing; the traversal is a left fold over the
// indices the container already knows about.
//                                        (30.07.2026.) (SW port)
////////////////////////////////////////////////////////////////////////////

template <class Accumulated, class Parameters, std::size_t index,
          bool done = (index == Parameters::static_size)>
struct FoldWidgets
{
    using type = typename FoldWidgets<
        WidgetsStorage<Accumulated, LE::Parameters::ParameterAt<Parameters, index>>, Parameters,
        index + 1>::type;
};

template <class Accumulated, class Parameters, std::size_t index>
struct FoldWidgets<Accumulated, Parameters, index, true>
{
    using type = Accumulated;
};
} // namespace Detail

////////////////////////////////////////////////////////////////////////////////
///
/// \class ParameterWidgets
///
////////////////////////////////////////////////////////////////////////////////

template <class ParametersParam> class ParameterWidgets
{
  public:
    typedef ParametersParam Parameters;

    using Container = typename Detail::FoldWidgets<Detail::EmptyWidgets, Parameters, 0>::type;

  public:
#ifndef NDEBUG
    ParameterWidgets() : constructed_(false) {}
    ~ParameterWidgets() { LE_ASSERT(!constructed_); }
#endif // NDEBUG

    void construct(ModuleUI &parent)
    {
        LE_ASSERT(!constructed_);
        doConstruct(parent);
        container().setup(Detail::WidgetInitialiser());
#ifndef NDEBUG
        constructed_ = true;
#endif // NDEBUG
    }

    void destroy()
    {
        LE_ASSERT(constructed_);
        container().~Container();
#ifndef NDEBUG
        constructed_ = false;
#endif // NDEBUG
    }

  private:
    void doConstruct(ModuleUI &parent)
    {
        Detail::ModuleWidgetConstructionState constructionState(parent);
        LE_ASSUME(&parameterWidgetsStorage_);
        Container *const pContainer(new (&parameterWidgetsStorage_) Container(constructionState));
        LE_ASSUME(pContainer);
    }

    Container &container()
    {
        Container *LE_RESTRICT const pContainer(
            &reinterpret_cast<Container &>(parameterWidgetsStorage_));
        LE_ASSUME(pContainer);
        return *pContainer;
    }

  private:
    typedef
        typename std::aligned_storage<sizeof(Container), std::alignment_of<Container>::value>::type
            ParameterWidgetsStorage;
    ParameterWidgetsStorage parameterWidgetsStorage_;

#ifndef NDEBUG
    bool constructed_;
#endif // NDEBUG
}; // class ParameterWidgets

template <> class ParameterWidgets<Effects::Detail::EmptyParameters>
{
  public:
    static void construct(ModuleUI &) {}
    static void destroy() {}
}; // class ParameterWidgets<EmptyParameters>

template <class Interface> struct ParameterWidgetsVTable
{
    template <class Implementation>
    ParameterWidgetsVTable(Implementation const &)
        :
#ifndef _MSC_VER //...mrmlj...msvc(12) compilation error bug...
          doCreateGUI([](ModuleUI &uiBase) {
              LE::Utility::polymorphicDowncast<Implementation *>(&uiBase.module())->create(uiBase);
          }),
          doDestroyGUI([](ModuleUI::Module &base) {
              LE::Utility::polymorphicDowncast<Implementation *>(&base)->destroy();
          })
#else
          doCreateGUI(&createGUI<Implementation>), doDestroyGUI(&destroyGUI<Implementation>)
#endif // _MSC_VER
    {
    }

    /// \note Both of these once carried a calling convention -- __fastcall for
    /// GNU, whatever LE_MSVC_SPECIFIC held for MSVC -- and both were emptied out,
    /// the GNU one to a comment and the MSVC one to `LE_MSVC_SPECIFIC()` with no
    /// argument at all. That is a function-like macro invoked with nothing, which
    /// MSVC warns about (C4003) once per declaration and every other compiler
    /// expands to the same nothing. Said plainly instead.
    ///                                       (30.07.2026.) (SW port)
    void (*const doCreateGUI)(ModuleUI &);
    void (*const doDestroyGUI)(ModuleUI::Module &) /*noexcept*/;

#ifdef _MSC_VER
  private:
    template <class Implementation> static void createGUI(ModuleUI &uiBase)
    {
        LE::Utility::polymorphicDowncast<Implementation *>(&uiBase.module())->create(uiBase);
    }
    template <class Implementation> static void destroyGUI(ModuleUI::Module &base)
    {
        LE::Utility::polymorphicDowncast<Implementation *>(&base)->destroy();
    }
#endif
}; // struct ParameterWidgetsVTable

//------------------------------------------------------------------------------
} // namespace GUI
//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // moduleUI_hpp
