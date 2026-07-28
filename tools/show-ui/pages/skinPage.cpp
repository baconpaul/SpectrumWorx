////////////////////////////////////////////////////////////////////////////////
///
/// \file skinPage.cpp
/// ------------------
///
///   Every embedded skin bitmap, at 1:1, with its number and size, plus the two
/// embedded fonts. Proof that the resource library resolves and decodes, and a
/// contact sheet for whoever has to work out which PNG is which widget.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "../page.hpp"

#include "gui/resources.hpp"

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>
//------------------------------------------------------------------------------
namespace
{
//------------------------------------------------------------------------------

namespace GUI = LE::SW::GUI;

struct Entry
{
    unsigned int number;
    juce::Image image;
};

/// The skin's own background (01) is 900-odd pixels wide and would dominate the
/// sheet; everything is drawn at 1:1 regardless, so the sheet is simply wide.
class SkinPage final : public juce::Component
{
  public:
    SkinPage()
    {
        for (unsigned int number(1); number <= GUI::numberOfResourceBitmaps; ++number)
        {
            auto const &image(GUI::resourceBitmap(number));
            if (image.isValid())
                entries_.push_back({number, image});
        }
        layOut();
    }

    void paint(juce::Graphics &graphics) override
    {
        graphics.fillAll(juce::Colour(0xFF141414));

        graphics.setColour(juce::Colours::white);
        graphics.setFont(juce::FontOptions(18.0f));
        graphics.drawText(juce::String(entries_.size()) + " embedded skin bitmaps", margin, margin,
                          getWidth() - 2 * margin, 24, juce::Justification::topLeft);

        drawFontSamples(graphics);

        for (auto const &placement : placements_)
        {
            auto const &entry(entries_[placement.entry]);

            // A checkerboard behind each, so that transparency reads as
            // transparency rather than as black.
            drawCheckerboard(graphics, placement.bounds);
            graphics.drawImage(entry.image, placement.bounds.toFloat(),
                               juce::RectanglePlacement::centred |
                                   juce::RectanglePlacement::onlyReduceInSize);

            juce::String caption(juce::String(entry.number).paddedLeft('0', 2) + "   " +
                                 juce::String(entry.image.getWidth()) + "x" +
                                 juce::String(entry.image.getHeight()));
            if (placement.reduced)
                caption += " (fit)";

            graphics.setColour(placement.reduced ? juce::Colours::orange : juce::Colours::grey);
            graphics.setFont(juce::FontOptions(10.0f));
            graphics.drawText(caption, placement.bounds.getX(), placement.bounds.getBottom() + 2,
                              placement.bounds.getWidth(), 12, juce::Justification::topLeft);
        }
    }

  private:
    static constexpr int margin{16};
    static constexpr int gap{14};
    static constexpr int captionHeight{16};
    static constexpr int headerHeight{96};
    static constexpr int sheetWidth{1180};

    struct Placement
    {
        std::size_t entry;
        juce::Rectangle<int> bounds;
        bool reduced; ///< drawn smaller than 1:1, and captioned as such
    };

    /// \note The knob bitmaps are vertical film strips of 127 frames -- 02 is
    /// 55 x 6985 -- so a sheet at true size is 17 000 pixels tall and useless.
    /// Anything past this is fitted into the cell and marked.
    static constexpr int maxCell{190};

    void layOut()
    {
        int x(margin), y(margin + headerHeight), rowHeight(0);
        for (std::size_t index(0); index < entries_.size(); ++index)
        {
            auto const &image(entries_[index].image);
            auto const oversized(image.getWidth() > maxCell || image.getHeight() > maxCell);
            auto const scale(oversized ? std::min(static_cast<double>(maxCell) / image.getWidth(),
                                                  static_cast<double>(maxCell) / image.getHeight())
                                       : 1.0);
            auto const width(std::max(1, juce::roundToInt(image.getWidth() * scale)));
            auto const height(std::max(1, juce::roundToInt(image.getHeight() * scale)));

            if ((x > margin) && (x + width > sheetWidth - margin))
            {
                x = margin;
                y += rowHeight + captionHeight + gap;
                rowHeight = 0;
            }
            placements_.push_back({index, {x, y, width, height}, oversized});
            x += width + gap;
            rowHeight = std::max(rowHeight, height);
        }
        setSize(sheetWidth, y + rowHeight + captionHeight + margin);
    }

    void drawFontSamples(juce::Graphics &graphics) const
    {
        auto const sample(
            [&](juce::Typeface::Ptr const typeface, juce::String const &label, int const y) {
                graphics.setColour(juce::Colours::grey);
                graphics.setFont(juce::FontOptions(11.0f));
                graphics.drawText(label + (typeface != nullptr ? "" : "   NOT LOADED"), margin, y,
                                  90, 20, juce::Justification::centredLeft);
                if (typeface == nullptr)
                    return;
                graphics.setColour(juce::Colours::white);
                graphics.setFont(juce::FontOptions(typeface).withHeight(15.0f));
                graphics.drawText("SpectrumWorx 0123456789 -- the quick brown fox", margin + 96, y,
                                  getWidth() - margin - 96, 20, juce::Justification::centredLeft);
            });

        sample(GUI::regularTypeface(), "Vera", margin + 32);
        sample(GUI::boldTypeface(), "Vera bold", margin + 56);
    }

    static void drawCheckerboard(juce::Graphics &graphics, juce::Rectangle<int> const &bounds)
    {
        constexpr int square{6};
        for (int y(0); y < bounds.getHeight(); y += square)
            for (int x(0); x < bounds.getWidth(); x += square)
            {
                graphics.setColour(((x / square + y / square) % 2) ? juce::Colour(0xFF2A2A2A)
                                                                   : juce::Colour(0xFF1F1F1F));
                graphics.fillRect(bounds.getX() + x, bounds.getY() + y,
                                  std::min(square, bounds.getWidth() - x),
                                  std::min(square, bounds.getHeight() - y));
            }
    }

    std::vector<Entry> entries_;
    std::vector<Placement> placements_;
}; // class SkinPage

std::unique_ptr<juce::Component> construct() { return std::make_unique<SkinPage>(); }

SWShowUI::PageRegistration const registration{"skin", "every embedded bitmap and both fonts",
                                              &construct};

//------------------------------------------------------------------------------
} // anonymous namespace
//------------------------------------------------------------------------------
