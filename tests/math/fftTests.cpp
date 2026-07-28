////////////////////////////////////////////////////////////////////////////////
///
/// fftTests.cpp
/// ------------
///
///   The Accelerate-backed real FFT, which the golden fixtures in 3.6 sit on
/// top of. Everything here is a property of the transform rather than a
/// captured output, so it survives the stage 4 backend swap unchanged.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "le/math/constants.hpp"
#include "le/math/dft/fft.hpp"
#include "le/spectrumworx/engine/buffers.hpp"
#include "le/utility/buffers.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
//------------------------------------------------------------------------------

using Catch::Approx;
namespace Engine = LE::SW::Engine;
namespace Math = LE::Math;

namespace
{
/// Owns the shared storage the FFT positions itself in.
class FFTFixture
{
  public:
    explicit FFTFixture(std::uint16_t const fftSize)
    {
        Engine::StorageFactors const factors{fftSize,
#if LE_SW_ENGINE_WINDOW_PRESUM
                                             1,
#endif // LE_SW_ENGINE_WINDOW_PRESUM
                                             2, 1, 44100};
        auto const required(Math::FFT_float_real_1D::requiredStorage(factors));
        REQUIRE(storage_.resize(required + LE::Utility::Constants::vectorAlignment));
        Engine::Storage span(storage_.begin(), storage_.end());
        fft_.resize(factors, span);
        REQUIRE(fft_.size() == fftSize);
    }

    Math::FFT_float_real_1D const &fft() const { return fft_; }

  private:
    LE::Utility::AlignedHeapBuffer<char> storage_;
    Math::FFT_float_real_1D fft_;
};

constexpr std::uint16_t fftSize{256};
} // anonymous namespace

TEST_CASE("A forward then inverse transform is the identity", "[math][fft]")
{
    FFTFixture const fixture(fftSize);

    LE::Utility::AlignedHeapBuffer<float> time;
    REQUIRE(time.resize(fftSize));
    LE::Utility::AlignedHeapBuffer<float> imaginary;
    REQUIRE(imaginary.resize(fftSize / 2 + 1));

    LE::Utility::AlignedHeapBuffer<float> original;
    REQUIRE(original.resize(fftSize));
    for (std::uint16_t index(0); index < fftSize; ++index)
    {
        auto const t(static_cast<float>(index) / fftSize);
        original[index] = std::sin(2 * Math::Constants::pi * 5 * t) +
                          0.25f * std::cos(2 * Math::Constants::pi * 17 * t);
        time[index] = original[index];
    }

    fixture.fft().transform(time.begin(), imaginary.begin(), fftSize);
    fixture.fft().inverseTransform(time.begin(), imaginary.begin(), fftSize);

    for (std::uint16_t index(0); index < fftSize; ++index)
        CHECK(time[index] == Approx(original[index]).margin(1e-3));
}

TEST_CASE("A pure tone at a bin centre lands in that bin", "[math][fft]")
{
    FFTFixture const fixture(fftSize);
    constexpr std::uint16_t bin{20};

    LE::Utility::AlignedHeapBuffer<float> reals;
    REQUIRE(reals.resize(fftSize));
    LE::Utility::AlignedHeapBuffer<float> imags;
    REQUIRE(imags.resize(fftSize / 2 + 1));

    for (std::uint16_t index(0); index < fftSize; ++index)
        reals[index] = std::cos(2 * Math::Constants::pi * bin * index / fftSize);

    fixture.fft().transform(reals.begin(), imags.begin(), fftSize);

    LE::Utility::AlignedHeapBuffer<float> amplitudes;
    REQUIRE(amplitudes.resize(fftSize / 2 + 1));
    Math::amplitudes(reals.begin(), imags.begin(), amplitudes.begin(), amplitudes.end());

    auto const peak(std::max_element(amplitudes.begin(), amplitudes.end()));
    CHECK((peak - amplitudes.begin()) == bin);

    // and nothing else is anywhere near it
    auto const peakValue(*peak);
    for (std::uint16_t index(0); index < amplitudes.size(); ++index)
        if ((index < bin - 1) || (index > bin + 1))
            CHECK(amplitudes[index] < peakValue * 0.01f);
}

TEST_CASE("A constant signal is entirely DC", "[math][fft]")
{
    FFTFixture const fixture(fftSize);

    LE::Utility::AlignedHeapBuffer<float> reals;
    REQUIRE(reals.resize(fftSize));
    LE::Utility::AlignedHeapBuffer<float> imags;
    REQUIRE(imags.resize(fftSize / 2 + 1));
    std::fill(reals.begin(), reals.end(), 1.0f);

    fixture.fft().transform(reals.begin(), imags.begin(), fftSize);

    LE::Utility::AlignedHeapBuffer<float> amplitudes;
    REQUIRE(amplitudes.resize(fftSize / 2 + 1));
    Math::amplitudes(reals.begin(), imags.begin(), amplitudes.begin(), amplitudes.end());

    CHECK(amplitudes[0] > 0);
    for (std::uint16_t index(1); index < amplitudes.size(); ++index)
        CHECK(amplitudes[index] == Approx(0.0f).margin(amplitudes[0] * 1e-4f));
}

TEST_CASE("The transform is linear", "[math][fft]")
{
    FFTFixture const fixture(fftSize);
    auto const transform([&](float const scale, LE::Utility::AlignedHeapBuffer<float> &reals,
                             LE::Utility::AlignedHeapBuffer<float> &imags) {
        REQUIRE(reals.resize(fftSize));
        REQUIRE(imags.resize(fftSize / 2 + 1));
        for (std::uint16_t index(0); index < fftSize; ++index)
            reals[index] = scale * std::sin(2 * Math::Constants::pi * 9 * index / fftSize);
        fixture.fft().transform(reals.begin(), imags.begin(), fftSize);
    });

    LE::Utility::AlignedHeapBuffer<float> singleReals, singleImags, doubleReals, doubleImags;
    transform(1.0f, singleReals, singleImags);
    transform(2.0f, doubleReals, doubleImags);

    for (std::uint16_t index(0); index < fftSize / 2 + 1; ++index)
    {
        CHECK(doubleReals[index] == Approx(2 * singleReals[index]).margin(1e-3));
        CHECK(doubleImags[index] == Approx(2 * singleImags[index]).margin(1e-3));
    }
}
