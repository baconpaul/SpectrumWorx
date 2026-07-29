////////////////////////////////////////////////////////////////////////////////
///
/// \file stubEditor.cpp
/// -------------------
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "stubEditor.hpp"
#include "spectrumWorxCLAP.hpp"

#include "le/spectrumworx/effects/configuration/effectNames.hpp"

#include <sst/plugininfra/version_information.h>
//------------------------------------------------------------------------------
namespace LE::SW
{
//------------------------------------------------------------------------------

StubEditor::StubEditor(SpectrumWorxCLAP &plugin) : plugin_(plugin)
{
    for (std::uint8_t slot(0); slot < Constants::maxNumberOfModules; ++slot)
    {
        auto button(std::make_unique<juce::TextButton>());
        button->onClick = [this, slot] {
            plugin_.cycleModuleFromUI(slot);
            refreshLabels();
        };
        addAndMakeVisible(*button);
        slotButtons_[slot] = std::move(button);
    }

    rescanButton_ = std::make_unique<juce::TextButton>("Rescan parameters");
    rescanButton_->onClick = [this] { plugin_.requestRescanFromUI(); };
    addAndMakeVisible(*rescanButton_);

    refreshLabels();
    setSize(560, 280);
}

StubEditor::~StubEditor() = default;

void StubEditor::refreshLabels()
{
    for (std::uint8_t slot(0); slot < Constants::maxNumberOfModules; ++slot)
    {
        auto const effect(plugin_.effectIn(slot));
        slotButtons_[slot]->setButtonText(
            juce::String("Module ") + juce::String(slot + 1) + ": " +
            (effect == noModule ? juce::String("(empty)")
                                : juce::String(Effects::effectName(
                                      static_cast<std::uint8_t>(effect)))));
    }
}

void StubEditor::paint(juce::Graphics &g)
{
    g.fillAll(juce::Colour(0xFF16181C));

    g.setColour(juce::Colour(0xFF3AA0FF));
    g.drawRect(getLocalBounds().reduced(4), 2);

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(26.0f));
    g.drawText("SpectrumWorx", getLocalBounds().withHeight(48).reduced(20, 0),
               juce::Justification::centredLeft);

    g.setColour(juce::Colours::grey);
    g.setFont(juce::FontOptions(13.0f));
    g.drawText(juce::String("walking skeleton \xe2\x80\x94 ") +
                   sst::plugininfra::VersionInformation::project_version_and_hash,
               getLocalBounds().withTrimmedTop(46).withHeight(20).reduced(20, 0),
               juce::Justification::centredLeft);
    g.drawText(juce::String(plugin_.parameterCount()) + " parameters",
               getLocalBounds().withTrimmedBottom(8).removeFromBottom(20).reduced(20, 0),
               juce::Justification::centredLeft);
}

void StubEditor::resized()
{
    auto area(getLocalBounds().reduced(20).withTrimmedTop(74).withTrimmedBottom(28));
    for (auto &button : slotButtons_)
        if (button)
            button->setBounds(area.removeFromTop(26).reduced(0, 2));

    area.removeFromTop(8);
    rescanButton_->setBounds(area.removeFromTop(26));
}

//------------------------------------------------------------------------------
} // namespace LE::SW
//------------------------------------------------------------------------------
