////////////////////////////////////////////////////////////////////////////////
///
/// \file sample.hpp
/// ----------------
///
/// Copyright (c) 2010 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef sample_hpp__A94590F9_6645_4380_8512_060CF57872FA
#define sample_hpp__A94590F9_6645_4380_8512_060CF57872FA
//------------------------------------------------------------------------------
#include "le/utility/platformSpecifics.hpp"
#include "le/utility/span.hpp"

#include <juce/juce_core/juce_core.h>

#include <memory>
#include <mutex>
//------------------------------------------------------------------------------

namespace LE
{
//------------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
///
/// \class Sample
///
/// \brief Loads and streams a sample from memory, looped, always stereo.
///
////////////////////////////////////////////////////////////////////////////////

namespace Utility
{
#ifdef _WIN32
using CriticalSection = std::mutex;
#else
class CriticalSection;
#endif // _WIN32
} // namespace Utility

class Sample
{
  public:
    using ChannelData = LE::Utility::Span<float const>;

  public:
    char const *load(juce::File const &sampleFile, unsigned int desiredSampleRate,
                     Utility::CriticalSection &);

    void clear();

    ChannelData channel(unsigned int index) const;

    ChannelData channel1() const;
    ChannelData channel2() const;

    unsigned int &samplePosition();
    void restart();

    juce::File const &sampleFile() const { return sampleFile_; }
    juce::String const &sampleFileName() const { return sampleFile().getFullPathName(); }

    static juce::String supportedFormats();

    explicit operator bool() const;

  private:
    struct DataHolder
    {
        bool recreate(std::size_t newSizeInSamplesPerChannel);

        void takeDataFrom(DataHolder &other);

        std::unique_ptr<float[]> pBuffer;
        float *pChannel1End;
        float *pChannel2Beginning;
        float *pChannel2End;
    };

    class Impl;

  private:
    // To be implemented for each platform separately.
    static char const *doLoad(juce::String const &sampleFileName, unsigned int desiredSampleRate,
                              DataHolder &data);

  private:
    DataHolder data_;

    unsigned int samplePosition_;

    juce::File sampleFile_;

    static unsigned int const fixedNumberOfChannels = 2;
}; // class Sample

//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // sample_hpp
