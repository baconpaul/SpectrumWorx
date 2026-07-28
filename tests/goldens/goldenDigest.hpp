////////////////////////////////////////////////////////////////////////////////
///
/// \file goldenDigest.hpp
/// ----------------------
///
///   What a golden fixture actually stores.
///
///   A raw float dump of the whole matrix -- 57 effects x 2 FFT sizes x 2
/// overlap factors x 4 signals -- is ~30 MB before compression, which is a lot
/// of binary to put in a repository nobody can review. A digest is committed
/// instead: a bit-exact hash for same-platform regression, plus enough
/// numerical summary to compare across platforms within a tolerance, which is
/// what the plan asks the goldens to be able to do.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef goldenDigest_hpp__A7E4B93C_51D8_4A67_9F02_63C5D8E1B7A9
#define goldenDigest_hpp__A7E4B93C_51D8_4A67_9F02_63C5D8E1B7A9
//------------------------------------------------------------------------------
#include <array>
#include <cstdint>
#include <iosfwd>
#include <span>
#include <string>
#include <vector>
//------------------------------------------------------------------------------
namespace SWTest
{
//------------------------------------------------------------------------------

/// Log-spaced, so the bands are musically rather than linearly meaningful and
/// a shift of one bin at the top does not swamp everything below it.
constexpr unsigned int numberOfBands{8};

struct Digest
{
    std::uint64_t hash;                     ///< FNV-1a over the raw bits: same-platform exactness
    float peak;                             ///< max |x|
    float rms;                              ///< whole-render RMS
    float dcOffset;                         ///< mean; catches sign and windowing errors
    std::array<float, numberOfBands> bands; ///< RMS per log-spaced band, in dB
    std::uint32_t nonFiniteSamples;         ///< must be zero

    static Digest of(std::span<float const> interleaved, std::uint8_t channels, float sampleRate);
}; // struct Digest

/// One row of the fixture file.
struct Fixture
{
    std::string key; ///< effect/signal/fft/overlap, and the row's identity
    Digest digest;

    std::string serialise() const;
    static Fixture parse(std::string const &line);
};

/// \brief Cross-platform comparison.
///
/// The hash is checked only when \p exact -- it is the same-platform contract,
/// and any reassociation by a different compiler breaks it legitimately. The
/// numeric fields are always compared, in dB for the bands and relatively for
/// the rest.
struct Comparison
{
    bool matches;
    std::string explanation;
};

Comparison compare(Digest const &golden, Digest const &actual, bool exact);

//------------------------------------------------------------------------------
} // namespace SWTest
//------------------------------------------------------------------------------
#endif // goldenDigest_hpp
