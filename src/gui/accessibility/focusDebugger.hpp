////////////////////////////////////////////////////////////////////////////////
///
/// \file focusDebugger.hpp
/// -----------------------
///
///   sst-jucegui's FocusDebugger: a red box on the focus, and a line on stdout.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef focusDebugger_hpp__3E9D5A62_B147_4C08_8F2D_61A0C7E4B593
#define focusDebugger_hpp__3E9D5A62_B147_4C08_8F2D_61A0C7E4B593
//------------------------------------------------------------------------------
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

namespace LE::SW::GUI::Accessibility
{

class FocusDebugger final : private juce::FocusChangeListener
{
  public:
    explicit FocusDebugger(juce::Component &debugInto);
    ~FocusDebugger() override;

    FocusDebugger(FocusDebugger const &) = delete; // makes non-copyable

    void setEnabled(bool);
    bool isEnabled() const { return enabled_; }

  private:
    void globalFocusChanged(juce::Component *focused) override;

    // the component itself rather than a rectangle, so a panel swap cannot strand it
    void follow(juce::Component *focused);

    class Box;

    juce::Component &debugInto_;
    std::unique_ptr<Box> box_;
    bool enabled_{false};
}; // class FocusDebugger

} // namespace LE::SW::GUI::Accessibility

#endif // focusDebugger_hpp
