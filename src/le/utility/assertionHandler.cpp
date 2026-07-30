////////////////////////////////////////////////////////////////////////////////
///
/// assertionHandler.cpp
/// --------------------
///
/// Copyright (c) 2010 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
/// \note Was ( !NDEBUG || LE_PUBLIC_BUILD ) && LE_ENABLE_ASSERT_HANDLER, but
/// assert.hpp only declares assertionFailed() when the asserts themselves are
/// live, which NDEBUG now decides. A public build that wants its asserts back
/// asks for them with LE_CHECKED_BUILD=1.
///                                           (28.07.2026.) (SW port)
#if !defined(NDEBUG) && defined(LE_ENABLE_ASSERT_HANDLER)
//------------------------------------------------------------------------------
#include "platformSpecifics.hpp"
#include "tchar.hpp"
#include "trace.hpp"

#include "assert.hpp"

#if defined(__ANDROID__)
#include "android/log.h"
#elif defined(__APPLE__)
// http://stackoverflow.com/questions/1083541/built-in-preprocessor-token-to-detect-iphone-platform
#include "TargetConditionals.h"
#if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
#include "MacTypes.h"
#else
#include "CoreServices/CoreServices.h"
#endif
#include "signal.h"
#include "syslog.h"
#elif defined(_WIN32)
#include "windowsLite.hpp"
#endif

/// \note breakIntoDebugger()'s fallback arm below is `raise( SIGINT )`, and
/// `signal.h` was included only under `__APPLE__` — so every other POSIX target
/// took an arm whose declarations it had not seen. Debug-build-only, since a
/// release build has no assert handler to break from.
///                                           (29.07.2026.) (SW port)
#include <csignal>
#include <cstdio>
#include <cstdlib>
/// \note std::terminate lives here. Both uses of it are inside _WIN32 or
/// LE_PUBLIC_BUILD arms, so no other configuration compiles the line that needs
/// the declaration -- and libc++ hands it over transitively anyway, which is why
/// only MSVC ever asked.
///                                           (30.07.2026.) (SW port)
#include <exception>
#include <source_location>
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------

#pragma message("LEB: assertion handling enabled.")

LE_WEAK_SYMBOL_CONST char const assertionFailureMessageTitle[] = "LE SDK assertion failure";

#ifdef LEB_PRECOMPILE_JUCE
namespace SW
{
namespace GUI
{
bool warningOkCancelBox(TCHAR const *title, TCHAR const *question);
}
} // namespace SW
#endif // LEB_PRECOMPILE_JUCE

//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------

#pragma warning(push)
#pragma warning(disable : 4702) // Unreachable code.

namespace
{
static void printAssertionFailureTitle()
{
#if defined(__ANDROID__)
#elif defined(__APPLE__) && !(TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR)
    /// \note Was DebugStr(), deprecated since 10.8 and now gone. stderr is
    /// what a DAW log and a test runner both capture.
    ///                               (28.07.2026.) (SW port)
    std::fputs(LE::assertionFailureMessageTitle, stderr);
    std::fputc('\n', stderr);
#elif defined(_WIN32)
    ::OutputDebugStringA(LE::assertionFailureMessageTitle);
    std::fputs(LE::assertionFailureMessageTitle, stderr);
#else
    std::fputs(LE::assertionFailureMessageTitle, stderr);
#endif // platform/compiler
}

static void printDebugMessage(char const *const message)
{
#if defined(__ANDROID__)
// http://mobilepearls.com/labs/native-android-api/#logging
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-security"
    ::__android_log_assert("internal sanity check", LE::assertionFailureMessageTitle, message);
#pragma clang diagnostic pop
#else
    using namespace LE::Utility;
    auto const currentTag(Tracer::pTagString);
    Tracer::pTagString = LE::assertionFailureMessageTitle;
    Tracer::error(message);
    Tracer::pTagString = currentTag;
#endif // platform/compiler
}

#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable : 4505) // Unreferenced local function has been removed.
static void printDebugMessage(wchar_t const *const message)
{
    printAssertionFailureTitle();

    ::OutputDebugStringW(message);
    ::fputws(message, stderr);
}
#pragma warning(push)
#endif // _WIN32

#if defined(LEB_PRECOMPILE_JUCE) /*...mrmlj...*/ || defined(_WIN32)
#define LE_ASSERT_HAS_MSGBOX

static bool assertMessageBox(char const *const message)
{
#ifdef LEB_PRECOMPILE_JUCE
    return LE::SW::GUI::warningOkCancelBox(
        juce::String(LE::assertionFailureMessageTitle).getCharPointer().getAddress(),
        juce::String(message).getCharPointer().getAddress());
#else
    int const userChoice(
        ::MessageBoxA(nullptr, message, LE::assertionFailureMessageTitle, MB_ABORTRETRYIGNORE));
    switch (userChoice)
    {
    case IDRETRY:
        return false;
    case IDIGNORE:
        return true;
    default:
        std::terminate();
    }
#endif // LEB_PRECOMPILE_JUCE
}

#endif // LE_ASSERT_HAS_MSGBOX

void breakIntoDebugger()
{
#if defined(__ANDROID__)
#elif defined(_MSC_VER)
    _CrtDbgBreak();
#else
    // http://iphone.m20.nl/wp/2010/10/xcode-iphone-debugger-halt-assertions
    std::raise(SIGINT);
#endif
}

#pragma warning(push)
#pragma warning(                                                                                   \
    disable                                                                                        \
    : 4996) // 'itoa': The POSIX name for this item is deprecated. Instead, use the ISO C++ conformant name: _itoa.

static
    LE_NOINLINE void assertionFailedMsgAux([[maybe_unused]] char const *const expression,
                                           char const *const message,
                                           [[maybe_unused]] char const *const function,
                                           [[maybe_unused]] char const *const file, long const line)
{

    bool ignore;

    char fullMessage[4096] = {0};

#ifdef LE_PUBLIC_BUILD // not to leak too much information to beta testers...
    std::strcpy(fullMessage, message);
    std::strcat(fullMessage, " (");
#else
    if (message != expression)
    {
        std::strcpy(fullMessage, message);
        std::strcat(fullMessage, "\n");
    }
    std::strcat(fullMessage, "\n expression:\n  ");
    std::strcat(fullMessage, expression);
    std::strcat(fullMessage, "\n in function:\n  ");
    std::strcat(fullMessage, function);
    std::strcat(fullMessage, "\n in file:\n  ");
    std::strcat(fullMessage, file);
    std::strcat(fullMessage, "\n at line\n  ");
#endif // LE_PUBLIC_BUILD
#ifdef _MSC_VER
    /*std*/ ::itoa(line, &fullMessage[std::strlen(fullMessage)], 10);
#else
    std::sprintf(&fullMessage[std::strlen(fullMessage)], "%ld", line);
#endif // _MSC_VER
#ifdef LE_PUBLIC_BUILD
    std::strcat(fullMessage, ")");
#endif // LE_PUBLIC_BUILD

    printDebugMessage(fullMessage);

#ifdef LE_ASSERT_HAS_MSGBOX
    ignore = assertMessageBox(fullMessage);
#else
    ignore = false;
#endif // LE_ASSERT_HAS_MSGBOX

    if (!ignore)
    {
        breakIntoDebugger();

        // Do not forcibly terminate in internal debug builds...
#if defined(LE_PUBLIC_BUILD) && !defined(__ANDROID__)
        std::terminate();
#endif // LE_PUBLIC_BUILD
    }
}

#pragma warning(pop)
} // anonymous namespace

/// \note This used to define boost::assertion_failed and
/// boost::assertion_failed_msg, the Boost.Assert hook. Stage 2 pointed
/// LE_ASSERT at LE::Utility::assertionFailed and left the handler behind, so
/// nothing had defined it since.
///                                           (28.07.2026.) (SW port)
namespace LE
{
namespace Utility
{
LE_WEAK_FUNCTION void assertionFailed(char const *const expression, char const *const message,
                                      std::source_location const &location)
{
#ifdef LE_PUBLIC_BUILD // not to leak too much information to beta testers...
    assertionFailedMsgAux(nullptr, message, nullptr, nullptr, location.line());
#else
    assertionFailedMsgAux(expression, message, location.function_name(), location.file_name(),
                          location.line());
#endif // LE_PUBLIC_BUILD
}
} // namespace Utility
} // namespace LE

#pragma warning(pop)

#endif // ( !NDEBUG || LE_PUBLIC_BUILD ) && LE_ENABLE_ASSERT_HANDLER
