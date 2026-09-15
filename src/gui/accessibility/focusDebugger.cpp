////////////////////////////////////////////////////////////////////////////////
///
/// focusDebugger.cpp
/// -----------------
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "focusDebugger.hpp"

#include "traversal.hpp"

#include "gui/colourMap.hpp"

#include <iostream>

namespace LE::SW::GUI::Accessibility
{

class FocusDebugger::Box final : public juce::Component
{
  public:
    Box()
    {
        setAccessible(false);
        setWantsKeyboardFocus(false);
        setMouseClickGrabsKeyboardFocus(false);
        setInterceptsMouseClicks(false, false);
        setAlwaysOnTop(true);
        setTitle("Focus Debugger");
    }

  private:
    void paint(juce::Graphics &graphics) override
    {
        auto const red(ColourMap::getColour(ColourMap::FocusDebugger));
        graphics.fillAll(red.withAlpha(0.1f));
        graphics.setColour(red);
        graphics.drawRect(getLocalBounds(), 1);
    }
}; // class FocusDebugger::Box

FocusDebugger::FocusDebugger(juce::Component &debugInto) : debugInto_(debugInto)
{
    juce::Desktop::getInstance().addFocusChangeListener(this);
}

FocusDebugger::~FocusDebugger() { juce::Desktop::getInstance().removeFocusChangeListener(this); }

void FocusDebugger::setEnabled(bool const enabled)
{
    enabled_ = enabled;
    if (enabled_ && !box_)
    {
        box_ = std::make_unique<Box>();
        debugInto_.addChildComponent(*box_);
    }
    if (box_)
        box_->setVisible(false);
    if (enabled_)
        follow(juce::Component::getCurrentlyFocusedComponent());
}

void FocusDebugger::globalFocusChanged(juce::Component *const focused)
{
    if (enabled_)
        follow(focused);
}

void FocusDebugger::follow(juce::Component *const focused)
{
    if (!box_ || !focused || !debugInto_.isParentOf(focused))
        return;

    auto const bounds(debugInto_.getLocalArea(focused->getParentComponent(), focused->getBounds()));

    std::cout << "FD : [" << std::hex << focused << std::dec << "] '" << focused->getTitle()
              << "' name='" << focused->getName() << "' @ " << bounds.toString()
              << " traversal=" << traversalKey(*focused, debugInto_) << std::endl;

    box_->setBounds(bounds);
    box_->setVisible(true);
    box_->toFront(false);
}

} // namespace LE::SW::GUI::Accessibility
