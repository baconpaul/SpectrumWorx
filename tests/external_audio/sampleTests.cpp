////////////////////////////////////////////////////////////////////////////////
///
/// sampleTests.cpp
/// ---------------
///
///   The external audio file loader. Two things are worth pinning and neither
/// was before: that the factory samples are actually in the binary and actually
/// decode -- they are MP3, and which decoder answers for that is a per-platform
/// question (§5.0) -- and that a load produces two equal channels at the rate it
/// was asked for, because that is the contract the side channel is fed under.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "external_audio/sample.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
//------------------------------------------------------------------------------

namespace
{
using LE::Sample;

float peak(Sample::ChannelData const &channel)
{
    float loudest{0};
    for (auto const value : channel)
        loudest = std::max(loudest, std::abs(value));
    return loudest;
}
} // anonymous namespace

TEST_CASE("The factory samples are embedded", "[external-audio]")
{
    auto const samples(Sample::factorySamples());

    REQUIRE(!samples.empty());

    for (auto const &sample : samples)
    {
        UNSCOPED_INFO(sample.getFileName());
        // A bare name and no directory: see the note on factorySamples().
        CHECK(sample.getFileName() == sample.getFullPathName());
        CHECK(!sample.getFileNameWithoutExtension().isEmpty());
        CHECK(Sample::isFactorySample(sample));
    }

    CHECK(std::ranges::is_sorted(samples, [](juce::File const &l, juce::File const &r) {
        return l.getFileName() < r.getFileName();
    }));
}

TEST_CASE("A file that is neither on disk nor embedded fails to load", "[external-audio]")
{
    Sample sample;

    CHECK(sample.load(juce::File::createFileWithoutCheckingPath("no such sample.mp3"), 48000) !=
          nullptr);
    CHECK(!sample);
    CHECK(sample.sampleRate() == 0);
}

TEST_CASE("Every factory sample decodes to two equal channels", "[external-audio]")
{
    constexpr unsigned int rate{48000};

    for (auto const &file : Sample::factorySamples())
    {
        UNSCOPED_INFO(file.getFileName());

        Sample sample;
        REQUIRE(sample.load(file, rate) == nullptr);
        REQUIRE(static_cast<bool>(sample));

        CHECK(sample.sampleFile() == file);
        CHECK(sample.sampleRate() == rate);

        // Half a second of audio is the least any of these is; something
        // shorter means a decoder that stopped early rather than failed.
        CHECK(sample.channel1().size() > (rate / 2));
        CHECK(sample.channel1().size() == sample.channel2().size());
        CHECK(sample.channel(0).begin() == sample.channel1().begin());
        CHECK(sample.channel(1).begin() == sample.channel2().begin());

        // Not silence, and not out of range: this is what says the bytes went
        // through a decoder rather than straight into the buffer.
        auto const loudest(peak(sample.channel1()));
        CHECK(loudest > 0.001f);
        CHECK(loudest < 1.5f);
    }
}

TEST_CASE("The requested sample rate decides the length", "[external-audio]")
{
    auto const samples(Sample::factorySamples());
    REQUIRE(!samples.empty());
    auto const &file(samples.front());

    Sample at44k;
    Sample at88k;
    REQUIRE(at44k.load(file, 44100) == nullptr);
    REQUIRE(at88k.load(file, 88200) == nullptr);

    // Twice the rate, twice the frames, to within the frame the ratio rounds
    // off -- the same audio either way.
    auto const ratio(static_cast<double>(at88k.channel1().size()) /
                     static_cast<double>(at44k.channel1().size()));
    CHECK(ratio == Catch::Approx(2.0).epsilon(0.001));
}

TEST_CASE("A sample loaded with no rate keeps the file's own", "[external-audio]")
{
    auto const samples(Sample::factorySamples());
    REQUIRE(!samples.empty());
    auto const &file(samples.front());

    /// \note What a session restored before activate() does; the plugin then
    /// re-reads it once the host says what rate it wants, and sampleRate() being
    /// zero is how it knows to. See SpectrumWorxCLAP::activate().
    Sample sample;
    REQUIRE(sample.load(file, 0) == nullptr);
    CHECK(sample.sampleRate() == 0);
    CHECK(!sample.channel1().empty());
}

TEST_CASE("Clearing a sample forgets the file", "[external-audio]")
{
    auto const samples(Sample::factorySamples());
    REQUIRE(!samples.empty());

    Sample sample;
    REQUIRE(sample.load(samples.front(), 48000) == nullptr);
    REQUIRE(static_cast<bool>(sample));

    sample.clear();

    CHECK(!sample);
    CHECK(sample.sampleFile() == juce::File());
    CHECK(sample.sampleRate() == 0);
}

TEST_CASE("The supported formats wildcard names what can be loaded", "[external-audio]")
{
    auto const wildcards(Sample::supportedFormats());

    UNSCOPED_INFO(wildcards);
    // The factory samples are MP3, so a build whose format manager cannot say
    // so cannot open its own content.
    CHECK(wildcards.containsIgnoreCase("*.mp3"));
    CHECK(wildcards.containsIgnoreCase("*.wav"));
}
