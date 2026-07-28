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

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
//------------------------------------------------------------------------------

namespace
{
namespace GUI = LE::SW::GUI;

/// Undoes the caching between test cases, and -- more to the point -- before
/// Catch2's main() returns, so that no juce::Image outlives JUCE.
struct ResourceGuard
{
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
