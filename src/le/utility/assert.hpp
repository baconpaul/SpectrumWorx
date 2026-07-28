////////////////////////////////////////////////////////////////////////////////
///
/// \file assert.hpp
/// ----------------
///
/// Copyright (c) 2010 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef assert_hpp__6E5E1CC9_AFB3_4326_A27F_473AF9722938
#define assert_hpp__6E5E1CC9_AFB3_4326_A27F_473AF9722938
//------------------------------------------------------------------------------
#ifdef BOOST_ENABLE_ASSERT_HANDLER
#include "boost/assert.hpp"
#else
#include <cassert>
#endif // BOOST_ENABLE_ASSERT_HANDLER
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace Utility
{
//------------------------------------------------------------------------------

#ifdef BOOST_ENABLE_ASSERT_HANDLER
#define LE_ASSERT BOOST_ASSERT
#define LE_ASSERT_MSG BOOST_ASSERT_MSG
#define LE_VERIFY BOOST_VERIFY
#define LE_VERIFY_MSG BOOST_VERIFY_MSG
#else
#define LE_ASSERT assert
#define LE_ASSERT_MSG(expression, message) assert((expression) && (message))
#ifdef NDEBUG
#define LE_VERIFY(expression) ((void)(expression))
#define LE_VERIFY_MSG(expression, message) LE_VERIFY(expression)
#else
#define LE_VERIFY(expression) assert(expression)
#define LE_VERIFY_MSG(expression, message) LE_VERIFY((expression) && (message))
#endif // NDEBUG
#endif // BOOST_ENABLE_ASSERT_HANDLER

//------------------------------------------------------------------------------
} // namespace Utility
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // assert_hpp
