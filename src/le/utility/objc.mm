////////////////////////////////////////////////////////////////////////////////
///
/// objc.mm
/// -------
///
/// Copyright (c) 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "objc.hpp"

#include "platformSpecifics.hpp"

#import <Foundation/Foundation.h>

#include <cstdint>

namespace LE::Utility::ObjC
{

namespace
{

NSString *wrapString(char const *__restrict const c_str, std::uint32_t const encoding)
{
    /// \note Using a CFString with 'toll-free bridging' as no equivalent
    /// method exists in NSString.
    ///                                   (18.04.2014.) (Domagoj Saric)
    ::CFStringRef const cfString(
        ::CFStringCreateWithCStringNoCopy(nullptr, c_str, encoding, kCFAllocatorNull));
    return (__bridge NSString *)cfString;
}

NSString *copyString(char const *__restrict const c_str, std::uint32_t const encoding)
{
    return [NSString stringWithCString:c_str encoding:encoding];
}
} // anonymous namespace

NSString *asciiString(char const *__restrict const c_str)
{
    return wrapString(c_str, kCFStringEncodingASCII);
}
NSString *utf8String(char const *__restrict const c_str)
{
    return wrapString(c_str, kCFStringEncodingUTF8);
}

NSString *copyASCIIString(char const *__restrict const c_str)
{
    return copyString(c_str, NSASCIIStringEncoding);
}
NSString *copyUTF8String(char const *__restrict const c_str)
{
    return copyString(c_str, NSUTF8StringEncoding);
}

} // namespace LE::Utility::ObjC
