////////////////////////////////////////////////////////////////////////////////
///
/// \file mixResidue.hpp
/// --------------------
///
///   What "the output is not quite the input" is as a number: one rendered
/// channel against the mono signal it was fed, at a stated delay.
///
/// \note Shared by core/mixTransparencyTests.cpp and
/// clap/mixThroughThePluginTests.cpp, which ask the same question of the engine
/// and of the plugin around it -- two copies of this would be two chances for
/// the two to stop meaning the same thing.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef mixResidue_hpp__8E2B6C14_0A73_4F5D_BA96_C31E7D0428AF
#define mixResidue_hpp__8E2B6C14_0A73_4F5D_BA96_C31E7D0428AF
//------------------------------------------------------------------------------
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>

namespace SWTest
{

struct Residue
{
    double worst{0};
    std::uint32_t worstAt{0};
    double errorRms{0};
    double inputRms{0};
    double outputPeak{0};
    std::uint32_t compared{0};

    double errorDb() const
    {
        return (errorRms > 0) ? 20 * std::log10(errorRms / inputRms) : -1000.0;
    }
};

/// \brief `rendered[n + latency]` against `input[n]`, over everything they share.
inline Residue mixResidue(std::span<float const> const rendered, std::span<float const> const input,
                          std::uint32_t const latency)
{
    Residue residue;
    if ((input.size() <= latency) || (rendered.size() <= latency))
        return residue;
    residue.compared =
        static_cast<std::uint32_t>(std::min(input.size(), rendered.size() - latency));

    double errorSquares{0}, inputSquares{0};
    for (std::uint32_t frame(0); frame < residue.compared; ++frame)
    {
        auto const out(rendered[std::size_t{frame} + latency]);
        auto const expected(input[frame]);
        auto const difference(std::abs(static_cast<double>(out) - expected));
        if (difference > residue.worst)
        {
            residue.worst = difference;
            residue.worstAt = frame;
        }
        errorSquares += difference * difference;
        inputSquares += double{expected} * expected;
        residue.outputPeak = std::max(residue.outputPeak, std::abs(double{out}));
    }
    residue.errorRms = std::sqrt(errorSquares / residue.compared);
    residue.inputRms = std::sqrt(inputSquares / residue.compared);
    return residue;
}

/// \brief The latency region, which owes the host silence and nothing else.
inline double leadingPeak(std::span<float const> const rendered, std::uint32_t const latency)
{
    double peak{0};
    for (std::uint32_t frame(0); frame < latency && frame < rendered.size(); ++frame)
        peak = std::max(peak, std::abs(double{rendered[frame]}));
    return peak;
}

} // namespace SWTest

#endif // mixResidue_hpp
