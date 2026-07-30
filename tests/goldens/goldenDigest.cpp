////////////////////////////////////////////////////////////////////////////////
///
/// goldenDigest.cpp
/// ----------------
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "goldenDigest.hpp"

// For LE_ACC_FFT / LE_PFFFT: which FFT backend is under the render is part of
// what makes a fixture file's bit-exact hashes meaningful or not.
#include "le/math/dft/fft.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <sstream>
//------------------------------------------------------------------------------
namespace SWTest
{
//------------------------------------------------------------------------------

namespace
{
constexpr float silenceFloor{-200.0f}; ///< dB reported for an empty band

std::uint64_t fnv1a(std::span<float const> const samples)
{
    // Over the raw bits, so a change of one ulp anywhere is a different hash.
    std::uint64_t hash{0xCBF29CE484222325ull};
    for (auto const sample : samples)
    {
        std::uint32_t bits;
        std::memcpy(&bits, &sample, sizeof(bits));
        // -0.0 and +0.0 compare equal but hash differently; normalise, because
        // which one a denormal-flushing branch produces is not interesting.
        if (bits == 0x80000000u)
            bits = 0;
        for (unsigned int byte(0); byte < sizeof(bits); ++byte)
        {
            hash ^= (bits >> (byte * 8)) & 0xFFu;
            hash *= 0x100000001B3ull;
        }
    }
    return hash;
}

float dB(double const amplitude)
{
    return (amplitude > 0) ? static_cast<float>(20 * std::log10(amplitude)) : silenceFloor;
}
} // anonymous namespace

Digest Digest::of(std::span<float const> const interleaved, std::uint8_t const channels,
                  float const sampleRate)
{
    Digest digest{};
    digest.hash = fnv1a(interleaved);

    double sumOfSquares{0};
    double sum{0};
    float peak{0};
    std::uint32_t nonFinite{0};
    for (auto const sample : interleaved)
    {
        if (!std::isfinite(sample))
        {
            ++nonFinite;
            continue;
        }
        peak = std::max(peak, std::abs(sample));
        sum += sample;
        sumOfSquares += static_cast<double>(sample) * sample;
    }
    auto const count(interleaved.size() ? interleaved.size() : 1);
    digest.peak = peak;
    digest.rms = static_cast<float>(std::sqrt(sumOfSquares / count));
    digest.dcOffset = static_cast<float>(sum / count);
    digest.nonFiniteSamples = nonFinite;

    // Band energies, from a plain DFT of the first channel decimated to a fixed
    // 1024 point window. Fixed size and fixed window position, so the summary
    // does not move when the render length does.
    constexpr std::size_t window{1024};
    auto const frames(interleaved.size() / std::max<std::uint8_t>(channels, 1));
    std::vector<double> magnitude(window / 2 + 1, 0.0);
    if (frames >= window)
    {
        // Average the magnitude spectrum over as many non-overlapping windows
        // as fit, which is far more stable across platforms than a single one.
        std::size_t windows{0};
        for (std::size_t start(0); start + window <= frames; start += window)
        {
            for (std::size_t bin(0); bin < magnitude.size(); ++bin)
            {
                double real{0}, imaginary{0};
                for (std::size_t n(0); n < window; ++n)
                {
                    auto const sample(interleaved[(start + n) * channels]);
                    if (!std::isfinite(sample))
                        continue;
                    // Hann, to keep leakage from smearing the band edges.
                    auto const w(0.5 * (1 - std::cos(2 * M_PI * n / (window - 1))));
                    auto const angle(-2 * M_PI * static_cast<double>(bin) * n / window);
                    real += sample * w * std::cos(angle);
                    imaginary += sample * w * std::sin(angle);
                }
                magnitude[bin] += std::sqrt(real * real + imaginary * imaginary);
            }
            ++windows;
        }
        if (windows)
            for (auto &value : magnitude)
                value /= windows;
    }

    // Eight log-spaced bands from 20 Hz to Nyquist.
    auto const binWidth(sampleRate / window);
    auto const lowest(20.0);
    auto const highest(static_cast<double>(sampleRate) / 2);
    auto const ratio(std::pow(highest / lowest, 1.0 / numberOfBands));
    auto edge(lowest);
    for (unsigned int band(0); band < numberOfBands; ++band)
    {
        auto const next(edge * ratio);
        auto const firstBin(static_cast<std::size_t>(edge / binWidth));
        auto const lastBin(
            std::min(magnitude.size() - 1, static_cast<std::size_t>(next / binWidth)));
        double energy{0};
        std::size_t bins{0};
        for (auto bin(firstBin); bin <= lastBin; ++bin, ++bins)
            energy += magnitude[bin] * magnitude[bin];
        digest.bands[band] = dB(bins ? std::sqrt(energy / bins) : 0.0);
        edge = next;
    }
    return digest;
}

std::string Fixture::serialise() const
{
    char buffer[512];
    int written(std::snprintf(buffer, sizeof(buffer), "%-44s %016llx %.9g %.9g %.9g %u",
                              key.c_str(), static_cast<unsigned long long>(digest.hash),
                              static_cast<double>(digest.peak), static_cast<double>(digest.rms),
                              static_cast<double>(digest.dcOffset), digest.nonFiniteSamples));
    for (auto const band : digest.bands)
        written += std::snprintf(buffer + written, sizeof(buffer) - written, " %.4f",
                                 static_cast<double>(band));
    return buffer;
}

Fixture Fixture::parse(std::string const &line)
{
    Fixture fixture{};
    std::istringstream stream(line);
    std::string hash;
    stream >> fixture.key >> hash >> fixture.digest.peak >> fixture.digest.rms >>
        fixture.digest.dcOffset >> fixture.digest.nonFiniteSamples;
    fixture.digest.hash = std::stoull(hash, nullptr, 16);
    for (auto &band : fixture.digest.bands)
        stream >> band;
    return fixture;
}

std::string provenance()
{
#if defined(__APPLE__)
    char const *const os{"macos"};
#elif defined(_WIN32)
    char const *const os{"windows"};
#elif defined(__linux__)
    char const *const os{"linux"};
#else
    char const *const os{"unknown-os"};
#endif

#if defined(__aarch64__) || defined(_M_ARM64)
    char const *const architecture{"arm64"};
#elif defined(__x86_64__) || defined(_M_X64)
    char const *const architecture{"x86_64"};
#else
    char const *const architecture{"unknown-arch"};
#endif

#if defined(LE_ACC_FFT)
    char const *const fft{"accelerate"};
#elif defined(LE_PFFFT)
    char const *const fft{"pffft"};
#else
    char const *const fft{"unknown-fft"};
#endif

    return std::string(os) + "-" + architecture + "/" + fft;
}

namespace
{
constexpr float amplitudeTolerance{1e-4f};
constexpr float bandTolerance{0.01f}; // dB

float relative(float const expected, float const got)
{
    auto const scale(std::max({std::abs(expected), std::abs(got), 1e-6f}));
    return std::abs(expected - got) / scale;
}
} // anonymous namespace

float Deltas::worst() const { return std::max(peak, rms); }

bool Deltas::withinTolerance() const
{
    return !nonFiniteDiffers && (peak <= amplitudeTolerance) && (rms <= amplitudeTolerance) &&
           (dcOffset <= amplitudeTolerance) && (band <= bandTolerance);
}

Deltas deltas(Digest const &golden, Digest const &actual)
{
    Deltas measured;
    measured.nonFiniteDiffers = (golden.nonFiniteSamples != actual.nonFiniteSamples);
    measured.peak = relative(golden.peak, actual.peak);
    measured.rms = relative(golden.rms, actual.rms);
    // dcOffset is an absolute quantity near zero, so a relative test is
    // meaningless for it; compare against the render's own scale.
    measured.dcOffset = std::abs(golden.dcOffset - actual.dcOffset) / std::max(golden.rms, 1e-6f);

    for (unsigned int band(0); band < numberOfBands; ++band)
    {
        // A band that is silent in both is not interesting, however far apart
        // the two floors happen to be.
        auto const bothSilent((golden.bands[band] <= silenceFloor + 1) &&
                              (actual.bands[band] <= silenceFloor + 1));
        if (bothSilent)
            continue;
        auto const difference(std::abs(golden.bands[band] - actual.bands[band]));
        if (difference > measured.band)
        {
            measured.band = difference;
            measured.worstBand = band;
            /// \note Two bands can both be far below anything audible and still
            /// be tens of dB apart -- -189 dB against -180 dB is nine dB of
            /// "drift" over a signal nobody will ever hear. Flagged rather than
            /// excluded, so the report can say which it is.
            measured.bandsNearSilence = std::max(golden.bands[band], actual.bands[band]) < -120.0f;
        }
    }
    return measured;
}

Comparison compare(Digest const &golden, Digest const &actual, bool const exact)
{
    auto const fail([](std::string reason) { return Comparison{false, std::move(reason)}; });

    auto const measured(deltas(golden, actual));

    if (measured.nonFiniteDiffers)
        return fail("non-finite sample count " + std::to_string(actual.nonFiniteSamples) +
                    ", expected " + std::to_string(golden.nonFiniteSamples));

    if (exact && (actual.hash != golden.hash))
        return fail("bit-exact hash mismatch");

    if (measured.peak > amplitudeTolerance)
        return fail("peak " + std::to_string(actual.peak) + ", expected " +
                    std::to_string(golden.peak));
    if (measured.rms > amplitudeTolerance)
        return fail("rms " + std::to_string(actual.rms) + ", expected " +
                    std::to_string(golden.rms));
    if (measured.dcOffset > amplitudeTolerance)
        return fail("dc offset " + std::to_string(actual.dcOffset) + ", expected " +
                    std::to_string(golden.dcOffset));
    if (measured.band > bandTolerance)
        return fail("band " + std::to_string(measured.worstBand) + " " +
                    std::to_string(actual.bands[measured.worstBand]) + " dB, expected " +
                    std::to_string(golden.bands[measured.worstBand]) + " dB");

    return Comparison{true, {}};
}

//------------------------------------------------------------------------------
} // namespace SWTest
//------------------------------------------------------------------------------
