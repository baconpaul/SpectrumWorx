////////////////////////////////////////////////////////////////////////////////
///
/// moduleUI.cpp
/// ------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "moduleUI.hpp"

#include "core/modules/moduleDSPAndGUI.hpp"
#include "gui/editor/spectrumWorxEditor.hpp"

#include "le/parameters/lfo.hpp"
#include "le/parameters/printer.hpp"
#include "le/parameters/uiElements.hpp"
#include "le/spectrumworx/engine/setup.hpp"
#include "le/utility/platformSpecifics.hpp"

#include "le/utility/assert.hpp"
#include "le/utility/polymorphicDowncast.hpp"

#include <cstdio>
#include <cstdlib>
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

ModuleLEDTextButton::ModuleLEDTextButton(juce::Component &parent, unsigned int const x,
                                         unsigned int const y)
    : LEDTextButton(parent, x, y, nullptr)
{
    setName(control().name());
    //...mrmlj...for temporary test selection...
    setSize(resourceBitmap<ModuleComboOn>().getWidth(), getHeight() + 2);
}

void ModuleLEDTextButton::clicked() { moduleParameterChanged(); }

void ModuleLEDTextButton::mouseDown(juce::MouseEvent const &event)
{
    if (!hasDirectFocus())
    {
        grabKeyboardFocus();
    }
    else if (!isLFOEnabled())
    {
        LEDTextButton::mouseDown(event);
    }
}

void ModuleLEDTextButton::paintButton(juce::Graphics &g, bool const isMouseOverButton,
                                      bool const isButtonDown)
{
    if (hasDirectFocus())
        paintImage(g, resourceBitmap<ModuleComboOn>(), 0, -1);
    g.setOrigin(3, 1);
    LEDTextButton::paintButton(g, isMouseOverButton, isButtonDown);
}

TriggerButton::TriggerButton(juce::Component &parent, unsigned int const x, unsigned int const y)
    : BitmapButton(parent, resourceBitmap<TriggerBtnOn>(), resourceBitmap<TriggerBtnOff>(),
                   juce::Colours::transparentWhite, false)
{
    setName(control().name());

    setBounds(x, y, ModuleUI::width, getHeight() + 13);

    setTriggeredOnMouseDown(true);

    addToParentAndShow(parent, *this);
}

void TriggerButton::setValue(param_type const newValue)
{
    setState(newValue ? buttonDown : buttonNormal);
}

void TriggerButton::mouseDown(juce::MouseEvent const &e)
{
    if (!hasDirectFocus())
    {
        grabKeyboardFocus();
    }
    else if (!isLFOEnabled())
    {
        BitmapButton::mouseDown(e);
        moduleParameterChanged();
    }
}

void TriggerButton::mouseUp(juce::MouseEvent const &e) noexcept
{
    if (!isLFOEnabled())
    {
        BitmapButton::mouseUp(e);
        moduleParameterChanged();
    }
}

void TriggerButton::paintButton(juce::Graphics &graphics, bool const isMouseOverButton,
                                bool const isButtonDown)
{
    unsigned int const imageWidth(51);
    unsigned int const imageHeight(51);
    LE_ASSERT(getCurrentImage().getWidth() == imageWidth);
    LE_ASSERT(getCurrentImage().getHeight() == imageHeight);
    Detail::paintTextButton(*this, graphics, 0, imageHeight + 2, (ModuleUI::width - imageWidth) / 2,
                            0, isMouseOverButton, isButtonDown);
    if (this->hasDirectFocus())
    {
        paintImage(graphics, resourceBitmap<ModuleKnobSelected>(),
                   (ModuleUI::width - imageWidth) / 2 - 1, -1);
    }
}

ModuleKnob::ModuleKnob(juce::Component &parent, unsigned int const x, unsigned int const y)
    : Knob(parent, x, y, marginForGlow * 2,
           std::max<unsigned int>(marginForGlow * 2, spaceForText)),
      pImageStrip_(nullptr)
{
    //...mrmlj...LE_ASSERT( imageStrip.getHeight() / numberOfKnobSubbitmaps == 50 );
    //...mrmlj...LE_ASSERT( imageStrip.getWidth ()                          == 50 );

    setScrollWheelEnabled(false);
}

void ModuleKnob::setupForParameter(juce::Image const &imageStrip,
                                   Quantization const quantizationType,
                                   std::uint8_t const quantizationStep)
{
    auto const &info(control().info());
    Knob::setupForParameter(info.name, imageStrip, info.default_);
    //LE_ASSERT( !isLFOEnabled() ); //...mrmlj...when turning the GUI on or off...
    setDoubleClickReturnValue(!isLFOEnabled(), info.default_);
    quantization_ = quantizationType;
    pImageStrip_ = &imageStrip;
    switch (quantization_)
    {
    case Fixed:
        setRange(info.minimum, info.maximum, quantizationStep);
        break;
    case FrequencyInHertz:
        LE_ASSERT(quantizationStep == 1);
        break;
    case TimeInMilliseconds:
        LE_ASSERT(quantizationStep == 0 || quantizationStep == 1);
        break;
        LE_DEFAULT_CASE_UNREACHABLE();
    }
}

void ModuleKnob::mouseDown(juce::MouseEvent const &event) noexcept
{
    // Implementation note:
    //   In order for the base class to handle the mouseDown() event, the
    // control has to be disabled afterwards.
    //                                        (09.12.2011.) (Domagoj Saric)
    Knob::mouseDown(event);
    setEnabled(!isLFOEnabled());
}

void ModuleKnob::mouseUp(juce::MouseEvent const &event) noexcept
{
    LE_ASSERT(isEnabled() == !isLFOEnabled());
    Knob::mouseUp(event);
    setEnabled(true);
}

void ModuleKnob::paint(juce::Graphics &graphics)
{
    unsigned int const imageWidth(pImageStrip_->getWidth());
    unsigned int const imageHeight(imageWidth);

    if (!control().isLFOEnabled() || shouldUpdateLFOControl(control()))
        Knob::paint(*pImageStrip_, marginForGlow, marginForGlow, graphics);
    else
        paintImage(graphics, resourceBitmap<ModuleKnobLFOed>(), marginForGlow, marginForGlow);
    if (this->hasDirectFocus())
    {
        juce::Image const &selection(imageWidth < 51 ? resourceBitmap<SmallModuleKnobSelected>()
                                                     : resourceBitmap<ModuleKnobSelected>());
        LE_ASSERT(selection.getWidth() == selection.getHeight());
        LE_ASSERT(unsigned(selection.getWidth()) == imageWidth + 2);
        unsigned int const selectionWidth(imageWidth + 2);
        unsigned int const xy(marginForGlow - (selectionWidth - imageWidth) / 2);
        paintImage(graphics, selection, xy, xy);
    }

    graphics.setColour(juce::Colours::lightgrey);
    {
        juce::Font font(Theme::singleton().whiteFont());
        font.setHeight(10);
        graphics.setFont(font);
    }
    graphics.drawFittedText(getName(), 0, imageHeight + marginForGlow + (marginForGlow / 2),
                            getWidth(), 12, juce::Justification::horizontallyCentred, 2, 0.6f);
}

void ModuleKnob::valueChanged() noexcept
{
    LE_ASSERT(isMouseOverOrDragging());
    moduleParameterChanged();
}

void ModuleKnob::lfoStateChanged()
{
    /// \note JUCE 8 split the out-parameter off into isDoubleClickReturnEnabled();
    /// getDoubleClickReturnValue() now just returns the value, which is all this
    /// wanted -- the flag it had to pass a variable for was discarded.
    ///                                       (28.07.2026.) (SW port)
    double const defaultValue(getDoubleClickReturnValue());
    setDoubleClickReturnValue(!isLFOEnabled(), defaultValue);
    syncMouseWheelAndLFOState();
}

void ModuleKnob::updateForEngineSetupChanges(Engine::Setup const &engineSetup)
{
    ModuleKnob::param_type quantization;
    switch (quantization_)
    {
    case Fixed:
        return;
    case FrequencyInHertz:
        quantization = engineSetup.frequencyRangePerBin<ModuleKnob::param_type>();
        break;
    case TimeInMilliseconds:
        quantization = engineSetup.stepTime() * 1000;
        break;
        LE_DEFAULT_CASE_UNREACHABLE();
    }
    ParameterInfo const &parameterInfo(control().info());
    param_type const minimum(parameterInfo.minimum);
    param_type const maximum(parameterInfo.maximum);

    /// \note There is nothing to quantise against until the engine has been set
    /// up: with no sample rate there is no step time and no bin width, so
    /// stepTime() is zero and every assumption below is false. That is reachable
    /// rather than theoretical -- a session restored before activate() builds its
    /// module GUIs against an empty Setup, which is the "quantization > 0"
    /// assertion a standalone hits on startup -- and it is an ordering, not an
    /// error. SpectrumWorxEditor::updateForEngineSetupChanges() re-ranges every
    /// module once a real setup exists, which activate() now asks it to do.
    ///
    ///   A quantum as coarse as the parameter's whole range is the same problem
    /// from the other end, and it is what a large FFT size at a low sample rate
    /// produces for a parameter measured in milliseconds. Leaving the range alone
    /// beats deriving one whose minimum has been rounded up past its maximum.
    ///                                       (29.07.2026.) (SW port)
    if ((quantization <= 0) || (quantization >= maximum))
        return;

    using namespace Math::PositiveFloats;
    LE_ASSUME(minimum >= 0);
    LE_ASSUME(maximum > 0);
    LE_ASSUME(quantization > 0);
    LE_ASSUME(maximum > quantization);
    bool const quantumAsMinimum((minimum < quantization) && (minimum != 0));
    double const adjustedMinimum(
        quantumAsMinimum ? quantization
                         : Math::convert<param_type>(ceil(minimum / quantization)) * quantization);
    double const adjustedMaximum(Math::convert<param_type>(floor(maximum / quantization)) *
                                 quantization);
    LE_ASSERT(adjustedMinimum >= minimum);
    LE_ASSERT(adjustedMaximum <=
              maximum + 250 * std::numeric_limits<float>::epsilon()); //...mrmlj...
    setRange(adjustedMinimum, adjustedMaximum, quantization);
}

void ModuleKnob::moduleControlActivated() { syncMouseWheelAndLFOState(); }
void ModuleKnob::moduleControlDeactivated() { setScrollWheelEnabled(false); }
void ModuleKnob::syncMouseWheelAndLFOState() { setScrollWheelEnabled(!isLFOEnabled()); }

#ifdef __GNUC__ //...mrmlj... GCC 4.6, Clang 2.8-3.2
unsigned int const ModuleKnob::spaceForText /* = 18*/;
#endif // __GNUC__

DiscreteParameter::DiscreteParameter(juce::Component &parent, unsigned int const x,
                                     unsigned int const y)
    : ComboBox(parent, resourceBitmap<ModuleCombo>(), resourceBitmap<ModuleComboOn>())
{
    setName(control().name());
    DiscreteParameter::setTopLeftPosition(x, y);
    LE_ASSERT(control().info().default_ == 0);
}

void DiscreteParameter::mouseDown(juce::MouseEvent const &)
{
    if (!hasDirectFocus())
    {
        grabKeyboardFocus();
    }
    else if (!isLFOEnabled())
    {
        /// \note The menu is asynchronous now, so the notification happens in
        /// the callback rather than on the next line. The SafePointer matters:
        /// a module can be ejected while its menu is down.
        ///                                   (28.07.2026.) (SW port)
        ComboBox::showMenu([self = juce::Component::SafePointer<DiscreteParameter>(this)](
                               bool const valueChanged) {
            if (self && valueChanged)
                self->moduleParameterChanged();
        });
    }
}

void DiscreteParameter::focusChanged() { repaint(); }

#pragma warning(push)
#pragma warning(disable : 4355) // 'this' used in base member initializer list.

ModuleUI::ModuleUI(SpectrumWorxEditor &editor, LE::Utility::IntrusivePtr<SW::Module> pModule,
                   std::uint8_t const slotIndex)
    : editor_(editor), pModule_(std::move(pModule)),
      bypass_(*this, resourceBitmap<ModuleMuted>(), resourceBitmap<ModuleOn>()),
      eject_(*this, resourceBitmap<Eject>(), resourceBitmap<Eject>(),
             juce::Colours::darkgrey.withAlpha(0.4f))
{
    LE_ASSERT(isThisTheGUIThread() ||
              juce::MessageManager::getInstance()->currentThreadHasLockedMessageManager());
    LE_ASSERT(pModule_);

    LE_ASSERT(resourceBitmap<ModuleBgSelected>().getWidth() ==
              resourceBitmap<ModuleBg>().getWidth());
    LE_ASSERT(resourceBitmap<ModuleBgSelected>().getHeight() ==
              resourceBitmap<ModuleBg>().getHeight());
    LE_ASSERT(resourceBitmap<ModuleBgSelected>().getWidth() == width);
    LE_ASSERT(resourceBitmap<ModuleBgSelected>().getHeight() == height);

    setSize(width, height);

    bypass_.setTopLeftPosition((ModuleUI::width / 2) - (bypass_.getWidth() / 2),
                               ModuleUI::height - 26 - resourceBitmap<ModuleOn>().getHeight());

    eject_.setTopLeftPosition((ModuleUI::width - eject_.getWidth()) / 2, -3);

    bypass_.addListener(this);
    eject_.addListener(this);

    setMouseClickGrabsKeyboardFocus(true);
    setWantsKeyboardFocus(true);

    //...mrmlj...for testing...
    //juce::Desktop::getInstance().getAnimator().animateComponent
    //(
    //    this,
    //    juce::Rectangle<int>
    //    (
    //        myHorizontalOffset, verticalOffset,
    //        width             , height
    //    ),
    //    0, 200, false, 0, 0
    //);

    LE_ASSERT_MSG(unsigned(this->getNumChildComponents()) == baseWidgets,
                  "Unexpected number of child widgets before the effect's own controls.");

    ////////////////////////////////////////////////////////////////////////////
    // The effect's own controls, and everything Module::createGUI() used to do
    // after building them.
    ////////////////////////////////////////////////////////////////////////////

    /// \note Parented before the effect's controls are built, and invisible until
    /// the caller shows it. Several of them walk `getParentComponent()` up to the
    /// editor as they are constructed -- `SpectrumWorxEditor::fromChild()` -- and
    /// a strip that is not in the hierarchy yet has nothing to walk. The old
    /// createGUI() did the same thing with an `editor.addChildComponent()` under
    /// `#ifndef NDEBUG`, for the same reason and only in a checked build.
    ///                                       (02.08.2026.) (SW port)
    editor_.addChildComponent(this);

    pWidgets_ = createModuleWidgets(module().effectTypeIndex(), *this);
    LE_ASSERT_MSG(pWidgets_ != nullptr, "No widgets for this effect index.");

    LE_ASSERT_MSG(getNumChildComponents() ==
                      (baseWidgets + module().numberOfEffectSpecificParameters()),
                  "Unexpected number of child widgets at end of ModuleUI constructor.");

    updateForEngineSetupChanges(editor_.engineSetup());

    /// \note A strip is never selected at the moment it is built, so its shared
    /// parameter controls are not showing and do not need updating.
    ///                                       (07.02.2014.) (Domagoj Saric)
    LE_ASSERT(!selected());
    setBypass(module().bypass());

    auto const effectParameters(module().numberOfEffectSpecificParameters());
    for (std::uint8_t parameter(0); parameter < effectParameters; ++parameter)
        setEffectParameter(parameter, module().getEffectParameter(parameter), AutomationOrPreset);

    moveToSlot(slotIndex);
}

#pragma warning(pop)

ModuleUI::~ModuleUI()
{
    LE_ASSERT(isThisTheGUIThread() ||
              juce::MessageManager::getInstance()->currentThreadHasLockedMessageManager());

    if (selected())
    {
        //...mrmlj...
        //LE_ASSERT( hasFocus() || editor()./*...mrmlj...sharedModuleControls().hasFocus()*/ sharedModuleControlsActive() );

        // Implementation note:
        //   Unforunately moveKeyboardFocusToSibling() does not just select the
        // next ModuleUI, if any, but also its 'first' control which is
        // undesired so we simply do nothing for now.
        //                                    (08.07.2011.) (Domagoj Saric)
        //moveKeyboardFocusToSibling( false );

        this->setWantsKeyboardFocus(false);
        editor().moduleDeactivated();
        editor().pSelectedModule_ = nullptr;
    }
    else
    {
        LE_ASSERT(!hasFocus());
    }

    fadeOutComponent(*this, 0, 600, true);
}

void ModuleUI::setUpForEffect(char const *const effectName, char const *const effectDescription)
{
    LE_ASSERT(getName().isEmpty());
    LE_ASSERT(description_.isEmpty());
    setName(effectName);
    description_ = effectDescription;
}

void ModuleUI::moveToSlot(std::uint8_t const slotIndex)
{
    std::uint16_t const myHorizontalOffset(horizontalOffset + slotIndex * (width + distance));
    setTopLeftPosition(myHorizontalOffset, verticalOffset);
}

void ModuleUI::paint(juce::Graphics &graphics)
{
    bool const isActive(selected());
    graphics.setOpacity(isActive ? 1.0f : 0.5f);
    paintImage(graphics,
               isActive ? resourceBitmap<ModuleBgSelected>() : resourceBitmap<ModuleBg>());
    graphics.setColour(Theme::singleton().blueColour());
    graphics.drawHorizontalLine(height - 30, static_cast<float>(ModuleUI::border),
                                Math::convert<float>(getWidth() - ModuleUI::border));

    graphics.setFont(Theme::singleton().whiteFont());
    graphics.drawFittedText(getName(), 3, height - 30, width - 6, 28, juce::Justification::centred,
                            3, 0.6f);
}

void ModuleUI::mouseDrag(juce::MouseEvent const &event)
{
    if (event.mods.isLeftButtonDown())
        editor().moduleDrag(*this, event);
}

void ModuleUI::mouseUp(juce::MouseEvent const &event) noexcept
{
    editor().moduleDragEnd(*this, event);
}

void ModuleUI::mouseEnter(juce::MouseEvent const &)
{
    if (selectionTracksMouseMovements())
        activate();
}

void ModuleUI::mouseExit(juce::MouseEvent const &event) noexcept
{
    /// \note In some strange cases (e.g. while a ComboBox drop down menu is
    /// open and the mouse is moved over a module) JUCE will call mouseExit()
    /// without first calling mouseEnter().
    ///                                       (24.05.2012.) (Domagoj Saric)
    if (!editor().selectedModule())
        return;

    if (selectionTracksMouseMovements() &&
        !juce::Rectangle<int>(0, 0, width, height).contains(event.x, event.y))
        deactivate();
}

void ModuleUI::focusGained(FocusChangeType)
{
    activate();
    LE_ASSERT(selected());
}

void ModuleUI::focusLost(FocusChangeType)
{
    // Implementation note:
    //   If only transferring focus to a subcontrol or to the shared controls do
    // not deactivate.
    //                                        (14.11.2011.) (Domagoj Saric)
    if (hasFocus() || editor().sharedModuleControlsActiveAndFocused())
        return;

    //...mrmlj...rethink this focus changing logic and assumptions
    //LE_ASSERT( selected() );
    if (selected())
        deactivate();
}

void ModuleUI::focusOfChildComponentChanged(FocusChangeType const changeType)
{
    if (hasFocus())
        ModuleUI::focusGained(changeType);
    else
        ModuleUI::focusLost(changeType);
}

void ModuleUI::activate()
{
    LE_ASSERT(hasFocus() || selectionTracksMouseMovements());
    if (this->selected())
        return;

    // Implementation note:
    //   If the previously active module wasn't actually focused but the shared
    // controls it will not deactivate (and thus repaint) itself in the
    // focusLost() handler so a repaint must be forced here.
    //                                        (14.11.2011.) (Domagoj Saric)
    if (editor().selectedModule())
        editor().selectedModule()->repaint();

    editor().pSelectedModule_ = this;
    editor().moduleActivated();
    repaint();
}

void ModuleUI::deactivate()
{
    LE_ASSERT(selected());
    LE_ASSERT(!hasFocus());

    editor().moduleDeactivated();
    editor().pSelectedModule_ = nullptr;
    repaint();
}

bool ModuleUI::selectionTracksMouseMovements() const
{
    return (Theme::settings().moduleUIMouseOverReaction == Theme::WhenParentOrNothingSelected) &&
           ModuleControlBase::noModuleOrModuleControlFocused(editor());
}

namespace
{
auto const bypassIndex = LE::Parameters::IndexOf<Effects::BaseParameters::Parameters,
                                                 Effects::BaseParameters::Bypass>::value;

void setParameterControl(ModuleControlBase &control, float const parameterValue,
                         ModuleUI::ParameterChangeSource const source)
{
    if ((source != ModuleUI::LFOValue) || shouldUpdateLFOControl(control))
    {
        control.setValue(parameterValue);
    }
    if ((source == ModuleUI::AutomationOrPreset) && control.isActive())
    {
        SpectrumWorxEditor::fromChild(control.widget()).updateActiveControlValue();
    }
}
} // anonymous namespace

void ModuleUI::setBaseParameter(std::uint8_t const sharedParameterIndex, float const parameterValue,
                                ParameterChangeSource const source)
{
    if (sharedParameterIndex == bypassIndex)
    {
        LE_ASSUME(source == AutomationOrPreset);
        setBypass(Math::convert<bool>(parameterValue));
    }
    else
    {
        holdSharedControls(true);
        if (selected())
            setParameterControl(sharedControls().controlForParameter(sharedParameterIndex),
                                parameterValue, source);
        holdSharedControls(false);
    }
}

void ModuleUI::setEffectParameter(std::uint8_t const effectParameterIndex,
                                  float const parameterValue, ParameterChangeSource const source)
{
    setParameterControl(effectSpecificParameterControl(effectParameterIndex), parameterValue,
                        source);
}

void ModuleUI::setParameter(std::uint8_t const parameterIndex, float const parameterValue,
                            ParameterChangeSource const source)
{
    if (parameterIndex < Effects::BaseParameters::Parameters::static_size)
        setBaseParameter(parameterIndex, parameterValue, source);
    else
        setEffectParameter(Engine::ModuleParameters::effectSpecificParameterIndex(parameterIndex),
                           parameterValue, source);
}

void ModuleUI::setBypass(bool const bypass) { bypass_.setValue(bypass); }

LE_COLD void ModuleUI::updateForEngineSetupChanges(Engine::Setup const &engineSetup)
{
    /// \note SharedModuleControls are updated in/by
    /// SpectrumWorxEditor::updateForEngineSetupChanges().
    ///                                       (13.02.2014.) (Domagoj Saric)
    std::uint8_t const numberOfControls(module().numberOfEffectSpecificParameters());
    for (std::uint8_t parameterIndex(0); parameterIndex < numberOfControls; ++parameterIndex)
    {
        effectSpecificParameterControl(parameterIndex).updateForEngineSetupChanges(engineSetup);
    }
}

void ModuleUI::updateLFOParameter(std::uint8_t const parameterIndex,
                                  std::uint8_t const lfoParameterIndex,
                                  Plugins::AutomatedParameterValue const value)
{
    //...mrmlj...value unused - updated from the LFO...
    editor().updateLFO(*this, parameterIndex, lfoParameterIndex, value);
}

void ModuleUI::buttonClicked(juce::Button *LE_RESTRICT const pButton)
{
    if (pButton == &bypass_)
    {
        float const value(Math::convert<float>(bypass_.getValue()));
        editor().updateModuleParameterAndNotifyHost(*this, bypassIndex, value);
    }
    else
    {
        LE_ASSERT(pButton == &eject_);
        //...mrmlj...investigate why this doesn't work when placed inside the ModuleUI destructor...
        /// \note Answered, and it is still true: JUCE moves the focus when a
        /// strip is destroyed and delivers the loss to whichever control had it,
        /// which re-enters the editor through a control that is going. What has
        /// changed is that the strip is no longer destroyed inside this call, so
        /// this deactivation is now only half the job -- see
        /// SpectrumWorxEditor::detachFrom(), which does the other half where the
        /// strip actually dies. This one stays because the module is still in the
        /// chain here, which is what ending the host's gesture needs.
        ///                                   (02.08.2026.) (SW port)
        auto *const pActiveControl(editor().activeControl());
        if (pActiveControl && (this == &pActiveControl->moduleUI()))
            editor().moduleControlDectivated(*pActiveControl);
        editor().removeModule(*this);
    }
}

/// \note Was a `polymorphicDowncast` of `getParentComponent()`, with an assertion
/// that there was one. See the note on the constructor: the region is written to
/// before it is parented, so the editor cannot be recovered that way.
///                                           (02.08.2026.) (SW port)
SpectrumWorxEditor &ModuleUI::editor() { return editor_; }

SpectrumWorxEditor const &ModuleUI::editor() const { return editor_; }

bool ModuleUI::selected() const { return this == editor().selectedModule(); }

SharedModuleControls &ModuleUI::sharedControls()
{
    LE_ASSERT_MSG(selected(), "Inactive modules do not have an active shared controls UI.");
    return editor().sharedModuleControls();
}

void ModuleUI::holdSharedControls(bool const doHold) const
{
    LE_ASSERT(editor().holdSharedModuleControls_ != doHold);
    //...mmrlj...LE_ASSERT( selected() == editor().sharedModuleControlsActive() );
    editor().holdSharedModuleControls_ = doHold;
}

bool ModuleUI::sharedControlsLocked() const
{
    LE_ASSERT_MSG(selected(), "Inactive modules do not have an active shared controls UI.");
    return editor().holdSharedModuleControls_;
}

ModuleControlBase &ModuleUI::effectSpecificParameterControl(std::uint8_t const parameterIndex)
{
    std::uint8_t const actualChildIndex(parameterIndex + baseWidgets);
    LE_ASSERT_MSG(actualChildIndex < unsigned(this->getNumChildComponents()),
                  "Parameter index out of range.");
    juce::Component *LE_RESTRICT const pWidget(this->getChildComponent(actualChildIndex));
    LE_ASSUME(pWidget);
    return ModuleControlBase::controlForWidget(*pWidget);
}
ModuleControlBase const &
ModuleUI::effectSpecificParameterControl(std::uint8_t const parameterIndex) const
{
    return const_cast<ModuleUI &>(*this).effectSpecificParameterControl(parameterIndex);
}

ModuleUI::Module &ModuleUI::module()
{
    LE_ASSUME(pModule_.get() != nullptr);
    return *pModule_;
}
ModuleUI::Module const &ModuleUI::module() const { return const_cast<ModuleUI &>(*this).module(); }

Utility::CriticalSectionLock ModuleUI::getProcessingLock() const
{
    return editor().getProcessingLock();
}

//------------------------------------------------------------------------------
namespace Detail
{
ModuleWidgetConstructionState::ModuleWidgetConstructionState(ModuleUI &parent)
    : parent(parent), yOffset(14),
      parameterIndex(Engine::ModuleParameters::numberOfLFOBaseParameters)
{
}

EmptyWidgets::EmptyWidgets(ModuleWidgetConstructionState const &state)
{
    LE_ASSERT_MSG(state.yOffset < static_cast<unsigned int>(state.parent.getHeight()),
                  "You added more parameters/controls to the effect than can fit into its UI");
    (void)state;
}

template <>
ModuleWidgetHolder<ModuleLEDTextButton>::ModuleWidgetHolder(ModuleWidgetConstructionState &state)
    : widget(state.parent, state.parent, ModuleUI::border, state.yOffset, state.parameterIndex++)
{
    state.yOffset += widget.getHeight() + 6;
}

template <>
ModuleWidgetHolder<TriggerButton>::ModuleWidgetHolder(ModuleWidgetConstructionState &state)
    : widget(state.parent, state.parent, 0, state.yOffset + 4, state.parameterIndex++)
{
    state.yOffset += widget.getHeight() + 6;
}

template <>
ModuleWidgetHolder<DiscreteParameter>::ModuleWidgetHolder(ModuleWidgetConstructionState &state)
    : widget(state.parent, state.parent, ModuleUI::border, state.yOffset += 4,
             state.parameterIndex++)
{
    state.yOffset += widget.getHeight() + 4;
}

template <>
ModuleWidgetHolder<ModuleKnob>::ModuleWidgetHolder(ModuleWidgetConstructionState &state)
    : widget(state.parent, state.parent, ModuleUI::border, state.yOffset, state.parameterIndex++)
{
    state.yOffset += widget.getHeight();
    //...mrmlj...
    LE_ASSERT(resourceBitmap<ModuleKnobStrip>().getWidth() == 51);
    state.yOffset += 51;
}

} // namespace Detail

//------------------------------------------------------------------------------
} // namespace GUI
//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
