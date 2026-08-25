////////////////////////////////////////////////////////////////////////////////
///
/// \file knobsPage.cpp
/// -------------------
///
///   Both knobs, swept across their range.
///
///   They are drawn rather than blitted as of 14/15.08.2026, and what that
/// replaced was five film strips of 127 frames each -- so the artwork was its
/// own contact sheet and anyone could see the whole travel at once. This is that
/// contact sheet for the paint code: the editor knob, then the module knob in
/// both polarities and both sizes, selected and not.
///
///   The editor pages cannot show this. A knob there sits wherever its
/// parameter's default put it -- which for the symmetric ones is dead centre,
/// the one frame where the wedge is a hairline and says least -- and nothing
/// headless can drag it somewhere else.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "../page.hpp"

#include "gui/modules/moduleUI.hpp"
#include "gui/theme.hpp"

#include <juce_gui_basics/juce_gui_basics.h>

#include <iterator>
//------------------------------------------------------------------------------
namespace
{
//------------------------------------------------------------------------------

namespace GUI = LE::SW::GUI;

struct Row
{
    char const *caption;
    unsigned int diameter;
    bool editor;  ///< an EditorKnob rather than a ModuleKnob
    bool bipolar; ///< module knobs only, as are the two below
    GUI::Highlight highlight{GUI::Highlight::None};

    /// \brief Whether this row is an LFO's travel rather than a value.
    ///
    /// \note And the row then sweeps the *band* rather than the value, because
    /// the value is not drawn while there is one -- nine identical knobs would be
    /// the correct picture and a useless one. \see paintModuleKnob().
    bool lfoRange{false};
}; // struct Row

/// \brief A band a third of the travel wide, starting \p position of the way
/// along, which is what a banded row shows at each of its nine steps.
juce::Range<float> lfoBandAt(float const position)
{
    float constexpr width{0.35f};
    auto const start(position * (1 - width));
    return {start, start + width};
}

Row const rows[]{
    {"EditorKnob, 83 px (in, out and mix)", GUI::EditorKnob::diameter, true, false},
    {"ModuleKnob unipolar, 77 px", GUI::ModuleKnob::diameter, false, false},
    {"ModuleKnob bipolar, 77 px", GUI::ModuleKnob::diameter, false, true},
    {"ModuleKnob unipolar, 77 px, hovered", GUI::ModuleKnob::diameter, false, false,
     GUI::Highlight::Hovered},
    {"ModuleKnob unipolar, 77 px, selected", GUI::ModuleKnob::diameter, false, false,
     GUI::Highlight::Selected},
    {"ModuleKnob, 77 px, an LFO's travel swept across it -- no value is drawn",
     GUI::ModuleKnob::diameter, false, false, GUI::Highlight::None, true},
    {"ModuleKnob, 77 px, the same, selected", GUI::ModuleKnob::diameter, false, false,
     GUI::Highlight::Selected, true},
    {"ModuleKnob unipolar, 35 px (gain and wet)", GUI::ModuleKnob::smallDiameter, false, false},
    {"ModuleKnob bipolar, 35 px, selected", GUI::ModuleKnob::smallDiameter, false, true,
     GUI::Highlight::Selected},
};

class KnobsPage final : public juce::Component
{
  public:
    KnobsPage()
    {
        if (!GUI::Theme::haveSingleton())
            GUI::Theme::createSingleton();
        setSize(margin * 2 + steps * cell, margin * 2 + rowCount * cell + headerHeight);
    }

    void paint(juce::Graphics &graphics) override
    {
        // The module panel's own background, so the rims and the halo are read
        // against what they will actually sit on.
        graphics.fillAll(juce::Colour(0xFF232323));

        label(graphics, "The editor's knobs, painted", margin, margin, 22.5f, juce::Colours::white);

        int y(margin + headerHeight);
        for (auto const &row : rows)
        {
            label(graphics, row.caption, margin, y - 21, 16.5f, juce::Colours::grey);
            for (unsigned int step(0); step < steps; ++step)
            {
                auto const value(static_cast<float>(step) / (steps - 1));
                juce::Rectangle<float> const face(
                    static_cast<float>(margin + int(step) * cell + (cell - int(row.diameter)) / 2),
                    static_cast<float>(y + (cell - int(row.diameter)) / 2),
                    static_cast<float>(row.diameter), static_cast<float>(row.diameter));
                if (row.editor)
                    GUI::paintEditorKnob(graphics, face, value);
                else
                    GUI::paintModuleKnob(graphics, face, value, row.bipolar, row.highlight,
                                         row.lfoRange
                                             ? std::optional<juce::Range<float>>(lfoBandAt(value))
                                             : std::nullopt);
            }
            y += cell;
        }
    }

  private:
    static constexpr unsigned int steps{9}; ///< values across, 0 to 1 inclusive
    static constexpr int cell{102};
    static constexpr int margin{30};
    static constexpr int headerHeight{60};

    static constexpr int rowCount{int(std::size(rows))};

    static void label(juce::Graphics &graphics, juce::String const &text, int const x, int const y,
                      float const height, juce::Colour const colour)
    {
        graphics.setColour(colour);
        graphics.setFont(juce::FontOptions(GUI::regularTypeface()).withHeight(height));
        graphics.drawText(text, x, y, 900, 24, juce::Justification::centredLeft);
    }
}; // class KnobsPage

std::unique_ptr<juce::Component> construct() { return std::make_unique<KnobsPage>(); }

SWShowUI::PageRegistration const registration{
    "knobs", "both painted knobs across their whole travel", &construct};

//------------------------------------------------------------------------------
} // anonymous namespace
//------------------------------------------------------------------------------
