////////////////////////////////////////////////////////////////////////////////
///
/// \file swClapEntryImpl.cpp
/// ------------------------
///
/// The CLAP plugin factory, plus the AUv2 sub-factory clap-wrapper probes at
/// build time to generate the Audio Unit's Info.plist.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "swClapEntryImpl.hpp"
#include "spectrumWorxCLAP.hpp"

#include <clap/clap.h>
#include <clapwrapper/auv2.h>

#include <cstring>
//------------------------------------------------------------------------------
namespace LE::SW::ClapFirst
{
//------------------------------------------------------------------------------
namespace
{
std::uint32_t pluginCount(clap_plugin_factory const *) { return 1; }

clap_plugin_descriptor const *pluginDescriptor(clap_plugin_factory const *,
                                               std::uint32_t const index)
{
    return index == 0 ? descriptor() : nullptr;
}

clap_plugin const *create(clap_plugin_factory const *, clap_host const *const host,
                          char const *const pluginID)
{
    if (std::strcmp(pluginID, descriptor()->id) == 0)
        return createPlugin(host);
    return nullptr;
}

bool auv2Info(clap_plugin_factory_as_auv2 const *, std::uint32_t const index,
              clap_plugin_info_as_auv2_t *const info)
{
    if (index != 0)
        return false;

    info->au_type[0] = 0; // derived from the CLAP features
    std::strncpy(info->au_subt, "SpWx", 5);
    return true;
}

constexpr clap_plugin_factory factory{pluginCount, pluginDescriptor, create};

constexpr clap_plugin_factory_as_auv2 auv2Factory{"LiEn", "Little Endian Ltd", auv2Info};
} // namespace

bool clapInit(char const *) { return true; }

void clapDeinit() {}

void const *getFactory(char const *const factoryID)
{
    if (std::strcmp(factoryID, CLAP_PLUGIN_FACTORY_ID) == 0)
        return &factory;
    if (std::strcmp(factoryID, CLAP_PLUGIN_FACTORY_INFO_AUV2) == 0)
        return &auv2Factory;
    return nullptr;
}

//------------------------------------------------------------------------------
} // namespace LE::SW::ClapFirst
//------------------------------------------------------------------------------
