////////////////////////////////////////////////////////////////////////////////
///
/// \file engineHarness.hpp
/// -----------------------
///
///   Drives the DSP core without a host. SpectrumWorxCore is abstract only in
/// the LE_NOVTABLE sense -- it has no pure virtuals -- but its constructor is
/// protected and Engine::Processor::modules() downcasts to it, so the engine
/// cannot be instantiated except through a derived class. This is that class,
/// plus the deterministic signal generators the goldens are rendered from.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef engineHarness_hpp__1C0F5A3E_74B2_49D6_BC81_2E9A05F3D7B4
#define engineHarness_hpp__1C0F5A3E_74B2_49D6_BC81_2E9A05F3D7B4
//------------------------------------------------------------------------------
#include "core/automatedModuleChain.hpp"
#include "core/spectrumWorxCore.hpp"

#include "le/spectrumworx/effects/configuration/constants.hpp"
#include "le/spectrumworx/engine/parameters.hpp"
#include "le/utility/buffers.hpp"

#include <cmath>
#include <cstdint>
#include <numbers>
#include <span>
#include <vector>
//------------------------------------------------------------------------------
namespace SWTest
{
//------------------------------------------------------------------------------

/// \brief The engine, instantiable, with its own Program.
class Engine : public LE::SW::SpectrumWorxCore
{
  public:
    using Core = LE::SW::SpectrumWorxCore;
    using Program = LE::SW::Program;

    Engine() { setProgram(program_); }

    /// SpectrumWorxCore::process is protected; a host reaches it through the
    /// per-format wrapper.
    using Core::process;

    Program &program() { return program_; }

    template <class Parameter> bool set(typename Parameter::param_type const value)
    {
        return Core::setGlobalParameter<Parameter>(*this, value);
    }

    LE::SW::GlobalParameters::Parameters &parameters() { return program_.parameters(); }

  private:
    Program program_;
}; // class Engine

//------------------------------------------------------------------------------
// Deterministic test signals
//------------------------------------------------------------------------------

/// \note Everything here is generated rather than loaded. The plan asked for
/// "one short real excerpt" as a fourth signal; a licence-clean one is not
/// something this repository has, so Voice below stands in for it -- a
/// harmonic stack with formant shaping and vibrato, which exercises the same
/// machinery (dense partials, a moving pitch) without shipping audio.
///                                       (28.07.2026.) (SW port)
enum struct Signal : std::uint8_t
{
    Impulse,
    Sweep,
    PinkNoise,
    Voice
};

constexpr char const *name(Signal const signal)
{
    switch (signal)
    {
    case Signal::Impulse:
        return "impulse";
    case Signal::Sweep:
        return "sweep";
    case Signal::PinkNoise:
        return "pink";
    case Signal::Voice:
        return "voice";
    }
    return "";
}

/// \brief xorshift32. Deterministic across platforms and standard libraries,
/// which std::mt19937 is too but the distributions on top of it are not.
class Rng
{
  public:
    explicit constexpr Rng(std::uint32_t const seed) noexcept : state_(seed | 1u) {}

    constexpr std::uint32_t next() noexcept
    {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        return state_;
    }

    /// Uniform in [-1, 1), exactly representable, no library dependence.
    constexpr float nextBipolar() noexcept
    {
        return static_cast<float>(static_cast<std::int32_t>(next() >> 8)) / 8388608.0f - 1.0f;
    }

  private:
    std::uint32_t state_;
}; // class Rng

void generate(Signal, std::span<float> mono, float sampleRate);

//------------------------------------------------------------------------------
// Rendering
//------------------------------------------------------------------------------

struct RenderSetup
{
    std::uint16_t fftSize;
    std::uint8_t overlapFactor;
    std::uint8_t numberOfChannels;
    std::uint32_t sampleRate;
    std::uint32_t blockSize;
};

/// \brief Runs one effect over one signal and returns the interleaved output.
///
/// \param effectIndex index into Effects::effectsList.hpp, or -1 for a bypassed
///        chain (which the goldens use to pin the engine's own WOLA path).
std::vector<float> render(RenderSetup const &, std::int8_t effectIndex, Signal,
                          std::uint32_t frames);

//------------------------------------------------------------------------------
} // namespace SWTest
//------------------------------------------------------------------------------
#endif // engineHarness_hpp
