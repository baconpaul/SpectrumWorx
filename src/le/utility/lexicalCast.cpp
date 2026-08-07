////////////////////////////////////////////////////////////////////////////////
///
/// lexicalCast.cpp
/// ---------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "lexicalCast.hpp"

#include "platformSpecifics.hpp"

// Implementation note:
//   On *NIX platforms we link dynamically with the CRT so we just use the
// sprintf function.
//                                        (12.12.2011.) (Domagoj Saric)
//   MSVC builds used to route the float and int conversions through
// Boost.Spirit's karma/qi instead, to keep the statically linked CRT's printf
// out of the binary. That was the last Boost dependency in this file, so both
// platforms now take the CRT path; if the size ever matters again, hand roll it
// rather than bringing Spirit back.
//                                        (28.07.2026.) (SW port)

#include <cstdio>
#ifndef NDEBUG
#include <cctype>
#include <cmath>
#endif // NDEBUG
#include <cstring>

namespace LE::Utility
{

LE_OPTIMIZE_FOR_SIZE_BEGIN()

// http://code.google.com/p/stringencoders/source/browse/trunk/src/modp_numtoa.c
// http://www.dreamincode.net/code/snippet2482.htm
// http://www.piumarta.com/software/fcvt

/// \note The buffer is a bare pointer, so the bound snprintf gets is the one the
/// interface asks its callers for: RequiredStringStorage<T>::value. The asserts
/// are what says so -- snprintf returns the length it *wanted*, so a buffer the
/// constant sizes too small shows up as a truncation in a debug build rather
/// than as an overrun in a release one.
///                                           (02.08.2026.) (SW port)
LE_COLD unsigned int lexical_cast(std::int32_t const value, char *const buffer)
{
    auto const charactersWritten(
        std::snprintf(buffer, RequiredStringStorage<std::int32_t>::value, "%d", value));
    LE_ASSERT(charactersWritten < RequiredStringStorage<std::int32_t>::value);
    return static_cast<unsigned int>(charactersWritten);
}
LE_COLD unsigned int lexical_cast(long const value, char *const buffer)
{
    return lexical_cast(static_cast<std::int32_t>(value), buffer);
}
LE_COLD unsigned int lexical_cast(std::uint32_t const value, char *const buffer)
{
    auto const charactersWritten(
        std::snprintf(buffer, RequiredStringStorage<std::uint32_t>::value, "%u", value));
    LE_ASSERT(charactersWritten < RequiredStringStorage<std::uint32_t>::value);
    return static_cast<unsigned int>(charactersWritten);
}
LE_COLD unsigned int lexical_cast(unsigned long const value, char *const buffer)
{
    return lexical_cast(static_cast<std::uint32_t>(value), buffer);
}

LE_COLD unsigned int lexical_cast(float const value, char *const buffer)
{
    return lexical_cast(value, 4, buffer);
}
LE_COLD unsigned int lexical_cast(double const value, char *const buffer)
{
    return lexical_cast(value, 9, buffer);
}
LE_COLD unsigned int lexical_cast(float const value, std::uint8_t const decimalPlaces,
                                  char *const buffer)
{
    return lexical_cast(static_cast<double>(value), decimalPlaces, buffer);
}
LE_COLD LE_NOINLINE unsigned int lexical_cast(double const value, std::uint8_t const decimalPlaces,
                                              char *const buffer)
{
    char const format[] = {'%', '.', static_cast<char>('0' + decimalPlaces), 'f', '\0'};
    auto const charactersWanted(
        std::snprintf(buffer, RequiredStringStorage<double>::value, format, value));
    LE_ASSERT(charactersWanted < RequiredStringStorage<double>::value);
    unsigned int totalCharactersWritten(static_cast<unsigned int>(charactersWanted));
    if (decimalPlaces)
    {
        /// \note Trim trailing zeros.
        ///                                   (15.12.2011.) (Domagoj Saric)
        char *pEnd(buffer + totalCharactersWritten);
        char const *const pDot(pEnd - decimalPlaces - 1);
        LE_ASSERT(*pEnd == '\0');
        LE_ASSERT(*pDot == '.' || !std::isfinite(value));
        while ((pEnd != pDot) && (*--pEnd == '0'))
        {
        }
        pEnd += (pEnd != pDot);
        LE_ASSERT(*pEnd == '0' || *pEnd == '.' || *pEnd == '\0');
        *pEnd = '\0';
        totalCharactersWritten = static_cast<unsigned int>(pEnd - buffer);
    }
    else
    {
        LE_ASSERT(std::isalnum(buffer[totalCharactersWritten - 1]));
    }
    LE_ASSERT_MSG((std::abs(lexical_cast<float>(buffer) - static_cast<float>(value)) <
                   (1 / std::pow(10.0f, decimalPlaces))) ||
                      !std::isfinite(value),
                  "Zero trimming broken.");
    return totalCharactersWritten;
}

template <> LE_COLD bool lexical_cast<bool>(char const *const valueString)
{
    LE_ASSERT(valueString[0] == '0' || valueString[0] == '1');
    LE_ASSERT(valueString[1] == '\0' || valueString[1] == '"' || valueString[1] == '<');
    std::uint8_t const value(valueString[0] - '0');
    LE_ASSUME((value == 0) || (value == 1));
    return reinterpret_cast<bool const &>(value);
}

template <> LE_COLD int lexical_cast<int>(char const *valueString)
{
    return std::atoi(valueString);
}

template <> LE_COLD long lexical_cast<long>(char const *const valueString)
{
    return lexical_cast<int>(valueString);
}
template <> LE_COLD unsigned int lexical_cast<unsigned int>(char const *const valueString)
{
    return lexical_cast<int>(valueString);
}

namespace
{
double lexical_cast_double_worker(char const *&pValueString);

float lexical_cast_float_worker(char const *&pValueString)
{
    return static_cast<float>(lexical_cast_double_worker(pValueString));
}

double lexical_cast_double_worker(char const *&pValueString)
{
    char *pEnd;
    double const result(std::strtod(pValueString, &pEnd));
    pValueString = pEnd;
    return result;
}
} // namespace

template <> LE_COLD float lexical_cast<float>(char const *valueString)
{
    return lexical_cast_float_worker(valueString);
}
template <> LE_COLD double lexical_cast<double>(char const *valueString)
{
    return lexical_cast_double_worker(valueString);
}

LE_OPTIMIZE_FOR_SIZE_END()

} // namespace LE::Utility
