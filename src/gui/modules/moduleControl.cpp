////////////////////////////////////////////////////////////////////////////////
///
/// module.cpp
/// ----------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "moduleControl.hpp"

#include "core/modules/automatedModuleImpl.inl" //...mrmlj...

#include "gui/editor/spectrumWorxEditor.hpp"
#include "gui/modules/moduleUI.hpp"

#include "le/parameters/lfo.hpp"
#include "le/parameters/printer.hpp"

#include "core/modules/moduleDSPAndGUI.hpp"

#include "le/utility/polymorphicDowncast.hpp"

#include <juce_gui_basics/juce_gui_basics.h>
#include "le/utility/span.hpp"
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace SW
{
//------------------------------------------------------------------------------
namespace GUI
{
//------------------------------------------------------------------------------

// Implementation note:
//   Because no two windows can have focus at the same time and because JUCE
// does not support multithreaded GUI code, it is safe to use a static here even
// if there are multiple effect editor instances open.
//                                            (07.07.2011.) (Domagoj Saric)
/// \todo Verify this on the Mac.
///                                           (07.07.2011.) (Domagoj Saric)
/// \note Verified, and the answer is no -- but not for the reason the note was
/// worried about. Focus really is exclusive; *lifetime* is not. Two editors
/// shared one pointer, so closing one left the other holding a control that had
/// been freed, and `reportInactiveControl()` would then call
/// `editor().moduleControlDectivated()` on it. It is
/// SpectrumWorxEditor::pActiveControl_ now, one per editor.
///                                           (02.08.2026.) (SW port)

bool ModuleControlBase::isActive() const { return this == editor().activeControl(); }

bool ModuleControlBase::tryActivateControl() const
{
    if (isActive())
        return false;

    if (juce::Component::getNumCurrentlyModalComponents() != 0)
        return false;

    Theme::ModuleUIMouseOverReaction const desiredReaction(
        Theme::settings().moduleUIMouseOverReaction);
    juce::Component const *const pFocusedComponent(juce::Component::getCurrentlyFocusedComponent());

    bool const nothingFocused(noModuleOrModuleControlFocused());
    bool const parentFocused(pFocusedComponent == &moduleUI());
    bool const parentOrNothingFocused(parentFocused |
                                      nothingFocused); // Intentional bitwise or as an optimization.
    bool const controlFocused(pFocusedComponent == &widget());
    if ((controlFocused) ||
        (desiredReaction == Theme::WhenParentOrNothingSelected && parentOrNothingFocused) ||
        (desiredReaction == Theme::WhenParentModuleSelected && parentFocused))
        return true;
    else
        return false;
}

bool ModuleControlBase::reportActiveControl(double const minimum, double const maximum,
                                            double const interval)
{
    if (tryActivateControl())
    {
        /// \note The control is marked as activated only after the editor
        /// has been informed (and the LFO GUI created) to avoid race condition
        /// crashes when a user activates a control that is being automated (and
        /// SpectrumWorxEditor::updateActiveControlValue() gets called as a
        /// response to setParameter() before the LFO gets created).
        ///                                   (25.04.2013.) (Domagoj Saric)
        editor().moduleControlActivated(*this, minimum, maximum, interval);
        editor().pActiveControl_ = this;
        return true;
    }
    return false;
}

bool ModuleControlBase::reportInactiveControl() const
{
    if (isActive() && !Detail::hasDirectFocus(widget()))
    {
        editor().moduleControlDectivated(*this);
        editor().pActiveControl_ = nullptr;
        return true;
    }
    else
        return false;
}

void ModuleControlBase::moduleParameterChanged()
{
    LE_ASSERT(!editor().selectedModule() || editor().selectedModule() == &moduleUI());
    LE_ASSERT(noModuleOrModuleControlFocused() || Detail::hasDirectFocus(widget()) ||
              moduleUI().hasDirectFocus());
    LE_ASSERT(isActive());
    LE_ASSERT(!isLFOEnabled());

    /// \note Checkout revision 7493 or earlier for (more templated) code
    /// that directly accessed the effect's parameters.
    ///                                       (12.03.2013.) (Domagoj Saric)
    SpectrumWorxEditor &editor(this->editor());
    std::uint8_t const parameterIndex(moduleParameterIndex() + 1); // Bypass
    editor.updateModuleParameterAndNotifyHost(moduleUI(), parameterIndex, getValue());
    editor.updateActiveControlValue();
}

void ModuleControlBase::configureControl(bool const mouseClickCanGrabFocus)
{
    widget().setWantsKeyboardFocus(true);
    widget().setMouseClickGrabsKeyboardFocus(mouseClickCanGrabFocus);
}

bool ModuleControlBase::noModuleOrModuleControlFocused(SpectrumWorxEditor const &editor)
{
    // Implementation note:
    //   We ignore focused widgets on auxiliary windows.
    //                                        (07.07.2011.) (Domagoj Saric)
    juce::Component const *const pFocusedComponent(juce::Component::getCurrentlyFocusedComponent());
    return (pFocusedComponent == nullptr) || (pFocusedComponent == &editor) ||
           (!editor.isParentOf(*pFocusedComponent));
}

bool ModuleControlBase::noModuleOrModuleControlFocused() const
{
    return noModuleOrModuleControlFocused(editor());
}

ModuleControlBase::Module &ModuleControlBase::module() { return moduleUI().module(); }

ModuleUI &ModuleControlBase::moduleUI()
{
    // Implementation note:
    //   Shared module controls are not children of ModuleUI instances so we
    // cannot just return
    // *LE::Utility::polymorphicDowncast<ModuleUI *>( widget().getParentComponent() ).
    //                                        (04.10.2011.) (Domagoj Saric)
    LE_ASSERT(pModuleUI_);
    return *pModuleUI_;
}

/// \note Was `polymorphicDowncast<SpectrumWorxEditor *>( moduleUI().getParentComponent() )`,
/// so it could only be asked once the module's region had been parented -- and
/// `Module::createGUI` writes every parameter into the widgets before that
/// happens. ModuleUI holds its editor now.
///                                           (02.08.2026.) (SW port)
SpectrumWorxEditor &ModuleControlBase::editor() const
{
    /// \note The const_cast is what the previous body got for free: JUCE's
    /// `getParentComponent() const` hands back a non-const `Component *`.
    return const_cast<ModuleUI &>(moduleUI()).editor();
}

LE::Parameters::LFOImpl &ModuleControlBase::lfo() { return module().lfo(moduleParameterIndex()); }
LE::Parameters::RuntimeInformation const &ModuleControlBase::info() const
{
    return module().parameterInfo(moduleParameterIndex() + 1);
}
char const *ModuleControlBase::name() const { return info().name; }

bool ModuleControlBase::isActive(juce::Component const &widget)
{
    /// \note Recovers the editor from the widget rather than asking a static
    /// which control is active, so that the question is answered by the editor
    /// the widget belongs to. The caller is Theme's painting, which has a widget
    /// and nothing else.
    auto const *const pControl(SpectrumWorxEditor::fromChild(widget).activeControl());
    return pControl && (&widget == &pControl->widget());
}

juce::String ModuleControlBase::getValueString(float const *LE_RESTRICT const pValue) const
{
    std::array<char, 20> buffer;
    using Printer = Engine::ModuleParameters::ParameterPrinter;
    Printer const printer = {
        pValue ? *pValue : 0,
        pValue ? Printer::Unchanged : Printer::Internal,
        {LE::Utility::makeSpan(&buffer[0], buffer.size()), moduleUI().editor().engineSetup()}};
    std::uint8_t const parameterIndex(moduleParameterIndex() + 1 /*Bypass*/);
    char const *const pValueString(module().getParameterValueString(parameterIndex, printer));
    char const *const pUnit(Automation::getParameterUnit(parameterIndex, &module()));
    juce::String result(pValueString);
    result.appendCharPointer(juce::CharPointer_ASCII(pUnit));
    return result;
}

namespace
{
// Cheap cross-casting between widget and ModuleControlBase instances
class LE_NOVTABLE ControlWidgetBridge : public ModuleControlBase, public WidgetBase<>
{
  public:
    static ModuleControlBase &asControl(juce::Component &widget)
    {
        LE_ASSUME(&widget);
        ModuleControlBase &control(static_cast<ControlWidgetBridge &>(widget));
        LE_ASSERT_MSG(&control == dynamic_cast<ModuleControlBase *>(&widget),
                      "Widget is not a module control.");
        LE_ASSUME(&control);
        return control;
    }

    static juce::Component &asWidget(ModuleControlBase &control)
    {
        LE_ASSUME(&control);
        juce::Component &widget(static_cast<ControlWidgetBridge &>(control));
        LE_ASSERT_MSG(&widget == dynamic_cast<juce::Component *>(&control),
                      "Module control detached from its widget.");
        LE_ASSUME(&widget);
        return widget;
    }

  private:
    ControlWidgetBridge();
    ~ControlWidgetBridge();
}; // class ControlWidgetBridge
} // anonymous namespace

juce::Component &ModuleControlBase::widget() { return ControlWidgetBridge::asWidget(*this); }
ModuleControlBase &ModuleControlBase::controlForWidget(juce::Component &widget)
{
    return ControlWidgetBridge::asControl(widget);
}

bool ModuleControlBase::isLFOEnabled() const { return lfo().enabled(); }

void ModuleControlBase::reassignTo(ModuleUI &moduleUI)
{
    LE_ASSERT_MSG(isASharedModuleControl(),
                  "This functionality is meant only for SharedModuleControls where the "
                  "controls are not actually owned by the ModuleUI and can thus be "
                  "assigned to different module UIs.");
    pModuleUI_ = &moduleUI;
}

void ModuleControlBase::reassignTo(std::uint8_t const parameterIndex)
{
    LE_ASSERT_MSG(isASharedModuleControl(),
                  "This functionality is meant only for SharedModuleControls where the "
                  "frequency control can change the parameter it maps to.");
    parameterIndex_ = parameterIndex;
}

void ModuleControlBase::clearActiveControl()
{
    LE_ASSERT_MSG(isASharedModuleControl(),
                  "This functionality is meant only for the "
                  "SharedModuleControls::FrequencyRange class where the same "
                  "ModuleControlBase instance can map to a different logical control.");
    editor().pActiveControl_ = nullptr;
}

bool ModuleControlBase::isASharedModuleControl() const
{
    /// \note Shared module control widgets are not children of the
    /// corresponding ModuleUI instance (but rather of the singleton
    /// SharedModuleControls instance).
    ///                                       (12.02.2014.) (Domagoj Saric)
    return pModuleUI_ != widget().getParentComponent();
}

//------------------------------------------------------------------------------
} // namespace GUI
//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
