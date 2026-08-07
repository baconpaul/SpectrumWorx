////////////////////////////////////////////////////////////////////////////////
///
/// \file windows.hpp
/// -----------------
///
/// Copyright (c) 2011 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef windows_hpp__1A80D559_4313_4EBB_89BA_4740817A0C44
#define windows_hpp__1A80D559_4313_4EBB_89BA_4740817A0C44
//------------------------------------------------------------------------------
#include "le/spectrumworx/engine/configuration.hpp"

#include "le/utility/platformSpecifics.hpp"
#include "le/utility/span.hpp"

namespace LE::Math
{

using DataRange = LE::Utility::Span<float>;

void calculateWindow(DataRange const &window, LE::SW::Engine::Constants::Window);

} // namespace LE::Math

#endif // windows_hpp
