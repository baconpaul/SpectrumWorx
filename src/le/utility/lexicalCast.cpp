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

#include <cmath>
#include <cstdio>
#include <cstdlib>
#ifndef NDEBUG
#include <cctype>
#endif // NDEBUG
#include <cstring>

namespace LE::Utility
{

// http://code.google.com/p/stringencoders/source/browse/trunk/src/modp_numtoa.c
// http://www.dreamincode.net/code/snippet2482.htm
// http://www.piumarta.com/software/fcvt

/// \note The buffer is a bare pointer, so the bound snprintf gets is the one the
/// interface asks its callers for: RequiredStringStorage<T>::value. The asserts
/// are what says so -- snprintf returns the length it *wanted*, so a buffer the
/// constant sizes too small shows up as a truncation in a debug build rather
/// than as an overrun in a release one.
///                                           (02.08.2026.) (SW port)
unsigned int lexical_cast(std::int32_t const value, char *const buffer)
{
    auto const charactersWritten(
        std::snprintf(buffer, RequiredStringStorage<std::int32_t>::value, "%d", value));
    LE_ASSERT(charactersWritten < RequiredStringStorage<std::int32_t>::value);
    return static_cast<unsigned int>(charactersWritten);
}
unsigned int lexical_cast(long const value, char *const buffer)
{
    return lexical_cast(static_cast<std::int32_t>(value), buffer);
}
unsigned int lexical_cast(std::uint32_t const value, char *const buffer)
{
    auto const charactersWritten(
        std::snprintf(buffer, RequiredStringStorage<std::uint32_t>::value, "%u", value));
    LE_ASSERT(charactersWritten < RequiredStringStorage<std::uint32_t>::value);
    return static_cast<unsigned int>(charactersWritten);
}
unsigned int lexical_cast(unsigned long const value, char *const buffer)
{
    return lexical_cast(static_cast<std::uint32_t>(value), buffer);
}

unsigned int lexical_cast(float const value, char *const buffer)
{
    return lexical_cast(value, 4, buffer);
}
unsigned int lexical_cast(double const value, char *const buffer)
{
    return lexical_cast(value, 9, buffer);
}
unsigned int lexical_cast(float const value, std::uint8_t const decimalPlaces, char *const buffer)
{
    return lexical_cast(static_cast<double>(value), decimalPlaces, buffer);
}
////////////////////////////////////////////////////////////////////////////////
///
/// \note Rendered into a scratch buffer of its own and copied back, rather than
/// straight into the caller's.
///
///   `%f` has no bound related to `RequiredStringStorage<double>` -- that
/// constant is sized for a mantissa's worth of significant digits, seventeen,
/// and `%.1f` of 1e30 wants thirty-three. The snprintf was bounded and so
/// truncated safely, but the trailing-zero trim below took its cursor from
/// snprintf's *return*, which is the length it wanted rather than the length it
/// wrote. It then walked down from `buffer[32]` of a seventeen-byte buffer and
/// wrote its terminator there, and handed the same out-of-range length back to a
/// caller -- both editor call sites `strcpy` a suffix at exactly that offset,
/// into a 32-byte stack array.
///
///   So the trim now runs over a rendering that cannot have been truncated, and
/// the caller sees a result only once it is known to fit. A value too wide for
/// the constant at this precision is printed with `%g` instead: fewer
/// significant digits than were asked for, but the right number, inside the
/// buffer, and something `strtod` reads back. Truncating `%f` would have been
/// none of those -- 1e30 would read "1000000000000000".
///
/// \note This refutes the tech_debt.md entry that had it as "release truncates
/// rather than overruns". The truncation was never the problem.
///                                           (08.08.2026.) (SW port)
///
////////////////////////////////////////////////////////////////////////////////

LE_NOINLINE unsigned int lexical_cast(double const value, std::uint8_t const decimalPlaces,
                                      char *const buffer)
{
    static constexpr auto capacity{RequiredStringStorage<double>::value};

    /// Enough for `%f` of any double: DBL_MAX is 309 integer digits, and the
    /// sign, the point and the decimals are bounded by the assert below.
    static constexpr unsigned int scratchSize{344};
    char scratch[scratchSize];

    LE_ASSERT_MSG(decimalPlaces <= 9, "One digit is all the format below has room for.");
    char const format[] = {'%', '.', static_cast<char>('0' + decimalPlaces), 'f', '\0'};

    auto const charactersWanted(std::snprintf(scratch, scratchSize, format, value));
    LE_ASSERT(charactersWanted > 0);
    LE_ASSERT(static_cast<unsigned int>(charactersWanted) < scratchSize);
    unsigned int totalCharactersWritten(static_cast<unsigned int>(charactersWanted));

    /// \note `length > decimalPlaces` and not merely `decimalPlaces`: infinity
    /// and NaN print as three characters whatever the precision asked for, and
    /// there is no point in them to trim back to.
    if (decimalPlaces && (totalCharactersWritten > decimalPlaces))
    {
        /// \note Trim trailing zeros.
        ///                                   (15.12.2011.) (Domagoj Saric)
        char *pEnd(scratch + totalCharactersWritten);
        char const *const pDot(pEnd - decimalPlaces - 1);
        LE_ASSERT(*pEnd == '\0');
        LE_ASSERT(*pDot == '.' || !std::isfinite(value));
        while ((pEnd != pDot) && (*--pEnd == '0'))
        {
        }
        pEnd += (pEnd != pDot);
        LE_ASSERT(*pEnd == '0' || *pEnd == '.' || *pEnd == '\0');
        *pEnd = '\0';
        totalCharactersWritten = static_cast<unsigned int>(pEnd - scratch);
    }
    else
    {
        LE_ASSERT(std::isalnum(scratch[totalCharactersWritten - 1]));
    }

    if (totalCharactersWritten < capacity)
    {
        std::memcpy(buffer, scratch, totalCharactersWritten + 1);
    }
    else
    {
        auto const compactCharacters(std::snprintf(buffer, capacity, "%g", value));
        LE_ASSERT(compactCharacters > 0);
        /// Six significant digits and a three-digit exponent is thirteen
        /// characters at the widest, against a constant of seventeen.
        LE_ASSERT(static_cast<unsigned int>(compactCharacters) < capacity);
        totalCharactersWritten = static_cast<unsigned int>(compactCharacters);
    }

    ////////////////////////////////////////////////////////////////////////////
    /// \note And "-0" is "0". A value that rounds to zero at the precision being
    /// shown *is* zero as far as this string is concerned, and the sign in front
    /// of it is noise a user has to interpret: a module gain sitting at exactly
    /// 0 dB reads as "-0 dB" the moment a host normalises the value and writes it
    /// back, because -48 + (48/72) * 72 is -7e-15 rather than 0.
    ///
    ///   Here rather than at that subtraction, because the subtraction is not
    /// wrong -- it is as close to zero as a float gets -- and because every other
    /// route to a tiny negative lands here too.
    ///                                       (07.08.2026.) (SW port)
    ////////////////////////////////////////////////////////////////////////////
    if (buffer[0] == '-')
    {
        char const *pCharacter(buffer + 1);
        while ((*pCharacter == '0') || (*pCharacter == '.'))
            ++pCharacter;
        if (*pCharacter == '\0')
        {
            std::memmove(buffer, buffer + 1, totalCharactersWritten); // the terminator too
            --totalCharactersWritten;
        }
    }

    /// \note In double rather than float, and with a relative tolerance beside
    /// the absolute one. Read back as a float this compared `inf` against `inf`
    /// for any value over FLT_MAX -- a NaN, so the check was vacuously false and
    /// only the `isfinite` arm was holding it up. The relative arm is what the
    /// `%g` fallback above needs: six significant digits is all it promises.
    ///                                       (08.08.2026.) (SW port)
    [[maybe_unused]] auto const readBack(lexical_cast<double>(buffer));
    LE_ASSERT_MSG(!std::isfinite(value) ||
                      (std::abs(readBack - value) < (1 / std::pow(10.0, decimalPlaces))) ||
                      (std::abs(readBack - value) <= std::abs(value) * 1e-5),
                  "Zero trimming broken.");
    return totalCharactersWritten;
}

template <> bool lexical_cast<bool>(char const *const valueString)
{
    LE_ASSERT(valueString[0] == '0' || valueString[0] == '1');
    LE_ASSERT(valueString[1] == '\0' || valueString[1] == '"' || valueString[1] == '<');
    std::uint8_t const value(valueString[0] - '0');
    LE_ASSUME((value == 0) || (value == 1));
    return reinterpret_cast<bool const &>(value);
}

template <> int lexical_cast<int>(char const *valueString) { return std::atoi(valueString); }

template <> long lexical_cast<long>(char const *const valueString)
{
    return lexical_cast<int>(valueString);
}
template <> unsigned int lexical_cast<unsigned int>(char const *const valueString)
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

template <> float lexical_cast<float>(char const *valueString)
{
    return lexical_cast_float_worker(valueString);
}
template <> double lexical_cast<double>(char const *valueString)
{
    return lexical_cast_double_worker(valueString);
}

std::optional<double> parseNumber(char const *const text)
{
    if (!text)
        return {};

    char *pEnd;
    double const value(std::strtod(text, &pEnd));

    /// \note The two answers the lexical_cast<> above cannot give. `strtod`
    /// leaves pEnd where it started when it read nothing at all, which is the
    /// only way to tell "" and "off" from "0"; and it happily reads "nan", which
    /// is a value nothing downstream may be handed.
    if (pEnd == text)
        return {};
    if (std::isnan(value))
        return {};

    return value;
}

} // namespace LE::Utility
