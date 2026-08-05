////////////////////////////////////////////////////////////////////////////////
///
/// \file pitchDetector.hpp
/// -----------------------
///
/// Copyright (c) 2010 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef pitchDetector_hpp__1C810919_7127_459E_981C_46F7AC84CF16
#define pitchDetector_hpp__1C810919_7127_459E_981C_46F7AC84CF16
//------------------------------------------------------------------------------
#include "le/spectrumworx/effects/channelStateStatic.hpp"
#include "le/spectrumworx/engine/buffers.hpp"

#include <cstdint>
#include "le/utility/span.hpp"
//------------------------------------------------------------------------------
namespace LE
{
namespace SW
{
namespace Engine
{
class Setup;
} // namespace Engine
} // namespace SW
} // namespace LE
namespace LE
{
//------------------------------------------------------------------------------
//namespace Analysis
//{
//------------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
///
/// \class HPS
///
/// \brief Harmonic Product Spectrum. Holds product and position.
///
////////////////////////////////////////////////////////////////////////////////

struct HPS
{
    float harmonicProduct;
    std::uint16_t bin;
}; // struct HPS

////////////////////////////////////////////////////////////////////////////////
///
/// \class PitchDetector
///
/// \brief Finds pitch.
///
/// First PeakDetector detects peaks, then HPS spectrum is found. Then from
/// these two information pitch is estimated. Example: HPS says the pitch
/// is concentrated in bin x; if bin x belongs to the peak y, then parabola
/// fit frequency is taken from the peak y and that is the pitch.
///
////////////////////////////////////////////////////////////////////////////////

struct Peak;
class PeakDetector;

class PitchDetector
{
  public:
    struct ChannelState : LE::SW::Effects::StaticChannelState
    {
        float lastPitch;

        void reset();
    }; // struct ChannelState

  public:
    static float findPitch(SW::Engine::ReadOnlyDataRange const &amplitudes, ChannelState &,
                           float lfb, float hfb, SW::Engine::Setup const &);

  private:
    using HPSRange = LE::Utility::Span<HPS>;

    static void findHarmonicProductSpectrumAndSort(SW::Engine::ReadOnlyDataRange amplitudes,
                                                   HPSRange);
    static float estimatePitch(float lastPitch, float lfb, float hfb, HPSRange,
                               PeakDetector const &);
    static Peak const *binPeak(std::uint16_t bin, PeakDetector const &);
}; // class PitchDetector

//------------------------------------------------------------------------------
//} // namespace Analysis
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // pitchDetector_hpp
