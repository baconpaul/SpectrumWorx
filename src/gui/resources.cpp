////////////////////////////////////////////////////////////////////////////////
///
/// \file resources.cpp
/// -------------------
///
/// Copyright (c) 2010 - 2016. Little Endian Ltd.
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "resources.hpp"

#include "le/utility/assert.hpp"

#include <cmrc/cmrc.hpp>

#include <array>
#include <cstdio>
//------------------------------------------------------------------------------
CMRC_DECLARE(swSkin);
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace SW
{
//------------------------------------------------------------------------------
namespace GUI
{
//------------------------------------------------------------------------------

namespace
{
cmrc::embedded_filesystem skin() { return cmrc::swSkin::get_filesystem(); }

/// \brief Reads one embedded file, or returns an empty span if it is not there.
///
/// \note assets/skin has gaps -- 15, 18 and 54 do not exist, and 19, 25, 26, 29
/// and 36 to 39 exist but nothing references them -- so a miss is a normal
/// answer for a number in range, not an error.
std::pair<char const *, std::size_t> embeddedFile(juce::String const &name)
{
    auto const filesystem(skin());
    // The resource library is rooted at assets/, so that presets and samples
    // can join it in stage 8 without a second one.
    auto const path(("skin/" + name).toStdString());
    if (!filesystem.exists(path))
        return {nullptr, 0};
    auto const file(filesystem.open(path));
    return {file.begin(), static_cast<std::size_t>(file.end() - file.begin())};
}

juce::Image decodeBitmap(unsigned int const number)
{
    // "01.png" ... "68.png": zero padded to two digits, as the files are named.
    auto const name(juce::String(number).paddedLeft('0', 2) + ".png");
    auto const [data, size](embeddedFile(name));
    if (!data)
        return {}; // a gap in the numbering, not an error -- see the header

    juce::MemoryInputStream stream(data, size, false);
    juce::Image image(juce::PNGImageFormat().decodeImage(stream));
    LE_ASSERT_MSG(image.isValid() && image.getWidth() && image.getHeight(), "Corrupt skin bitmap.");

    /// \note Deliberately no gamma correction here.
    ///
    ///   The 2016 loader ran an in-place pow( x, 2.2 / 1.8 ) over every byte of
    /// every bitmap on macOS, to convert artwork authored for a PC's 2.2 gamma
    /// to the Mac's 1.8. Apple moved macOS to 2.2 in Snow Leopard, in 2009, so
    /// the correction was already wrong when this code shipped: it darkens the
    /// skin on every Mac made since, and makes the Mac build disagree with the
    /// Windows one, which never did it.
    ///
    ///   It also walked the buffer a byte at a time with no regard for pixel
    /// stride, so it gamma-corrected the *alpha* channel along with the colour
    /// -- there is no reading of that which is correct.
    ///
    ///   Removing it changes how the plugin looks on macOS. It changes it to
    /// what the artwork says and to what Windows has always drawn.
    ///                                       (28.07.2026.) (SW port)

    return image;
}

std::array<juce::Image, numberOfResourceBitmaps + 1> bitmapCache;

juce::Typeface::Ptr loadTypeface(juce::String const &name, juce::Typeface::Ptr &cache)
{
    if (cache == nullptr)
    {
        auto const [data, size](embeddedFile(name));
        if (data)
            cache = juce::Typeface::createSystemTypefaceFor(data, size);
        LE_ASSERT_MSG(cache != nullptr, "Cannot load the skin typeface.");
    }
    return cache;
}

juce::Typeface::Ptr regularTypefaceCache;
juce::Typeface::Ptr boldTypefaceCache;
} // anonymous namespace

juce::Image const &resourceBitmap(unsigned int const number)
{
    LE_ASSERT(number <= numberOfResourceBitmaps);
    static juce::Image const invalid;
    if (number > numberOfResourceBitmaps)
        return invalid;

    auto &cached(bitmapCache[number]);
    if (!cached.isValid())
        cached = decodeBitmap(number);
    return cached;
}

bool hasResourceBitmap(unsigned int const number)
{
    return (number <= numberOfResourceBitmaps) && resourceBitmap(number).isValid();
}

juce::Typeface::Ptr regularTypeface() { return loadTypeface("Vera.ttf", regularTypefaceCache); }
juce::Typeface::Ptr boldTypeface() { return loadTypeface("VeraBd.ttf", boldTypefaceCache); }

void releaseCachedResources()
{
    for (auto &image : bitmapCache)
        image = juce::Image();
    regularTypefaceCache = nullptr;
    boldTypefaceCache = nullptr;
}

//------------------------------------------------------------------------------
} // namespace GUI
//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
