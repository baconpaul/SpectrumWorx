////////////////////////////////////////////////////////////////////////////////
///
/// \file clapHelpersInstantiation.cpp
/// ---------------------------------
///
/// clap-helpers is a template library whose definitions live in .hxx files.
/// This is the one translation unit that instantiates them.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "spectrumWorxCLAP.hpp"

#include <clap/helpers/host-proxy.hh>
#include <clap/helpers/host-proxy.hxx>
#include <clap/helpers/plugin.hxx>

#include <type_traits>

namespace clap::helpers
{
template class Plugin<LE::SW::misbehaviourLevel, LE::SW::checkingLevel>;
template class HostProxy<LE::SW::misbehaviourLevel, LE::SW::checkingLevel>;
} // namespace clap::helpers

static_assert(
    std::is_same_v<LE::SW::PluginHelper,
                   clap::helpers::Plugin<LE::SW::misbehaviourLevel, LE::SW::checkingLevel>>);
