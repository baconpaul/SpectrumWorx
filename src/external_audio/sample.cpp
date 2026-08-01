////////////////////////////////////////////////////////////////////////////////
///
/// sample.cpp
/// ----------
///
/// Copyright (c) 2010 - 2016. Little Endian Ltd.
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "sample.hpp"

#include "le/math/vector.hpp" // Math::min/max, for the sanity checks in load()
#include "le/utility/ignoreUnused.hpp"

#include <juce_audio_formats/juce_audio_formats.h>

#include <cmrc/cmrc.hpp>

#include <algorithm>
#include <cmath>
//------------------------------------------------------------------------------
CMRC_DECLARE(swAssets);
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace
{
//------------------------------------------------------------------------------

/// \note The resource library is rooted at assets/ and holds the skin and the
/// presets as well, hence the prefix. See assets/CMakeLists.txt for why there is
/// only one.
constexpr std::string_view factoryRoot{"samples"};

cmrc::embedded_filesystem assets() { return cmrc::swAssets::get_filesystem(); }

std::string factoryPath(juce::File const &file)
{
    return std::string(factoryRoot) + '/' + file.getFileName().toStdString();
}

bool isFactory(cmrc::embedded_filesystem const &filesystem, std::string const &path)
{
    return filesystem.exists(path) && filesystem.is_file(path);
}

/// Disk first, then the factory samples in the binary -- so that a preset naming
/// a sample the user has since moved still finds the factory one, which is what
/// the 2016 build's <install>/Samples fallback was for.
std::unique_ptr<juce::InputStream> openSample(juce::File const &file)
{
    if (file.existsAsFile())
        if (auto stream(file.createInputStream()); stream && stream->openedOk())
            return stream;

    auto const filesystem(assets());
    auto const path(factoryPath(file));
    if (!isFactory(filesystem, path))
        return {};

    auto const resource(filesystem.open(path));
    /// \note Non-owning: the bytes are a span of the binary's read-only data and
    /// outlive everything.
    return std::make_unique<juce::MemoryInputStream>(
        resource.begin(), static_cast<std::size_t>(resource.end() - resource.begin()), false);
}

struct BasicFormats : juce::AudioFormatManager
{
    BasicFormats() { registerBasicFormats(); }
};

/// \note One, function local: registerBasicFormats() builds a dozen AudioFormat
/// objects and both callers -- a load, and the file chooser asking what it may
/// show -- want the same answer. Both are on the message thread.
juce::AudioFormatManager &formats()
{
    static BasicFormats manager;
    return manager;
}

//------------------------------------------------------------------------------
} // anonymous namespace
//------------------------------------------------------------------------------

std::vector<juce::File> Sample::factorySamples()
{
    std::vector<juce::File> found;

    auto const filesystem(assets());
    if (!filesystem.is_directory(std::string(factoryRoot)))
        return found;

    for (auto const &entry : filesystem.iterate_directory(std::string(factoryRoot)))
        if (entry.is_file())
            found.push_back(
                juce::File::createFileWithoutCheckingPath(juce::String(entry.filename())));

    /// \note By name, explicitly: juce::File has an operator< but not the rest
    /// of the six, so it does not model std::totally_ordered and the default
    /// comparator does not apply to it.
    std::ranges::sort(found, [](juce::File const &left, juce::File const &right) {
        return left.getFileName() < right.getFileName();
    });
    return found;
}

bool Sample::isFactorySample(juce::File const &file)
{
    return !file.existsAsFile() && isFactory(assets(), factoryPath(file));
}

juce::String Sample::supportedFormats() { return formats().getWildcardForAllFormats(); }

char const *Sample::load(juce::File const &sampleFile, unsigned int const desiredSampleRate,
                         Utility::CriticalSection &criticalSection)
{
    DataHolder newData;
    char const *const pErrorString(doLoad(sampleFile, desiredSampleRate, newData));
    if (!pErrorString)
    {
        Utility::CriticalSectionLock const lock(criticalSection);
        this->sampleFile_ = sampleFile;
        this->sampleRate_ = desiredSampleRate;
        this->data_.takeDataFrom(newData);
        // Implementation note:
        //   The sample position has to be cleared while we still hold the lock
        // otherwise race conditions (and thus crashes) occur.
        //                                    (18.06.2010.) (Domagoj Saric)
        /// \todo Think of a cleaner solution where the Sample class knows
        /// nothing about critical sections and threading issues. This is the
        /// same lock the audio thread try_locks around reading the data, and
        /// both belong to the threading redesign rather than here.
        ///                                   (22.09.2010.) (Domagoj Saric)
        ///                                   (01.08.2026.) (SW port)
        samplePosition_ = 0;

        // Assert something was actually read.
        LE_ASSERT(!channel1().empty());

        // Assert all channels are of equal size.
        LE_ASSERT(channel1().size() == channel2().size());

        /// \note One tolerance rather than 1.0 off Apple and 1.15 on it. Both a
        /// lossy decoder and the interpolator below overshoot a full-scale
        /// signal, on every platform -- the 2010 comment blamed the Mac MP3
        /// path for what is a property of MP3. This is a "we decoded audio and
        /// not garbage" check, and it is only that.
        ///                                   (01.08.2026.) (SW port)
        float const maximumAbsoluteValue(1.5f);

        LE_ASSERT(Math::max(channel1()) <= +maximumAbsoluteValue);
        LE_ASSERT(Math::max(channel2()) <= +maximumAbsoluteValue);
        LE_ASSERT(Math::min(channel1()) >= -maximumAbsoluteValue);
        LE_ASSERT(Math::min(channel2()) >= -maximumAbsoluteValue);

        LE::Utility::ignoreUnused(maximumAbsoluteValue);
    }
    return pErrorString;
}

////////////////////////////////////////////////////////////////////////////////
//
// Sample::doLoad()
// ----------------
//
////////////////////////////////////////////////////////////////////////////////
///
/// \note Always two channels at the engine's sample rate, because that is what
/// the side channel is: a mono file is duplicated, a stereo one taken as it is,
/// anything wider has its first two channels used. Resampling is
/// juce::LagrangeInterpolator, which is what the 2016 platform decoders asked
/// their converters for and is well beyond what a side chain source needs.
///
////////////////////////////////////////////////////////////////////////////////

char const *Sample::doLoad(juce::File const &sampleFile, unsigned int const desiredSampleRate,
                           DataHolder &data)
{
    auto stream(openSample(sampleFile));
    if (!stream)
        return "Unable to open file.";

    std::unique_ptr<juce::AudioFormatReader> const reader(
        formats().createReaderFor(std::move(stream)));
    if (!reader)
        return "Unrecognised or unsupported audio file format.";

    auto const sourceFrames(reader->lengthInSamples);
    if ((reader->numChannels == 0) || (sourceFrames <= 0) || (reader->sampleRate <= 0))
        return "The file holds no audio.";
    /// \note The whole file is decoded into memory and looped from there, as it
    /// always was. A guard rather than a design: 32 bit frames x 2 channels, so
    /// this is 800 MB and nothing a side chain wants.
    if (sourceFrames > 100'000'000)
        return "The file is too long.";

    juce::AudioBuffer<float> source(static_cast<int>(reader->numChannels),
                                    static_cast<int>(sourceFrames));
    if (!reader->read(source.getArrayOfWritePointers(), source.getNumChannels(), 0,
                      source.getNumSamples()))
        return "Failed reading data.";

    /// \note The ratio the interpolator wants: how many input samples make one
    /// output sample. A file already at the engine's rate -- and one loaded
    /// before there is an engine rate at all -- takes the copy path.
    auto const ratio(desiredSampleRate ? reader->sampleRate / static_cast<double>(desiredSampleRate)
                                       : 1.0);
    auto const targetFrames(
        static_cast<std::size_t>(std::floor(static_cast<double>(sourceFrames) / ratio)));
    if (targetFrames == 0)
        return "The file is too short for this sample rate.";

    if (!data.recreate(targetFrames))
        return "Out of memory.";

    float *const channels[fixedNumberOfChannels]{data.pBuffer.get(), data.pChannel2Beginning};
    for (unsigned int channel(0); channel < fixedNumberOfChannels; ++channel)
    {
        auto const sourceChannel(
            std::min<int>(static_cast<int>(channel), source.getNumChannels() - 1));
        float const *const pSource(source.getReadPointer(sourceChannel));

        if (ratio == 1.0)
            std::copy_n(pSource, targetFrames, channels[channel]);
        else
        {
            juce::LagrangeInterpolator interpolator;
            interpolator.process(ratio, pSource, channels[channel], static_cast<int>(targetFrames));
        }
    }

    data.pChannel1End += targetFrames;
    data.pChannel2End += targetFrames;

    return nullptr;
}

void Sample::clear()
{
    data_.pBuffer.reset();
    //samplePosition_ = 0;
    sampleRate_ = 0;
    sampleFile_ = juce::File();
}

Sample::ChannelData Sample::channel(unsigned int const index) const
{ //...mrmlj...think of a smarter solution (to provide true indexed channel access)...
    switch (index)
    {
    case 0:
        return channel1();
    case 1:
        return channel2();
        LE_DEFAULT_CASE_UNREACHABLE();
    }
}

Sample::ChannelData Sample::channel1() const
{
    return ChannelData(data_.pBuffer.get(), data_.pChannel1End);
}

Sample::ChannelData Sample::channel2() const
{
    return ChannelData(data_.pChannel2Beginning, data_.pChannel2End);
}

unsigned int &Sample::samplePosition()
{
    LE_ASSERT(*this);
    return samplePosition_;
}

void Sample::restart() { samplePosition_ = 0; }

bool Sample::DataHolder::recreate(std::size_t const newSizeInSamplesPerChannel)
{
    pBuffer.reset(new (std::nothrow) float[newSizeInSamplesPerChannel * fixedNumberOfChannels]);
    pChannel1End = pBuffer.get();
    pChannel2Beginning = pChannel1End + newSizeInSamplesPerChannel;
    pChannel2End = pChannel2Beginning;

    return pBuffer.get() != nullptr;
}

void Sample::DataHolder::takeDataFrom(DataHolder &other)
{
    pBuffer.swap(other.pBuffer);
    pChannel1End = other.pChannel1End;
    pChannel2Beginning = other.pChannel2Beginning;
    pChannel2End = other.pChannel2End;

// Catch (incorrect) subsequent usage of 'other' (now in undefined state).
#ifndef NDEBUG
    other.pBuffer.reset();
    other.pChannel1End = 0;
    other.pChannel2Beginning = 0;
    other.pChannel2End = 0;
#endif // NDEBUG
}

Sample::operator bool() const { return data_.pBuffer.get() != nullptr; }

//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
