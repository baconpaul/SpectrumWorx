////////////////////////////////////////////////////////////////////////////////
///
/// skinTests.cpp
/// -------------
///
///   The skin is embedded rather than installed as of stage 6.3, so "is the
/// artwork there" became a link-time property instead of a deployment one. This
/// is what checks it.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "gui/resources.hpp"

#include <juce_gui_basics/juce_gui_basics.h>

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <string>
#include <vector>
//------------------------------------------------------------------------------

namespace
{
namespace GUI = LE::SW::GUI;

/// Undoes the caching between test cases, and -- more to the point -- before
/// Catch2's main() returns, so that no juce::Image outlives JUCE.
///
/// \note The initialiser stands in for the one the CLAP shim holds in the
/// plugin. It is needed here as of the skin's first vector files: those are
/// rendered through a juce::Drawable, which is a juce::Component, and building
/// one asserts if the message manager is not up. Decoding a PNG never touched
/// it. Declared as a member rather than taken in each case so the ordering is
/// right -- the destructor body releases the images, and only then does the
/// initialiser go, so nothing outlives the JUCE it was made under.
struct ResourceGuard
{
    juce::ScopedJuceInitialiser_GUI initialiser;
    ~ResourceGuard() { GUI::releaseCachedResources(); }
};
} // anonymous namespace

TEST_CASE("Every named skin bitmap is embedded and decodes", "[gui][skin]")
{
    ResourceGuard const guard;

    std::vector<std::string> missing;
#define LE_SW_AUX_CHECK_BITMAP(name, number)                                                       \
    if (!GUI::hasResourceBitmap(number))                                                           \
        missing.push_back(#name " (" #number ".png)");
    LE_SW_RESOURCE_BITMAP_LIST(LE_SW_AUX_CHECK_BITMAP)
#undef LE_SW_AUX_CHECK_BITMAP

    for (auto const &name : missing)
        UNSCOPED_INFO(name);
    CHECK(missing.empty());
}

TEST_CASE("Skin bitmaps have plausible dimensions", "[gui][skin]")
{
    ResourceGuard const guard;

    // Nothing in the skin is 1x1 or larger than the editor could ever be; a
    // truncated resource decodes to something, so a size check is what catches
    // a half-embedded file.
    std::vector<std::string> odd;
#define LE_SW_AUX_CHECK_SIZE(name, number)                                                         \
    {                                                                                              \
        auto const &image(GUI::resourceBitmap(number));                                            \
        if (image.getWidth() < 2 || image.getHeight() < 2 || image.getWidth() > 4096 ||            \
            image.getHeight() > 16384)                                                             \
            odd.push_back(#name);                                                                  \
    }
    LE_SW_RESOURCE_BITMAP_LIST(LE_SW_AUX_CHECK_SIZE)
#undef LE_SW_AUX_CHECK_SIZE

    for (auto const &name : odd)
        UNSCOPED_INFO(name);
    CHECK(odd.empty());
}

TEST_CASE("Both skin typefaces load from the embedded bytes", "[gui][skin]")
{
    ResourceGuard const guard;

    // The 2016 build registered these with the OS from a file on disk; if this
    // fails, JUCE 8's in-memory typeface path is not doing what it claims and
    // the editor would silently fall back to a system font.
    REQUIRE(GUI::regularTypeface() != nullptr);
    REQUIRE(GUI::boldTypeface() != nullptr);
    CHECK(GUI::regularTypeface()->getName().isNotEmpty());
    CHECK(GUI::boldTypeface()->getName().isNotEmpty());
}

TEST_CASE("Releasing the cache is idempotent and reloads", "[gui][skin]")
{
    ResourceGuard const guard;

    REQUIRE(GUI::resourceBitmap(GUI::EditorBackground).isValid());
    GUI::releaseCachedResources();
    GUI::releaseCachedResources();
    CHECK(GUI::resourceBitmap(GUI::EditorBackground).isValid());
}

TEST_CASE("Skin vectors render at the size their bitmap had", "[gui][skin]")
{
    ResourceGuard const guard;

    //   These are the files that have been redrawn as SVG; resources.cpp
    // prefers the vector, so what these numbers now return is a rasterised
    // juce::Drawable rather than a decoded PNG. Every widget that draws them
    // is still laid out from getWidth()/getHeight(), so a vector that comes
    // back a different size from the bitmap it replaced moves controls around
    // the editor.
    //
    //   That is not a hypothetical: JUCE gives a DrawableComposite bounds that
    // are the union of its children -- the ink -- and the pill in 09.svg stops
    // ~4px short of the canvas on every side. Sizing the render off
    // getDrawableBounds() rather than off the <svg> width/height yields 50x17
    // where the skin says 57x24, and the preset button lands in the wrong
    // place.
    //
    //   The size *equality* is asserted by resources.cpp, which is the one
    // place holding both forms while the skin is half converted. This walks
    // the numbering rather than listing files, so it covers whatever has been
    // converted since without anyone remembering to add it here.
    unsigned int vectors(0);
    for (unsigned int number(1); number <= GUI::numberOfResourceBitmaps; ++number)
    {
        if (!GUI::resourceIsVector(number))
            continue;

        ++vectors;
        auto const &image(GUI::resourceBitmap(number));
        UNSCOPED_INFO("skin file " << number);
        CHECK(image.isValid());
        CHECK(image.getWidth() >= 2);
        CHECK(image.getHeight() >= 2);
    }

    UNSCOPED_INFO("vector files found: " << vectors);
    CHECK(vectors >= 10);
}

TEST_CASE("A skin vector draws its artwork, not an empty canvas", "[gui][skin]")
{
    ResourceGuard const guard;

    //   The size check above passes just as well for a blank image of the
    // right dimensions, which is exactly what a Drawable that failed to parse
    // would leave behind. These probe the pixels instead.
    auto const &plain(GUI::resourceBitmap(GUI::PresetOn)); // 09: no glow
    auto const &lit(GUI::resourceBitmap(GUI::PresetOff));  // 08: blue rim

    // The pill is a top-lit gradient, so the middle of the button is opaque
    // and the very corner of the canvas is outside it.
    CHECK(plain.getPixelAt(28, 12).getAlpha() == 255);
    CHECK(plain.getPixelAt(0, 0).getAlpha() == 0);
    CHECK(plain.getPixelAt(28, 6).getBrightness() > plain.getPixelAt(28, 18).getBrightness());

    //   And the lit variant differs from the plain one only in the rim and the
    // glow: same body, blue down the edge. If the two came back equal, the
    // loader would be handing every widget the same file.
    auto const rim(lit.getPixelAt(28, 4));
    CHECK(rim.getBlue() > rim.getRed());
    CHECK(lit.getPixelAt(28, 12) == plain.getPixelAt(28, 12));
}

TEST_CASE("The converted files are served as vectors", "[gui][skin]")
{
    ResourceGuard const guard;

    // Both forms are embedded and they are the same size, so nothing else here
    // can tell which one a number resolved to -- if the CMake glob stopped
    // picking up assets/skin/*.svg, every other case in this file would still
    // pass against the artwork the vectors were supposed to replace.
    CHECK(GUI::resourceIsVector(GUI::PresetOff));
    CHECK(GUI::resourceIsVector(GUI::PresetOn));
    CHECK(GUI::resourceIsVector(GUI::SettingsOff));
    CHECK(GUI::resourceIsVector(GUI::SettingsOn));
    CHECK(GUI::resourceIsVector(GUI::PresetSaveUp));
    CHECK(GUI::resourceIsVector(GUI::PresetDeleteDown));

    //   And the negative control, which is a hole in the numbering rather than
    // an unconverted file. It used to be the editor background and the knob
    // strip; both have since been redrawn, which broke this case and is the
    // reason it is worded this way now. A number with no file can never become
    // a vector, so this keeps saying what it means as the conversion finishes.
    CHECK_FALSE(GUI::resourceIsVector(15));
    CHECK_FALSE(GUI::resourceIsVector(GUI::numberOfResourceBitmaps + 1));
}

TEST_CASE("A vector is drawn at the context's resolution, not the skin's", "[gui][skin]")
{
    ResourceGuard const guard;

    //   This is the whole point of holding vectors: the same Artwork asked to
    // paint into a 2x context resolves the glyph edges at 2x, where the bitmap
    // it replaced could only be scaled up. If this ever stops holding, the
    // conversion has quietly become a slower way to draw the old PNGs.
    auto const &artwork(GUI::resourceArtwork(GUI::PresetOn));
    REQUIRE(artwork.isVector());

    auto const w(artwork.getWidth());
    auto const h(artwork.getHeight());

    juce::Image atTwice(juce::Image::ARGB, w * 2, h * 2, true);
    {
        juce::Graphics graphics(atTwice);
        graphics.addTransform(juce::AffineTransform::scale(2.0f));
        artwork.draw(graphics, 0, 0);
    }

    // The same artwork rasterised at 1x and blown up -- what a juce::Image
    // could have offered.
    auto const upscaled(
        artwork.image().rescaled(w * 2, h * 2, juce::Graphics::highResamplingQuality));

    int different(0);
    for (int y(0); y < h * 2; ++y)
        for (int x(0); x < w * 2; ++x)
            if (std::abs(atTwice.getPixelAt(x, y).getBrightness() -
                         upscaled.getPixelAt(x, y).getBrightness()) > 0.09f)
                ++different;

    UNSCOPED_INFO("pixels where the vector resolves detail the upscale cannot: " << different);
    CHECK(different > 100);
}

TEST_CASE("Holes in the numbering answer, rather than assert", "[gui][skin]")
{
    ResourceGuard const guard;

    // 15, 18 and 54 have no file. That is a property of the artwork, not a
    // mistake, so asking is legal and the answer is "no" -- which is what lets
    // a tool iterate 1..68 without knowing the gaps.
    //
    // A number past the end is a different thing: nothing names it, so a caller
    // that passes one has a bug, and resourceBitmap() asserts on it. Only
    // hasResourceBitmap() is total.
    CHECK_FALSE(GUI::hasResourceBitmap(15));
    CHECK_FALSE(GUI::hasResourceBitmap(18));
    CHECK_FALSE(GUI::hasResourceBitmap(54));
    CHECK_FALSE(GUI::hasResourceBitmap(GUI::numberOfResourceBitmaps + 1));
}
