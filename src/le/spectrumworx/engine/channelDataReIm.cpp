////////////////////////////////////////////////////////////////////////////////
///
/// channelDataReIm.cpp
/// -------------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "channelDataReIm.hpp"

namespace LE::SW::Engine
{

ChannelData_ReIm::ChannelData_ReIm(FullChannelData_ReIm &data, IndexRange const &workingRange)
    : SubRange<FullChannelData_ReIm, DataRange>(data, workingRange)
{
}

} // namespace LE::SW::Engine
