////////////////////////////////////////////////////////////////////////////////
///
/// aboutPageTests.cpp
/// ------------------
///
///   The settings panel's About tab, which stopped being a baked bitmap on
/// 15.08.2026 and became text.
///
/// \note That "the panel is not blank" is *not* here: overlayPanelTests.cpp
/// already opens this tab through the logo and measures how much of the overlay
/// rectangle it covers, which is the same question asked better. What is here is
/// the two things that became possible only once the page was text, and that a
/// render cannot see -- a version block that is a template rather than a version,
/// and credits the compiler quietly re-encoded.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "gui/about.hpp"

#include "configuration/buildStamp.hpp"

#include <sst/plugininfra/version_information.h>

#include <juce_gui_basics/juce_gui_basics.h>

#include <catch2/catch_test_macros.hpp>
//------------------------------------------------------------------------------
namespace
{
using AboutPage = LE::SW::GUI::AboutPage;
} // anonymous namespace

TEST_CASE("The About page's information block is a version, not a template", "[gui][about]")
{
    auto const information(AboutPage::information());

    /// \note The shape, as buildStampTests.cpp checks the stamp's: what goes
    /// wrong with generated version strings in this tree is a substitution that
    /// did not happen, and an unsubstituted `@CMAKE_PROJECT_VERSION_MAJOR@` is
    /// still a perfectly legible line on a panel nobody reads closely.
    CHECK(information.isNotEmpty());
    CHECK_FALSE(information.contains("@"));

    CHECK(information.contains("SpectrumWorx"));
    CHECK(information.contains(sst::plugininfra::VersionInformation::project_version));
    CHECK(information.contains(sst::plugininfra::VersionInformation::git_commit_hash));

    // Both halves of it: the release the tree was configured as, and the build.
    CHECK(information.contains(LE::SW::BuildStamp::date));
    CHECK(information.contains(LE::SW::BuildStamp::commit));

    /// The tag placeholder is sst-plugininfra's own and means "untagged"; the
    /// page has to translate it rather than show it.
    CHECK_FALSE(information.contains("-no-tag-"));
}

////////////////////////////////////////////////////////////////////////////////
///
/// \note This is the only test in the tree that asserts a code point, and the
/// reason is that four of the six original authors have one in their name.
///
///   Two things between the source file and the screen can eat them, and neither
/// fails loudly. A compiler that does not read the source as UTF-8 re-encodes the
/// literals in the machine's codepage -- MSVC's default, which is why the
/// top-level CMakeLists passes `/utf-8` -- and `juce::String(char const *)` is
/// the *ASCII* constructor, which asserts in a debug build and takes the bytes as
/// Latin-1 in a release one. Either way the page still renders, still lays out,
/// and still passes every pixel-coverage check; it just spells the names wrong.
///
/// \note Asserted against the code points rather than against a literal copy of
/// the same string, which would be re-encoded by the same compiler in the same
/// way and agree with it.
///
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("The original authors survive the compiler as UTF-8", "[gui][about]")
{
    auto const authors(AboutPage::originalAuthors());
    REQUIRE(authors.size() == 6);

    for (auto const &author : authors)
    {
        UNSCOPED_INFO(author.toRawUTF8());
        CHECK(author.isNotEmpty());
    }

    // Domagoj Šarić: U+0160 LATIN CAPITAL LETTER S WITH CARON, and twice
    // U+0107 LATIN SMALL LETTER C WITH ACUTE.
    CHECK(authors[1].startsWith("Domagoj"));
    CHECK(authors[1].containsChar(juce::juce_wchar(0x0160)));
    CHECK(authors[1].containsChar(juce::juce_wchar(0x0107)));

    // Ivan Dokmanić, Matija Bošnjaković, and Danijel Domazet's ćevapi.
    CHECK(authors[2].containsChar(juce::juce_wchar(0x0107)));
    CHECK(authors[3].containsChar(juce::juce_wchar(0x0161))); // š
    CHECK(authors[3].containsChar(juce::juce_wchar(0x0107)));
    CHECK(authors[5].containsChar(juce::juce_wchar(0x0107)));

    /// \note What a codepage round trip actually produces, and the check that
    /// would have caught it: CP1252 reading the UTF-8 bytes of "Š" gives "Å " --
    /// so the failure mode is a stray U+00C5, not a missing U+0160.
    for (auto const &author : authors)
        CHECK_FALSE(author.containsChar(juce::juce_wchar(0x00C5)));
}
