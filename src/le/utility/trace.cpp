////////////////////////////////////////////////////////////////////////////////
///
/// trace.cpp
/// ---------
///
/// Copyright (c) 2013 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "trace.hpp"

#include "abi.hpp"
#include "platformSpecifics.hpp"

#ifdef __APPLE__
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
#import <Foundation/Foundation.h>
#else
#include <MacTypes.h>
#include <mach-o/dyld.h>
#include <sys/syslog.h>
#endif // iOS
#endif // __APPLE__

#ifdef _WIN32
extern "C" __declspec(dllimport) void __stdcall OutputDebugStringA(char const *lpOutputString);
#endif // _WIN32

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace LE::Utility
{

LE_WEAK_SYMBOL char const *Tracer::pTagString = "LE";

namespace
{
#if TARGET_OS_IPHONE
void (^userMessageCallback)(NSString *message) = nullptr;

LE_COLD void iOSLog(char const *const pFormatString, va_list /*const*/ arglist)
{
    // https://developer.apple.com/library/ios/documentation/Cocoa/Conceptual/MemoryMgmt/Articles/mmAutoreleasePools.html
    @autoreleasepool
    {
        //@try
        {
            auto const formatString(
                [NSString stringWithCString:pFormatString encoding:NSASCIIStringEncoding]);
            auto const formattedString(
                [[[NSString alloc] initWithFormat:formatString arguments:arglist] autorelease]);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-security"
            ::NSLog(formattedString);
#pragma clang diagnostic pop
            if (userMessageCallback)
                userMessageCallback(formattedString);
        }
        //@catch ( ::NSException * ) {}
    }
}
#endif // target
} // anonymous namespace

#if TARGET_OS_IPHONE
void *Tracer_setUserMessageMethod(void *const opaquePtr)
{
    auto const previous(userMessageCallback);
    userMessageCallback = (decltype(userMessageCallback))(opaquePtr);
    return previous;
}
#else
void *Tracer_setUserMessageMethod(void * /*const opaquePtr*/) { return nullptr; }
#endif // OS

void Tracer::error(char const *const pFormatString, ...)
{
    va_list arglist;
    va_start(arglist, pFormatString);
#if (TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR)
    iOSLog(pFormatString, arglist);
#else
    auto const bufferSize(1024);
    char formattedError[bufferSize];
    char *pFormattedError(&formattedError[1]);
    std::strcpy(pFormattedError, pTagString);
    pFormattedError += std::strlen(pFormattedError);
    std::strcpy(pFormattedError, ", ERROR: ");
    pFormattedError += std::strlen(pFormattedError);
    unsigned int const tagCharacters(
        static_cast<unsigned int>(pFormattedError - (formattedError + 1)));
    int const charactersWritten(
        /*std*/ ::vsnprintf(pFormattedError, bufferSize - tagCharacters, pFormatString, arglist));
    LE_ASSUME(charactersWritten > 0 || (charactersWritten == 0 && *pFormatString == '\0'));
#if defined(__APPLE__)
// https://developer.apple.com/library/mac/documentation/Darwin/Reference/ManPages/man3/syslog.3.html
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-security"
    ::syslog(LOG_ERR, &formattedError[1]);
#pragma clang diagnostic pop
    /// \note Was DebugStr(), which took the Pascal string formattedError[0]
    /// was being length-prefixed for. Carbon is gone; stderr is what a DAW
    /// log and a test runner both capture.
    ///                                   (28.07.2026.) (SW port)
    std::fputs(&formattedError[1], stderr);
    std::fputs("\n", stderr);
#elif defined(_WIN32)
    std::fputs(&formattedError[1], stderr);
    pFormattedError[charactersWritten + 0] = '\n';
    pFormattedError[charactersWritten + 1] = '\0';
    ::OutputDebugStringA(&formattedError[1]);
#else
    /// \note The #else this replaces was Windows-only in everything but its
    /// spelling, so Linux reached OutputDebugStringA. There is no
    /// debugger-attached log channel to mirror to here: stderr is what a DAW
    /// log and a test runner both capture.
    ///                                   (29.07.2026.) (SW port)
    std::fputs(&formattedError[1], stderr);
    std::fputs("\n", stderr);
#endif
#endif // platform/compiler
    va_end(arglist);
}

void Tracer::message(char const *const pFormatString, ...)
{
    va_list arglist;
    va_start(arglist, pFormatString);
#if (TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR)
    iOSLog(pFormatString, arglist);
#else
    unsigned int const bufferSize(1024);
    char formattedMessage[bufferSize];
    int const charactersWritten(
        /*std*/ ::vsnprintf(formattedMessage, bufferSize, pFormatString, arglist));
    LE_ASSUME(charactersWritten > 0);
    std::puts(formattedMessage);
#if defined(__APPLE__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-security"
    va_start(arglist, pFormatString);
    ::vsyslog(LOG_INFO, pFormatString, arglist);
#pragma clang diagnostic pop
#elif defined(_WIN32)
    formattedMessage[charactersWritten + 0] = '\n';
    formattedMessage[charactersWritten + 1] = '\0';
    ::OutputDebugStringA(formattedMessage);
#endif // the std::puts above is the whole of it on a plain POSIX target
#endif // platform/compiler
    va_end(arglist);
}

#if TARGET_OS_IPHONE
bool Tracer::setObjCCallback(void (^newCallback)(NSString *message))
{
    Block_release(userMessageCallback);
    userMessageCallback = Block_copy(newCallback);
    return userMessageCallback != nullptr;
}
#endif // os

} // namespace LE::Utility
