////////////////////////////////////////////////////////////////////////////////
///
/// \file environmentPage.cpp
/// -------------------------
///
///   The page that has no dependencies, so that a harness which shows nothing
/// else still tells you whether the window, the message loop, the renderer and
/// the display metrics are working. Worth keeping after the GUI ports: when a
/// widget page misbehaves, this is what says whether the fault is above or
/// below the widget.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "../page.hpp"

#include <juce_gui_basics/juce_gui_basics.h>
//------------------------------------------------------------------------------
namespace
{
//------------------------------------------------------------------------------

class EnvironmentPage final : public juce::Component
{
  public:
    EnvironmentPage() { setSize(720, 460); }

    void paint(juce::Graphics &graphics) override
    {
        graphics.fillAll(juce::Colour(0xFF1E1E1E));

        auto bounds(getLocalBounds().reduced(24));

        graphics.setColour(juce::Colours::white);
        graphics.setFont(juce::FontOptions(22.0f));
        graphics.drawText("SpectrumWorx UI harness", bounds.removeFromTop(34),
                          juce::Justification::topLeft);

        graphics.setColour(juce::Colours::grey);
        graphics.setFont(juce::FontOptions(13.0f));
        for (auto const &line : facts())
            graphics.drawText(line, bounds.removeFromTop(20), juce::Justification::topLeft);

        bounds.removeFromTop(12);
        graphics.setColour(juce::Colours::white);
        graphics.drawText("pages", bounds.removeFromTop(20), juce::Justification::topLeft);
        graphics.setColour(juce::Colours::grey);
        for (auto const &page : SWShowUI::pages())
            graphics.drawText(juce::String(page.name) + "   -   " + page.description,
                              bounds.removeFromTop(18).withTrimmedLeft(16),
                              juce::Justification::topLeft);

        // A gradient and a curve, purely so that a broken software renderer is
        // visible rather than merely suspected.
        auto const swatch(getLocalBounds().removeFromBottom(56).reduced(24, 8));
        graphics.setGradientFill(
            juce::ColourGradient(juce::Colours::orange, swatch.getTopLeft().toFloat(),
                                 juce::Colours::blue, swatch.getTopRight().toFloat(), false));
        graphics.fillRoundedRectangle(swatch.toFloat(), 6.0f);
    }

  private:
    static juce::StringArray facts()
    {
        juce::StringArray lines;
        lines.add(juce::SystemStats::getJUCEVersion()); // already reads "JUCE v8.0.12"
        lines.add(juce::SystemStats::getOperatingSystemName() + "   " +
                  juce::SystemStats::getCpuVendor() + " " +
                  juce::String(juce::SystemStats::getNumCpus()) + " cores");

        // Null under --render: there is no window server to ask.
        auto const &displays(juce::Desktop::getInstance().getDisplays());
        if (auto const *const display = displays.getPrimaryDisplay())
            lines.add("primary display " + display->userArea.toString() + " at scale " +
                      juce::String(display->scale, 2));
        else
            lines.add("no display (offscreen)");

        lines.add(juce::String("message thread is ") +
                  (juce::MessageManager::getInstance()->isThisTheMessageThread() ? "current"
                                                                                 : "elsewhere"));
        return lines;
    }
}; // class EnvironmentPage

std::unique_ptr<juce::Component> construct() { return std::make_unique<EnvironmentPage>(); }

SWShowUI::PageRegistration const registration{
    "environment", "window, renderer and display metrics; no SpectrumWorx code", &construct};

//------------------------------------------------------------------------------
} // anonymous namespace
//------------------------------------------------------------------------------
