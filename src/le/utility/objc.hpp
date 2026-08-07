////////////////////////////////////////////////////////////////////////////////
///
/// \file objc.hpp
/// --------------
///
/// ObjC <-> C++ interop utilities.
///
/// Copyright (c) 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef objc_hpp__F939BC5B_C31E_4B2E_952D_83EBCE908813
#define objc_hpp__F939BC5B_C31E_4B2E_952D_83EBCE908813
#ifdef __OBJC__
//------------------------------------------------------------------------------
#include "abi.hpp"

@class NSString;

namespace LE::Utility::ObjC
{

NSString *asciiString(char const *c_str);
NSString *utf8String(char const *c_str);

NSString *copyASCIIString(char const *c_str);
NSString *copyUTF8String(char const *c_str);

} // namespace LE::Utility::ObjC

#endif // __OBJC__
#endif // objc_hpp
