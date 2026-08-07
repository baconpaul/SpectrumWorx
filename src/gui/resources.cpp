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

// juce::Drawable, for the skin files that have been redrawn as vectors: it
// lives in gui_basics rather than in graphics, which resources.hpp includes.
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <cstdio>
//------------------------------------------------------------------------------
CMRC_DECLARE(swAssets);

namespace LE::SW::GUI
{

namespace
{
cmrc::embedded_filesystem skin() { return cmrc::swAssets::get_filesystem(); }

/// \brief Reads one embedded file, or returns an empty span if it is not there.
///
/// \note assets/skin has gaps -- 15, 18 and 54 do not exist, and 19, 25, 26, 29
/// and 36 to 39 exist but nothing references them -- so a miss is a normal
/// answer for a number in range, not an error.
std::pair<char const *, std::size_t> embeddedFile(juce::String const &name)
{
    auto const filesystem(skin());
    // The resource library is rooted at assets/ and holds the factory presets
    // too (stage 8.2), hence the prefix.
    auto const path(("skin/" + name).toStdString());
    if (!filesystem.exists(path))
        return {nullptr, 0};
    auto const file(filesystem.open(path));
    return {file.begin(), static_cast<std::size_t>(file.end() - file.begin())};
}

/// \brief Draws one embedded SVG through a juce::Drawable.
///
/// \note The canvas is taken from the <svg> width/height, not from
/// Drawable::getDrawableBounds(): a DrawableComposite computes that as the
/// union of its children, which is the ink rather than the page. The two
/// differ for every one of these buttons -- 09.svg's pill stops short of its
/// own edges -- and sizing by the ink would crop the file and move every
/// widget that lays itself out from image.getWidth(). JUCE's parser has
/// already set the drawable's content area to the viewBox (juce_SVGParser.cpp,
/// parseSVGElement), so drawing it at the origin lands where the file says.
Artwork loadVector(char const *const data, std::size_t const size)
{
    auto const svg(
        juce::parseXML(juce::String::createStringFromData(data, static_cast<int>(size))));
    if (svg == nullptr)
    {
        LE_ASSERT_MSG(false, "Malformed skin vector.");
        return {};
    }

    auto drawable(juce::Drawable::createFromSVG(*svg));
    if (drawable == nullptr)
    {
        LE_ASSERT_MSG(false, "Unrenderable skin vector.");
        return {};
    }

    auto width(svg->getDoubleAttribute("width"));
    auto height(svg->getDoubleAttribute("height"));
    if ((width <= 0) || (height <= 0)) // width/height are optional; the viewBox is the fallback
    {
        auto const box(juce::StringArray::fromTokens(svg->getStringAttribute("viewBox"), " ,", {}));
        if (box.size() == 4)
        {
            width = box[2].getDoubleValue();
            height = box[3].getDoubleValue();
        }
    }
    LE_ASSERT_MSG((width > 0) && (height > 0), "Skin vector has no size.");
    if ((width <= 0) || (height <= 0))
        return {};

    return Artwork(std::move(drawable), juce::roundToInt(width), juce::roundToInt(height));
}

/// \brief One skin file: the vector if it has been redrawn, else the bitmap.
///
/// \note The conversion is file by file (assets/skin/08.svg and friends), so
/// both forms are embedded and the vector wins where there is one. Reverting a
/// conversion is deleting the .svg.
Artwork loadArtwork(unsigned int const number)
{
    // "01" ... "68": zero padded to two digits, as the files are named.
    auto const stem(juce::String(number).paddedLeft('0', 2));

    if (auto const [vector, vectorSize](embeddedFile(stem + ".svg")); vector)
    {
        if (auto artwork(loadVector(vector, vectorSize)); artwork.isValid())
        {
#if !defined(NDEBUG)
            ////////////////////////////////////////////////////////////////////
            ///
            /// \note A conversion has to keep its bitmap's canvas, because the
            /// widgets lay themselves out from getWidth()/getHeight() and a
            /// vector that comes back a different size moves controls around
            /// the editor -- silently, and only on the screens nobody
            /// screenshotted. Both forms are still embedded while the skin is
            /// half converted, so the check is free to make and this is the one
            /// place that has both. Debug only: it costs a PNG decode.
            ///
            ////////////////////////////////////////////////////////////////////
            if (auto const [bitmap, bitmapSize](embeddedFile(stem + ".png")); bitmap)
            {
                juce::MemoryInputStream original(bitmap, bitmapSize, false);
                auto const wasImage(juce::PNGImageFormat().decodeImage(original));
                LE_ASSERT_MSG((wasImage.getWidth() == artwork.getWidth()) &&
                                  (wasImage.getHeight() == artwork.getHeight()),
                              "Skin vector does not have its bitmap's size.");
            }
#endif // NDEBUG
            return artwork;
        }
        // a broken .svg has already asserted; fall through to the bitmap
    }

    auto const [data, size](embeddedFile(stem + ".png"));
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

    return Artwork(image);
}

std::array<Artwork, numberOfResourceBitmaps + 1> artworkCache;

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

Artwork::Artwork() = default;
Artwork::Artwork(Artwork &&) noexcept = default;
Artwork &Artwork::operator=(Artwork &&) noexcept = default;
Artwork::~Artwork() = default;

Artwork::Artwork(juce::Image image)
    : image_(std::move(image)), width_(image_.getWidth()), height_(image_.getHeight())
{
}

Artwork::Artwork(std::unique_ptr<juce::Drawable> drawable, int const width, int const height)
    : drawable_(std::move(drawable)), width_(width), height_(height)
{
}

bool Artwork::isValid() const { return (drawable_ != nullptr) || image_.isValid(); }
bool Artwork::isVector() const { return drawable_ != nullptr; }

void Artwork::draw(juce::Graphics &graphics, int const x, int const y, float const opacity,
                   juce::Colour const overlay) const
{
    if (!isValid())
        return;

    //   The vector path, and the reason any of this exists: the Drawable is
    // painted into the caller's context, so it is rasterised at whatever scale
    // that context is really rendering at rather than at the 1x the skin was
    // drawn for.
    if (isVector() && overlay.isTransparent())
    {
        drawable_->draw(
            graphics, opacity,
            juce::AffineTransform::translation(static_cast<float>(x), static_cast<float>(y)));
        return;
    }

    //   Tinting means filling the artwork's alpha with a colour, which needs
    // the pixels. Three buttons ask for it, on mouse-over, and none of their
    // files are vectors yet; if one becomes one this rasterises it at 1x,
    // which is what it did before it was a vector at all.
    auto const &pixels(image());
    if (!pixels.isValid())
        return;

    auto const target(juce::Rectangle<int>(x, y, width_, height_).toFloat());
    auto const placement(juce::RectanglePlacement(juce::RectanglePlacement::stretchToFit)
                             .getTransformToFit(pixels.getBounds().toFloat(), target));

    if (!overlay.isOpaque())
    {
        graphics.setOpacity(opacity);
        graphics.drawImageTransformed(pixels, placement, false);
    }
    if (!overlay.isTransparent())
    {
        graphics.setColour(overlay);
        graphics.drawImageTransformed(pixels, placement, true);
    }
}

void Artwork::drawScaled(juce::Graphics &graphics, juce::Rectangle<int> const target,
                         juce::Rectangle<int> const source, float const opacity) const
{
    if (!isValid() || target.isEmpty() || source.isEmpty())
        return;

    if (isVector())
    {
        juce::Graphics::ScopedSaveState const state(graphics);
        graphics.reduceClipRegion(target);
        auto const scaleX(static_cast<float>(target.getWidth()) /
                          static_cast<float>(source.getWidth()));
        auto const scaleY(static_cast<float>(target.getHeight()) /
                          static_cast<float>(source.getHeight()));
        drawable_->draw(
            graphics, opacity,
            juce::AffineTransform::translation(static_cast<float>(-source.getX()),
                                               static_cast<float>(-source.getY()))
                .scaled(scaleX, scaleY)
                .translated(static_cast<float>(target.getX()), static_cast<float>(target.getY())));
        return;
    }

    auto const &pixels(image());
    if (!pixels.isValid())
        return;
    graphics.setOpacity(opacity);
    graphics.drawImage(pixels, target.getX(), target.getY(), target.getWidth(), target.getHeight(),
                       source.getX(), source.getY(), source.getWidth(), source.getHeight());
}

std::unique_ptr<juce::Drawable> Artwork::drawableCopy() const
{
    return (drawable_ != nullptr) ? drawable_->createCopy() : nullptr;
}

juce::Image const &Artwork::image() const
{
    if ((drawable_ != nullptr) && !image_.isValid() && (width_ > 0) && (height_ > 0))
    {
        image_ = juce::Image(juce::Image::ARGB, width_, height_, true);
        juce::Graphics graphics(image_);
        drawable_->draw(graphics, 1.0f);
    }
    return image_;
}

Artwork const &resourceArtwork(unsigned int const number)
{
    LE_ASSERT(number <= numberOfResourceBitmaps);
    static Artwork const invalid;
    if (number > numberOfResourceBitmaps)
        return invalid;

    auto &cached(artworkCache[number]);
    if (!cached.isValid())
        cached = loadArtwork(number);
    return cached;
}

juce::Image const &resourceBitmap(unsigned int const number)
{
    return resourceArtwork(number).image();
}

bool hasResourceBitmap(unsigned int const number)
{
    return (number <= numberOfResourceBitmaps) && resourceArtwork(number).isValid();
}

bool resourceIsVector(unsigned int const number)
{
    return (number <= numberOfResourceBitmaps) && resourceArtwork(number).isVector();
}

juce::Typeface::Ptr regularTypeface() { return loadTypeface("Vera.ttf", regularTypefaceCache); }
juce::Typeface::Ptr boldTypeface() { return loadTypeface("VeraBd.ttf", boldTypefaceCache); }

void releaseCachedResources()
{
    for (auto &artwork : artworkCache)
        artwork = Artwork();
    regularTypefaceCache = nullptr;
    boldTypefaceCache = nullptr;
}

} // namespace LE::SW::GUI
