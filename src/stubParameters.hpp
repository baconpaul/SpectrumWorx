////////////////////////////////////////////////////////////////////////////////
///
/// \file stubParameters.hpp
/// -----------------------
///
/// A stand-in for the real parameter system, with its shape but none of its
/// substance. Stage 1 only. It exists to put the finished plugin's parameter
/// topology in front of a host as early as possible:
///
///   - the same 287 entry skeleton (7 globals, 5 slot selectors, 5x10 module
///     parameters, 5x9x5 LFO parameters)
///   - the same packed uint32 IDs, so that a slot's Nth parameter keeps its ID
///     across an effect swap and only its metadata changes
///   - names, ranges, module paths and value-to-text that all change when a
///     slot's effect changes, which is what forces a mid-session rescan
///
/// Deleted in stage 5, when src/core/host_interop is ported and the real
/// SW::ParameterID and Program drive all of this.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef stubParameters_hpp__0B7E4C31_59A8_42F6_8D1C_A46E93B0F572
#define stubParameters_hpp__0B7E4C31_59A8_42F6_8D1C_A46E93B0F572
//------------------------------------------------------------------------------
#include <clap/clap.h>

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
//------------------------------------------------------------------------------
namespace LE::SW
{
//------------------------------------------------------------------------------

/// src/configuration/constants.hpp and src/core/host_interop/parameters.hpp,
/// for the default build configuration. See doc/tech/parameter-system.md.
namespace Skeleton
{
inline constexpr std::uint8_t modules{5};
inline constexpr std::uint8_t parametersPerModule{10};
inline constexpr std::uint8_t globals{7};
inline constexpr std::uint8_t lfoExportedParameters{5};

/// Parameter 0 of a module is Bypass, which has no LFO.
inline constexpr std::uint8_t lfoTargetsPerModule{parametersPerModule - 1};

inline constexpr std::uint16_t moduleParameters{modules * parametersPerModule};
inline constexpr std::uint16_t lfoParameters{lfoExportedParameters * lfoTargetsPerModule * modules};
inline constexpr std::uint16_t total{globals + modules + moduleParameters + lfoParameters};

static_assert(total == 287, "The skeleton no longer matches the 2016 build");
} // namespace Skeleton

enum class ParameterType : std::uint8_t
{
    Global = 0,
    ModuleChain = 1,
    Module = 2,
    LFO = 3
};

/// The byte layout of SW::ParameterID (src/core/parameterID.hpp) on a little
/// endian machine: the discriminator in the high byte, then the module index,
/// the module parameter index and the LFO parameter index. Reproduced here so
/// the skeleton hands hosts the exact IDs the ported plugin will use, and we
/// find out now whether anything objects to a sparse, non-sequential id space.
constexpr clap_id packParameterID(ParameterType const type, std::uint8_t const module,
                                  std::uint8_t const moduleParameter,
                                  std::uint8_t const lfoParameter)
{
    return (static_cast<std::uint32_t>(type) << 24) | (static_cast<std::uint32_t>(module) << 16) |
           (static_cast<std::uint32_t>(moduleParameter) << 8) |
           static_cast<std::uint32_t>(lfoParameter);
}

struct ParameterDescription
{
    clap_id id;
    ParameterType type;
    std::uint8_t module;          ///< slot index, or the global index for Global
    std::uint8_t moduleParameter; ///< 0 == Bypass
    std::uint8_t lfoParameter;
};

/// Just enough of an effect to make a slot swap visible to a host: a name, a
/// parameter count that differs from its neighbours', and parameter names.
struct FakeEffect
{
    char const *name;
    std::uint8_t parameterCount; ///< including Bypass at index 0
    std::array<char const *, Skeleton::parametersPerModule> parameterNames;
};

/// -1 is an empty slot, matching AutomatedModuleChain's Minimum<noModule>.
inline constexpr std::int8_t noModule{-1};

std::vector<FakeEffect> const &fakeEffects();

class StubParameters
{
  public:
    StubParameters();

    std::uint32_t count() const { return static_cast<std::uint32_t>(descriptions_.size()); }

    ParameterDescription const *byIndex(std::uint32_t index) const;
    ParameterDescription const *byID(clap_id) const;

    bool info(std::uint32_t index, clap_param_info *) const;

    double value(clap_id) const;
    /// Returns true if this was a slot selector whose effect actually changed.
    bool setValue(clap_id, double);

    bool valueToText(clap_id, double, char *display, std::uint32_t size) const;
    bool textToValue(clap_id, char const *display, double *) const;

    std::vector<double> const &values() const { return values_; }
    void setValues(std::vector<double> const &);

    /// The effect index in a slot, or noModule.
    std::int8_t effectIn(std::uint8_t moduleIndex) const;
    /// Advances a slot to the next effect, wrapping through the empty state.
    void cycleEffectIn(std::uint8_t moduleIndex);

  private:
    std::uint32_t indexOf(clap_id) const;
    std::pair<double, double> range(ParameterDescription const &) const;
    double defaultValue(ParameterDescription const &) const;
    /// Hidden when the slot is empty or the effect does not reach this
    /// parameter - the real plugin's slots are ragged the same way.
    bool isHidden(ParameterDescription const &) const;
    std::string nameOf(ParameterDescription const &) const;
    std::string moduleOf(ParameterDescription const &) const;

    std::vector<ParameterDescription> descriptions_;
    std::vector<double> values_;
}; // class StubParameters

//------------------------------------------------------------------------------
} // namespace LE::SW
//------------------------------------------------------------------------------
#endif // stubParameters_hpp
