////////////////////////////////////////////////////////////////////////////////
///
/// skinTests.cpp
/// -------------
///
///   The skin is embedded rather than installed as of stage 6.3, so "is the
/// artwork there" became a link-time property instead of a deployment one. This
/// is what checks it.
///
/// \note About the *serving* of the artwork and never about the artwork itself.
/// The skin is redrawn as ordinary work, so nothing here may name a pixel, a
/// colour, a size or which files have been converted to SVG -- a case that did
/// would fail on a redraw for a reason that has nothing to do with this tree.
/// What is left is the contract a widget depends on: the file is embedded, it
/// decodes, and what comes back is a valid image of plausible size.
///                                           (14.08.2026.) (SW port)
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "gui/resources.hpp"

#include <juce_gui_basics/juce_gui_basics.h>

#include <catch2/catch_test_macros.hpp>

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

    // A number past the end is nobody's: resourceBitmap() asserts on one, so
    // hasResourceBitmap() is the only total accessor and has to say no.
    CHECK_FALSE(GUI::hasResourceBitmap(GUI::numberOfResourceBitmaps + 1));
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
    // converted since without anyone remembering to add it here -- and it
    // reaches the numbers LE_SW_RESOURCE_BITMAP_LIST does not name (19, 25,
    // 26, 29, 36..39), which is what the case above cannot see.
    //
    //   A malformed SVG is what this is for: a Drawable that fails to parse
    // hands back an invalid or zero-sized image, and every widget laid out
    // from getWidth()/getHeight() then lands in the wrong place.
    //
    // \note No floor on how many vectors there are. Which files have been
    // redrawn is a property of the artwork rather than of the code, and a
    // count here would make replacing an SVG with a PNG -- or the reverse --
    // a test failure rather than a decision.
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
}

////////////////////////////////////////////////////////////////////////////////
///
/// \note Four cases stood below this line until 14.08.2026 and all four were
/// about *which artwork* the skin holds rather than about the code that serves
/// it: pixel probes into 08.svg and 09.svg (the pill is opaque in the middle,
/// the lit variant's rim is bluer than it is red), an inventory of the six
/// files redrawn as SVG, a 2x rasterisation of PresetOn measured against an
/// upscale of itself, and 15/18/54 pinned as holes in the numbering.
///
///   Every one of them fails when the skin is redrawn, which is now ordinary
/// work rather than a rare event -- and none of them fails for a reason about
/// this tree. What they were guarding is guarded above: a file that is not
/// embedded is caught by hasResourceBitmap(), a truncated or unparseable one by
/// the dimension and validity checks, and a number nobody names by the
/// past-the-end check.
///
////////////////////////////////////////////////////////////////////////////////
