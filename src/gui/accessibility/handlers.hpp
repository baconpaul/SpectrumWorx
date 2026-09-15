////////////////////////////////////////////////////////////////////////////////
///
/// \file handlers.hpp
/// ------------------
///
///   What the painted widgets tell a screen reader. \see doc/tech/accessibility.md
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef handlers_hpp__7B40E2C9_1F85_4A3D_A6E1_9D2C58B03F74
#define handlers_hpp__7B40E2C9_1F85_4A3D_A6E1_9D2C58B03F74
//------------------------------------------------------------------------------
#include "keyboardEdits.hpp"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

namespace LE::SW::GUI
{
class ParameterMenu;
}

namespace LE::SW::GUI::Accessibility
{

using TextFunction = std::function<juce::String()>;

// a button, toggle or drop-down; functions, as a strip can move under its handler
struct ButtonAccess
{
    juce::AccessibilityRole role{juce::AccessibilityRole::button};
    TextFunction title;
    TextFunction description;
    TextFunction value;
    /// set for a toggle
    std::function<bool()> isOn;
    std::function<void()> press;
    std::function<void()> showMenu;
};

std::unique_ptr<juce::AccessibilityHandler> makeButtonHandler(juce::Component &, ButtonAccess);

std::unique_ptr<juce::AccessibilityHandler> makeGroupHandler(juce::Component &, TextFunction title,
                                                             TextFunction description = {});

std::unique_ptr<juce::AccessibilityHandler> makeLabelHandler(juce::Component &, TextFunction text);

/// \brief A container a screen reader should see through.
std::unique_ptr<juce::AccessibilityHandler> makeIgnoredHandler(juce::Component &);

// for an edit the user made, never one an LFO or the host made
void announceValueChange(juce::Component &);

/// \brief Painted text, read out: nothing to see and nothing to click.
class AccessibleLabel final : public juce::Component
{
  public:
    explicit AccessibleLabel(TextFunction text);

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

  private:
    TextFunction text_;
}; // class AccessibleLabel

// a screen reader sees the proportion, so an increment is even on a skewed range
class SliderAccess
{
  public:
    enum class Thumb : std::uint8_t
    {
        only,
        lower,
        upper
    };

    virtual juce::Slider &accessibleSlider() = 0;
    virtual ParameterMenu &accessibleParameter() = 0;

    /// which thumb the keyboard moves
    virtual Thumb keyboardThumb() const { return Thumb::only; }
    virtual void chooseKeyboardThumb(Thumb) {}

    /// one whole edit, announced unless the widget's own path announces it
    virtual void applyAccessibleValue(double value) = 0;

    /// as the widget's own right press asks
    virtual bool menuSkipsDefault() const { return false; }

    bool handleAccessibleKey(juce::KeyPress const &);
    std::unique_ptr<juce::AccessibilityHandler> createSliderHandler();

    double thumbValue();
    double thumbProportion();
    void applyProportion(double proportion);
    double accessibleStep();

  protected:
    ~SliderAccess() = default;

  private:
    void nudge(int direction, KeyEdit::Modifier);
    double snapped(double value);
}; // class SliderAccess

} // namespace LE::SW::GUI::Accessibility

#endif // handlers_hpp
