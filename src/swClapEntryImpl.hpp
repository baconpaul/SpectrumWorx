////////////////////////////////////////////////////////////////////////////////
///
/// \file swClapEntryImpl.hpp
/// ------------------------
///
/// The three symbols src/clap-first/swClapEntry.cpp needs from this static
/// library to build a clap_plugin_entry for every plugin format.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef swClapEntryImpl_hpp__D4A81E67_23BC_4C95_B60F_8E5721A0C3D4
#define swClapEntryImpl_hpp__D4A81E67_23BC_4C95_B60F_8E5721A0C3D4

namespace LE::SW::ClapFirst
{

bool clapInit(char const *pluginPath);
void clapDeinit();
void const *getFactory(char const *factoryID);

} // namespace LE::SW::ClapFirst

#endif // swClapEntryImpl_hpp
