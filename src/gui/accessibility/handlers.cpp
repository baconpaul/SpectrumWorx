////////////////////////////////////////////////////////////////////////////////
///
/// handlers.cpp
/// ------------
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "handlers.hpp"

#include "gui/gui.hpp"

#include <algorithm>
#include <cmath>

namespace LE::SW::GUI::Accessibility
{

namespace
{
class TextValue final : public juce::AccessibilityTextValueInterface
{
  public:
    explicit TextValue(TextFunction value) : value_(std::move(value)) {}

    bool isReadOnly() const override { return true; }
    juce::String getCurrentValueAsString() const override { return value_(); }
    void setValueAsString(juce::String const &) override {}

  private:
    TextFunction value_;
}; // class TextValue

juce::AccessibilityActions actionsFor(ButtonAccess const &access)
{
    juce::AccessibilityActions actions;
    if (access.press)
    {
        actions.addAction(juce::AccessibilityActionType::press, access.press);
        if (access.isOn)
            actions.addAction(juce::AccessibilityActionType::toggle, access.press);
    }
    if (access.showMenu)
        actions.addAction(juce::AccessibilityActionType::showMenu, access.showMenu);
    return actions;
}

juce::AccessibilityHandler::Interfaces interfacesFor(ButtonAccess const &access)
{
    if (access.value)
        return {std::make_unique<TextValue>(access.value)};
    return {};
}

class ButtonHandler final : public juce::AccessibilityHandler
{
  public:
    ButtonHandler(juce::Component &component, ButtonAccess access)
        : juce::AccessibilityHandler(component, access.role, actionsFor(access),
                                     interfacesFor(access)),
          access_(std::move(access))
    {
    }

    juce::String getTitle() const override
    {
        return access_.title ? access_.title() : juce::AccessibilityHandler::getTitle();
    }

    juce::String getDescription() const override
    {
        return access_.description ? access_.description()
                                   : juce::AccessibilityHandler::getDescription();
    }

    juce::AccessibleState getCurrentState() const override
    {
        auto state(juce::AccessibilityHandler::getCurrentState());
        if (!access_.isOn)
            return state;
        state = state.withCheckable();
        return access_.isOn() ? state.withChecked() : state;
    }

  private:
    ButtonAccess const access_;
}; // class ButtonHandler

class TitledHandler final : public juce::AccessibilityHandler
{
  public:
    TitledHandler(juce::Component &component, juce::AccessibilityRole const role,
                  TextFunction title, TextFunction description)
        : juce::AccessibilityHandler(component, role), title_(std::move(title)),
          description_(std::move(description))
    {
    }

    juce::String getTitle() const override
    {
        return title_ ? title_() : juce::AccessibilityHandler::getTitle();
    }

    juce::String getDescription() const override
    {
        return description_ ? description_() : juce::AccessibilityHandler::getDescription();
    }

  private:
    TextFunction const title_;
    TextFunction const description_;
}; // class TitledHandler
} // anonymous namespace

std::unique_ptr<juce::AccessibilityHandler> makeButtonHandler(juce::Component &component,
                                                              ButtonAccess access)
{
    return std::make_unique<ButtonHandler>(component, std::move(access));
}

std::unique_ptr<juce::AccessibilityHandler>
makeGroupHandler(juce::Component &component, TextFunction title, TextFunction description)
{
    return std::make_unique<TitledHandler>(component, juce::AccessibilityRole::group,
                                           std::move(title), std::move(description));
}

std::unique_ptr<juce::AccessibilityHandler> makeLabelHandler(juce::Component &component,
                                                             TextFunction text)
{
    return std::make_unique<TitledHandler>(component, juce::AccessibilityRole::label,
                                           std::move(text), TextFunction());
}

std::unique_ptr<juce::AccessibilityHandler> makeIgnoredHandler(juce::Component &component)
{
    return std::make_unique<juce::AccessibilityHandler>(component,
                                                        juce::AccessibilityRole::ignored);
}

void announceValueChange(juce::Component &component)
{
    if (auto *const pHandler = component.getAccessibilityHandler())
        pHandler->notifyAccessibilityEvent(juce::AccessibilityEvent::valueChanged);
}

AccessibleLabel::AccessibleLabel(TextFunction text) : text_(std::move(text))
{
    setInterceptsMouseClicks(false, false);
    setWantsKeyboardFocus(false);
}

std::unique_ptr<juce::AccessibilityHandler> AccessibleLabel::createAccessibilityHandler()
{
    return makeLabelHandler(*this, text_);
}

namespace
{
class SliderValue final : public juce::AccessibilityValueInterface
{
  public:
    explicit SliderValue(SliderAccess &access) : access_(access) {}

    bool isReadOnly() const override { return !access_.accessibleParameter().parameterEditable(); }
    double getCurrentValue() const override { return access_.thumbProportion(); }
    void setValue(double const proportion) override { access_.applyProportion(proportion); }

    juce::String getCurrentValueAsString() const override
    {
        return access_.accessibleParameter().accessibleValue();
    }

    void setValueAsString(juce::String const &text) override
    {
        if (access_.accessibleParameter().accessibleSetText(text))
            announceValueChange(access_.accessibleSlider());
    }

    AccessibleValueRange getRange() const override
    {
        return {{0.0, 1.0}, access_.accessibleStep()};
    }

  private:
    SliderAccess &access_;
}; // class SliderValue

class SliderHandler final : public juce::AccessibilityHandler
{
  public:
    explicit SliderHandler(SliderAccess &access)
        : juce::AccessibilityHandler(
              access.accessibleSlider(), juce::AccessibilityRole::slider,
              juce::AccessibilityActions().addAction(
                  juce::AccessibilityActionType::showMenu,
                  [&access] {
                      access.accessibleParameter().showParameterMenuFromKeyboard(
                          access.menuSkipsDefault());
                  }),
              Interfaces{std::make_unique<SliderValue>(access)}),
          access_(access)
    {
    }

    juce::String getTitle() const override
    {
        return access_.accessibleParameter().accessibleName();
    }

  private:
    SliderAccess &access_;
}; // class SliderHandler
} // anonymous namespace

std::unique_ptr<juce::AccessibilityHandler> SliderAccess::createSliderHandler()
{
    return std::make_unique<SliderHandler>(*this);
}

double SliderAccess::thumbValue()
{
    auto &slider(accessibleSlider());
    switch (keyboardThumb())
    {
    case Thumb::lower:
        return slider.getMinValue();
    case Thumb::upper:
        return slider.getMaxValue();
    case Thumb::only:
        break;
    }
    return slider.getValue();
}

double SliderAccess::thumbProportion()
{
    return accessibleSlider().valueToProportionOfLength(thumbValue());
}

// one percent, or a whole step where a step is wider
double SliderAccess::accessibleStep()
{
    auto &slider(accessibleSlider());
    auto const length(slider.getMaximum() - slider.getMinimum());
    if (length <= 0)
        return 0.01;
    return std::max(0.01, slider.getInterval() / length);
}

double SliderAccess::snapped(double const value)
{
    auto &slider(accessibleSlider());
    auto const interval(slider.getInterval());
    auto const minimum(slider.getMinimum());
    auto result(value);
    if (interval > 0)
        result = minimum + interval * std::round((value - minimum) / interval);

    // a two-valued slider's thumbs may meet but not pass
    switch (keyboardThumb())
    {
    case Thumb::lower:
        return std::clamp(result, minimum, slider.getMaxValue());
    case Thumb::upper:
        return std::clamp(result, slider.getMinValue(), slider.getMaximum());
    case Thumb::only:
        break;
    }
    return std::clamp(result, minimum, slider.getMaximum());
}

// a snap that lands back where it started is a step too small for the range
void SliderAccess::applyProportion(double const proportion)
{
    if (!accessibleParameter().parameterEditable())
        return;

    auto &slider(accessibleSlider());
    auto const current(thumbValue());
    auto target(snapped(slider.proportionOfLengthToValue(std::clamp(proportion, 0.0, 1.0))));
    if ((target == current) && (proportion != thumbProportion()) && (slider.getInterval() > 0))
        target =
            snapped(current + slider.getInterval() * ((proportion > thumbProportion()) ? 1 : -1));
    if (target != current)
        applyAccessibleValue(target);
}

// sst-jucegui's keyboard steps: a fortieth of the travel, a tenth of that fine
void SliderAccess::nudge(int const direction, KeyEdit::Modifier const modifier)
{
    auto &slider(accessibleSlider());
    auto const interval(slider.getInterval());

    if ((modifier == KeyEdit::quantized) && (interval > 0))
    {
        auto const target(snapped(thumbValue() + direction * interval));
        if (target != thumbValue())
            applyAccessibleValue(target);
        return;
    }

    auto const step((modifier == KeyEdit::fine) ? 0.0025 : 0.025);
    applyProportion(thumbProportion() + direction * step);
}

bool SliderAccess::handleAccessibleKey(juce::KeyPress const &key)
{
    auto const edit(keyEditFor(key));
    auto &parameter(accessibleParameter());
    auto &slider(accessibleSlider());

    bool const twoThumbs(keyboardThumb() != Thumb::only);

    switch (edit.action)
    {
    case KeyEdit::none:
    case KeyEdit::trigger:
        return false;

    case KeyEdit::openMenu:
    case KeyEdit::openEditor:
        parameter.showParameterMenuFromKeyboard(menuSkipsDefault());
        return true;

    case KeyEdit::previous:
    case KeyEdit::next:
        if (twoThumbs)
        {
            chooseKeyboardThumb((edit.action == KeyEdit::previous) ? Thumb::lower : Thumb::upper);
            return true;
        }
        if (parameter.parameterEditable())
            nudge((edit.action == KeyEdit::next) ? +1 : -1, edit.modifier);
        return true;

    case KeyEdit::increase:
    case KeyEdit::decrease:
        if (parameter.parameterEditable())
            nudge((edit.action == KeyEdit::increase) ? +1 : -1, edit.modifier);
        return true;

    case KeyEdit::toMaximum:
    case KeyEdit::toMinimum:
        if (parameter.parameterEditable())
        {
            auto const target(snapped((edit.action == KeyEdit::toMaximum) ? slider.getMaximum()
                                                                          : slider.getMinimum()));
            if (target != thumbValue())
                applyAccessibleValue(target);
        }
        return true;

    case KeyEdit::toDefault:
        if (parameter.parameterEditable())
        {
            parameter.accessibleResetToDefault();
            announceValueChange(slider);
        }
        return true;
    }
    return false;
}

} // namespace LE::SW::GUI::Accessibility
