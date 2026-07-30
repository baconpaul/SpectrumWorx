////////////////////////////////////////////////////////////////////////////////
///
/// \file effectIndexToGroupMapping.hpp
/// -----------------------------
///
/// Effect index -> menu group, and the group list itself. Was 57 explicit
/// template specialisations written by CMake.
///
/// Copyright (c) 2011 - 2016. Little Endian Ltd.
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef effectIndexToGroupMapping_hpp__254A1D12_F9E2_40C8_B703_2E7EF2A152BE
#define effectIndexToGroupMapping_hpp__254A1D12_F9E2_40C8_B703_2E7EF2A152BE
//------------------------------------------------------------------------------
#include "effectGroups.hpp"
#include "effectsList.hpp"

#include "le/utility/typeList.hpp"

#include <tuple>
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

#define LE_SW_AUX_EFFECT_GROUP(folder, module, name, group) ModuleGroups::group,
using EffectGroups = std::tuple<LE_SW_EFFECT_LIST(LE_SW_AUX_EFFECT_GROUP) void>;
#undef LE_SW_AUX_EFFECT_GROUP

template <unsigned int effectIndex> struct Group
{
    using type = std::tuple_element_t<effectIndex, EffectGroups>;
};

using Groups = LE::Utility::TypeList<
    ModuleGroups::Pitch,
    ModuleGroups::Timbre,
    ModuleGroups::Time,
    ModuleGroups::Space,
    ModuleGroups::Phase,
    ModuleGroups::Loudness,
    ModuleGroups::Combine,
    ModuleGroups::PVD,
    ModuleGroups::Misc>;

//------------------------------------------------------------------------------
} // namespace Effects
//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // effectIndexToGroupMapping_hpp
