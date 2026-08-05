////////////////////////////////////////////////////////////////////////////////
///
/// indexRange.cpp
/// --------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "indexRange.hpp"

#include "le/utility/assert.hpp"
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

IndexRange::IndexRange(value_type const begin, value_type const end) : Pair(begin, end)
{
    LE_ASSERT_MSG(isValid(), "Invalid range");
}

void IndexRange::setFirst(value_type const newBeginning)
{
    LE_ASSERT_MSG(isValid(newBeginning, end()), "Invalid range");
    Pair::first = newBeginning;
}

void IndexRange::setLast(value_type const newLast)
{
    LE_ASSERT_MSG(isValid(first(), newLast + 1), "Invalid range");
    Pair::second = newLast + 1;
}

void IndexRange::setNewRange(value_type const newBeginning, value_type const newLast)
{
    LE_ASSERT_MSG(isValid(newBeginning, newLast + 1), "Invalid range");
    Pair::first = newBeginning;
    Pair::second = newLast + 1;
}

bool IndexRange::isValid(value_type const first, value_type const end) { return first <= end; }

bool IndexRange::isValid() const { return isValid(first(), end()); }

//------------------------------------------------------------------------------
} // namespace Effects
//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
