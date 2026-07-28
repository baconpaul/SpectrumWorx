////////////////////////////////////////////////////////////////////////////////
///
/// \file effectNames.cpp
/// ---------------
///
/// Effect index <-> display name.
///
/// Copyright (c) 2011 - 2016. Little Endian Ltd.
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "allEffects.hpp"
#include "effectNames.hpp"
#include "effectsList.hpp"

#include "constants.hpp"
#include "le/utility/platformSpecifics.hpp"

#include <algorithm>
#include <array>
#include <string_view>

#include <cstdint>
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace SW
{
//------------------------------------------------------------------------------
namespace Effects
{
//------------------------------------------------------------------------------

LE_OPTIMIZE_FOR_SIZE_BEGIN()

namespace
{
using EffectNames = std::array<char const *LE_RESTRICT const, Constants::numberOfEffects>;

#define LE_SW_AUX_EFFECT_TITLE(folder, module, name, group) name::title,
LE_MSVC_SPECIFIC(LE_WEAK_SYMBOL_CONST)
EffectNames const effectNames = {{LE_SW_EFFECT_LIST(LE_SW_AUX_EFFECT_TITLE)}};
#undef LE_SW_AUX_EFFECT_TITLE
} // anonymous namespace

LE_COLD
char const *effectName(std::uint8_t const effectIndex) { return effectNames[effectIndex]; }

LE_COLD
std::int8_t effectIndex(std::string_view const effectName)
{
    auto const pFoundEffectName(std::ranges::find(effectNames, effectName));
    if (pFoundEffectName == effectNames.end())
        return -1;
    return static_cast<std::int8_t>(pFoundEffectName - effectNames.begin());
}

LE_OPTIMIZE_FOR_SIZE_END()

//------------------------------------------------------------------------------
} // namespace Effects
//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
