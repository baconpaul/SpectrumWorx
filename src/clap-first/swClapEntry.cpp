////////////////////////////////////////////////////////////////////////////////
///
/// \file swClapEntry.cpp
/// --------------------
///
/// The exported clap_entry symbol. clap-wrapper recompiles this one file once
/// per plugin format and links it against the sw-impl static library, which is
/// where everything else lives.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include <clap/clap.h>

#include "swClapEntryImpl.hpp"
//------------------------------------------------------------------------------
extern "C"
{
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#endif

    // clang-format off
    CLAP_EXPORT extern struct clap_plugin_entry const clap_entry = {
        CLAP_VERSION,
        LE::SW::ClapFirst::clapInit,
        LE::SW::ClapFirst::clapDeinit,
        LE::SW::ClapFirst::getFactory
    };
    // clang-format on

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
}
