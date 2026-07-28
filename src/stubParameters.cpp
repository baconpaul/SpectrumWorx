////////////////////////////////////////////////////////////////////////////////
///
/// \file stubParameters.cpp
/// -----------------------
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "stubParameters.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <cmath>
#include <cstring>
//------------------------------------------------------------------------------
namespace LE::SW
{
//------------------------------------------------------------------------------
namespace
{
constexpr std::uint32_t notFound{~std::uint32_t(0)};

struct GlobalDescription
{
    char const *name;
    double minimum;
    double maximum;
    double defaultValue;
    bool stepped;
    char const *unit;
    std::array<char const *, 8> labels; ///< nullptr terminated, stepped only
};

// The seven globals of the default build configuration: LE_SW_ENGINE_INPUT_MODE
// full, LE_SW_ENGINE_WINDOW_PRESUM off.
constexpr std::array<GlobalDescription, Skeleton::globals> globalDescriptions{{
    {"Input Gain", 0, 2, 1, false, "x", {}},
    {"Output Gain", 0, 2, 1, false, "x", {}},
    {"Mix", 0, 100, 100, false, "%", {}},
    {"FFT Size",
     0,
     7,
     3,
     true,
     "",
     {"256", "512", "1024", "2048", "4096", "8192", "16384", "32768"}},
    {"Overlap Factor", 0, 4, 2, true, "", {"1", "2", "4", "8", "16", nullptr}},
    {"Window Function",
     0,
     5,
     0,
     true,
     "",
     {"Hann", "Hamming", "Blackman", "Blackman-Harris", "Bartlett", "Rectangular", nullptr}},
    {"Input Mode", 0, 2, 0, true, "", {"Main", "Side Chain", "Both", nullptr}},
}};

constexpr std::array<char const *, Skeleton::lfoExportedParameters> lfoParameterNames{
    "Depth", "Rate", "Phase", "Minimum", "Maximum"};

std::string trimmed(std::string s)
{
    auto const notSpace([](unsigned char const c) { return !std::isspace(c); });
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

bool equalsIgnoringCase(std::string const &a, char const *b)
{
    return a.size() == std::strlen(b) &&
           std::equal(a.begin(), a.end(), b, [](char const x, char const y) {
               return std::tolower(static_cast<unsigned char>(x)) ==
                      std::tolower(static_cast<unsigned char>(y));
           });
}

void copyOut(std::string const &source, char *const display, std::uint32_t const size)
{
    if (size == 0)
        return;
    auto const length(std::min<std::size_t>(source.size(), size - 1));
    std::memcpy(display, source.data(), length);
    display[length] = '\0';
}
} // namespace

// A slot swap has to be visible to the host as more than a renaming: the
// parameter counts differ, so the ragged tail of each slot goes in and out of
// hiding as well.
std::vector<FakeEffect> const &fakeEffects()
{
    static std::vector<FakeEffect> const effects{
        {"Convolver", 5, {"Bypass", "Wet", "Dry", "Impulse", "Smoothing"}},
        {"Autotune", 7, {"Bypass", "Scale", "Root", "Strength", "Formants", "Attack", "Release"}},
        {"Freeze", 3, {"Bypass", "Freeze", "Blend"}},
        {"Vocoder",
         9,
         {"Bypass", "Bands", "Attack", "Release", "Carrier", "Mix", "Shift", "Tilt", "Q"}},
        {"Pitch Shift", 5, {"Bypass", "Semitones", "Cents", "Formant", "Mix"}},
        {"Random Phase",
         10,
         {"Bypass", "Amount", "Seed", "Rate", "Spread", "Low", "High", "Smooth", "Skew", "Mix"}},
    };
    return effects;
}

StubParameters::StubParameters()
{
    descriptions_.reserve(Skeleton::total);

    for (std::uint8_t index(0); index < Skeleton::globals; ++index)
        descriptions_.push_back({packParameterID(ParameterType::Global, index, 0, 0),
                                 ParameterType::Global, index, 0, 0});

    for (std::uint8_t module(0); module < Skeleton::modules; ++module)
        descriptions_.push_back({packParameterID(ParameterType::ModuleChain, module, 0, 0),
                                 ParameterType::ModuleChain, module, 0, 0});

    for (std::uint8_t module(0); module < Skeleton::modules; ++module)
        for (std::uint8_t parameter(0); parameter < Skeleton::parametersPerModule; ++parameter)
            descriptions_.push_back({packParameterID(ParameterType::Module, module, parameter, 0),
                                     ParameterType::Module, module, parameter, 0});

    for (std::uint8_t module(0); module < Skeleton::modules; ++module)
        for (std::uint8_t parameter(1); parameter < Skeleton::parametersPerModule; ++parameter)
            for (std::uint8_t lfo(0); lfo < Skeleton::lfoExportedParameters; ++lfo)
                descriptions_.push_back(
                    {packParameterID(ParameterType::LFO, module, parameter, lfo),
                     ParameterType::LFO, module, parameter, lfo});

    values_.resize(descriptions_.size());
    for (std::size_t index(0); index < descriptions_.size(); ++index)
        values_[index] = defaultValue(descriptions_[index]);
}

std::uint32_t StubParameters::indexOf(clap_id const id) const
{
    auto const type(static_cast<ParameterType>(id >> 24));
    auto const module(static_cast<std::uint8_t>((id >> 16) & 0xFF));
    auto const parameter(static_cast<std::uint8_t>((id >> 8) & 0xFF));
    auto const lfo(static_cast<std::uint8_t>(id & 0xFF));

    switch (type)
    {
    case ParameterType::Global:
        if (module < Skeleton::globals && parameter == 0 && lfo == 0)
            return module;
        break;

    case ParameterType::ModuleChain:
        if (module < Skeleton::modules && parameter == 0 && lfo == 0)
            return Skeleton::globals + module;
        break;

    case ParameterType::Module:
        if (module < Skeleton::modules && parameter < Skeleton::parametersPerModule && lfo == 0)
            return Skeleton::globals + Skeleton::modules + module * Skeleton::parametersPerModule +
                   parameter;
        break;

    case ParameterType::LFO:
        if (module < Skeleton::modules && parameter >= 1 &&
            parameter < Skeleton::parametersPerModule && lfo < Skeleton::lfoExportedParameters)
            return Skeleton::globals + Skeleton::modules + Skeleton::moduleParameters +
                   (module * Skeleton::lfoTargetsPerModule + (parameter - 1)) *
                       Skeleton::lfoExportedParameters +
                   lfo;
        break;
    }
    return notFound;
}

ParameterDescription const *StubParameters::byIndex(std::uint32_t const index) const
{
    return index < descriptions_.size() ? &descriptions_[index] : nullptr;
}

ParameterDescription const *StubParameters::byID(clap_id const id) const
{
    return byIndex(indexOf(id));
}

std::int8_t StubParameters::effectIn(std::uint8_t const moduleIndex) const
{
    // The one place a slot value is turned into a table index, so the one place
    // that has to survive a host writing something outside the declared range.
    auto const raw(std::lround(values_[static_cast<std::size_t>(Skeleton::globals) + moduleIndex]));
    if (raw < 0 || raw >= static_cast<long>(fakeEffects().size()))
        return noModule;
    return static_cast<std::int8_t>(raw);
}

void StubParameters::cycleEffectIn(std::uint8_t const moduleIndex)
{
    auto const count(static_cast<std::int8_t>(fakeEffects().size()));
    auto const next(static_cast<std::int8_t>(
        effectIn(moduleIndex) + 1 >= count ? noModule : effectIn(moduleIndex) + 1));
    values_[static_cast<std::size_t>(Skeleton::globals) + moduleIndex] = next;
}

std::pair<double, double> StubParameters::range(ParameterDescription const &description) const
{
    switch (description.type)
    {
    case ParameterType::Global:
    {
        auto const &global(globalDescriptions[description.module]);
        return {global.minimum, global.maximum};
    }
    case ParameterType::ModuleChain:
        return {noModule, static_cast<double>(fakeEffects().size()) - 1};
    case ParameterType::Module:
    case ParameterType::LFO:
        return {0, 1};
    }
    return {0, 1};
}

double StubParameters::defaultValue(ParameterDescription const &description) const
{
    switch (description.type)
    {
    case ParameterType::Global:
        return globalDescriptions[description.module].defaultValue;
    case ParameterType::ModuleChain:
        return noModule;
    case ParameterType::Module:
        return description.moduleParameter == 0 ? 0.0 : 0.5;
    case ParameterType::LFO:
        switch (description.lfoParameter)
        {
        case 0:
            return 0.0; // Depth
        case 1:
            return 0.5; // Rate
        case 4:
            return 1.0; // Maximum
        default:
            return 0.0;
        }
    }
    return 0.0;
}

bool StubParameters::isHidden(ParameterDescription const &description) const
{
    std::uint8_t parameter(0);
    switch (description.type)
    {
    case ParameterType::Global:
    case ParameterType::ModuleChain:
        return false;
    case ParameterType::Module:
        parameter = description.moduleParameter;
        break;
    case ParameterType::LFO:
        parameter = description.moduleParameter;
        break;
    }

    auto const effect(effectIn(description.module));
    if (effect == noModule)
        return true;
    return parameter >= fakeEffects()[static_cast<std::size_t>(effect)].parameterCount;
}

std::string StubParameters::nameOf(ParameterDescription const &description) const
{
    auto const effect(effectIn(description.module));
    auto const moduleParameterName([&](std::uint8_t const parameter) -> std::string {
        if (effect == noModule)
            return fmt::format("Parameter {}", parameter);
        auto const &fake(fakeEffects()[static_cast<std::size_t>(effect)]);
        if (parameter >= fake.parameterCount)
            return fmt::format("Parameter {}", parameter);
        return fake.parameterNames[parameter];
    });

    switch (description.type)
    {
    case ParameterType::Global:
        return globalDescriptions[description.module].name;
    case ParameterType::ModuleChain:
        return fmt::format("Module {}", description.module + 1);
    case ParameterType::Module:
        return moduleParameterName(description.moduleParameter);
    case ParameterType::LFO:
        return fmt::format("{} LFO {}", moduleParameterName(description.moduleParameter),
                           lfoParameterNames[description.lfoParameter]);
    }
    return {};
}

std::string StubParameters::moduleOf(ParameterDescription const &description) const
{
    auto const effect(effectIn(description.module));
    auto const slot([&] {
        return fmt::format(
            "Module {}/{}", description.module + 1,
            effect == noModule ? "(empty)" : fakeEffects()[static_cast<std::size_t>(effect)].name);
    });

    switch (description.type)
    {
    case ParameterType::Global:
        return "Global";
    case ParameterType::ModuleChain:
        return "Chain";
    case ParameterType::Module:
        return slot();
    case ParameterType::LFO:
        return slot() + "/LFO";
    }
    return {};
}

bool StubParameters::info(std::uint32_t const index, clap_param_info *const out) const
{
    auto const *const description(byIndex(index));
    if (!description)
        return false;

    std::memset(out, 0, sizeof(*out));
    out->id = description->id;
    out->cookie = nullptr;
    out->flags = CLAP_PARAM_IS_AUTOMATABLE;
    if (isHidden(*description))
        out->flags |= CLAP_PARAM_IS_HIDDEN;

    copyOut(nameOf(*description), out->name, CLAP_NAME_SIZE);
    copyOut(moduleOf(*description), out->module, CLAP_PATH_SIZE);

    auto const [minimum, maximum](range(*description));
    out->min_value = minimum;
    out->max_value = maximum;
    out->default_value = defaultValue(*description);

    switch (description->type)
    {
    case ParameterType::Global:
        if (globalDescriptions[description->module].stepped)
            out->flags |= CLAP_PARAM_IS_STEPPED | CLAP_PARAM_IS_ENUM;
        break;

    case ParameterType::ModuleChain:
        out->flags |= CLAP_PARAM_IS_STEPPED | CLAP_PARAM_IS_ENUM;
        break;

    case ParameterType::Module:
        if (description->moduleParameter == 0)
            out->flags |= CLAP_PARAM_IS_STEPPED;
        break;

    case ParameterType::LFO:
        break;
    }
    return true;
}

double StubParameters::value(clap_id const id) const
{
    auto const index(indexOf(id));
    return index == notFound ? 0.0 : values_[index];
}

bool StubParameters::setValue(clap_id const id, double const value)
{
    auto const index(indexOf(id));
    if (index == notFound)
        return false;

    auto const &description(descriptions_[index]);
    auto const [minimum, maximum](range(description));
    auto const previous(values_[index]);
    // Hosts do send out-of-range values, and NaN survives std::clamp.
    values_[index] = std::isfinite(value) ? std::clamp(value, minimum, maximum) : previous;

    return description.type == ParameterType::ModuleChain &&
           std::lround(previous) != std::lround(values_[index]);
}

void StubParameters::setValues(std::vector<double> const &values)
{
    auto const count(std::min(values.size(), values_.size()));
    for (std::size_t index(0); index < count; ++index)
        setValue(descriptions_[index].id, values[index]);
}

bool StubParameters::valueToText(clap_id const id, double const value, char *const display,
                                 std::uint32_t const size) const
{
    auto const *const description(byID(id));
    if (!description)
        return false;

    switch (description->type)
    {
    case ParameterType::Global:
    {
        auto const &global(globalDescriptions[description->module]);
        if (global.stepped)
        {
            auto const step(static_cast<std::size_t>(std::max<long>(0, std::lround(value))));
            if (step < global.labels.size() && global.labels[step])
            {
                copyOut(global.labels[step], display, size);
                return true;
            }
            return false;
        }
        copyOut(fmt::format("{:.2f} {}", value, global.unit), display, size);
        return true;
    }

    case ParameterType::ModuleChain:
    {
        auto const effect(std::lround(value));
        copyOut(effect < 0 || effect >= static_cast<long>(fakeEffects().size())
                    ? std::string("(empty)")
                    : fakeEffects()[static_cast<std::size_t>(effect)].name,
                display, size);
        return true;
    }

    case ParameterType::Module:
        if (description->moduleParameter == 0)
        {
            copyOut(value >= 0.5 ? "Bypassed" : "Active", display, size);
            return true;
        }
        copyOut(fmt::format("{:.1f} %", value * 100), display, size);
        return true;

    case ParameterType::LFO:
        copyOut(fmt::format("{:.1f} %", value * 100), display, size);
        return true;
    }
    return false;
}

bool StubParameters::textToValue(clap_id const id, char const *const display,
                                 double *const value) const
{
    auto const *const description(byID(id));
    if (!description || !display)
        return false;

    auto const text(trimmed(display));

    switch (description->type)
    {
    case ParameterType::Global:
    {
        auto const &global(globalDescriptions[description->module]);
        if (global.stepped)
        {
            for (std::size_t step(0); step < global.labels.size() && global.labels[step]; ++step)
                if (equalsIgnoringCase(text, global.labels[step]))
                {
                    *value = static_cast<double>(step);
                    return true;
                }
            return false;
        }
        break;
    }

    case ParameterType::ModuleChain:
    {
        if (equalsIgnoringCase(text, "(empty)"))
        {
            *value = noModule;
            return true;
        }
        auto const &effects(fakeEffects());
        for (std::size_t effect(0); effect < effects.size(); ++effect)
            if (equalsIgnoringCase(text, effects[effect].name))
            {
                *value = static_cast<double>(effect);
                return true;
            }
        return false;
    }

    case ParameterType::Module:
        if (description->moduleParameter == 0)
        {
            if (equalsIgnoringCase(text, "Bypassed"))
            {
                *value = 1;
                return true;
            }
            if (equalsIgnoringCase(text, "Active"))
            {
                *value = 0;
                return true;
            }
            return false;
        }
        break;

    case ParameterType::LFO:
        break;
    }

    try
    {
        std::size_t consumed(0);
        auto parsed(std::stod(text, &consumed));
        if (consumed == 0)
            return false;
        if (description->type != ParameterType::Global)
            parsed /= 100;
        *value = parsed;
        return true;
    }
    catch (std::exception const &)
    {
        return false;
    }
}

//------------------------------------------------------------------------------
} // namespace LE::SW
//------------------------------------------------------------------------------
