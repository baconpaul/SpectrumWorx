////////////////////////////////////////////////////////////////////////////////
///
/// \file lexicalCastTests.cpp
/// --------------------------
///
///   Number to string, and the one thing the function has to promise: that it
/// stays inside `RequiredStringStorage<T>::value` bytes, because that is the
/// only size any caller gives it. There is no capacity parameter -- the constant
/// *is* the contract -- so a value the format cannot fit is not a truncated
/// string, it is a write past the end of somebody's stack array.
///
///   `lexical_cast(double, decimalPlaces, char *)` bounded its `snprintf` to
/// that constant and then took the cursor for its trailing-zero trim from
/// snprintf's *return value*, which is the length it would have wanted. For
/// `%.1f` of 1e30 that is 33 against a buffer of 17: the trim then read
/// `buffer[32]` downwards and wrote its terminator there. Both callers in the
/// editor hand it a 32-byte stack array and then `strcpy` a suffix at the
/// returned offset, so the returned length is a second way out of the buffer.
///
/// \note The guard bytes are what make this a test rather than a hope. Without
/// a sanitizer an overrun of a stack array is silent and usually harmless, which
/// is exactly how it survived: `tech_debt.md` recorded the truncation as
/// "release truncates rather than overruns", and the overrun is in the trim
/// rather than in the snprintf.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "le/utility/lexicalCast.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
//------------------------------------------------------------------------------
namespace
{
//------------------------------------------------------------------------------

using LE::Utility::lexical_cast;
using LE::Utility::RequiredStringStorage;

constexpr unsigned capacity{RequiredStringStorage<double>::value};

////////////////////////////////////////////////////////////////////////////////
///
/// \class Guarded
///
/// \brief The buffer a caller is entitled to, with a wall of sentinel bytes
/// behind it that nothing is entitled to touch.
///
////////////////////////////////////////////////////////////////////////////////

class Guarded
{
  public:
    static constexpr char canary{'\x7E'};

    Guarded() { storage_.fill(canary); }

    char *begin() { return storage_.data(); }
    char const *begin() const { return storage_.data(); }

    /// Whether everything past what the caller owns is still untouched.
    bool intact() const
    {
        return std::all_of(storage_.begin() + capacity, storage_.end(),
                           [](char const byte) { return byte == canary; });
    }

    std::string text() const { return std::string(storage_.data()); }

  private:
    std::array<char, capacity + 64> storage_;
}; // class Guarded

/// \brief Renders \p value and checks every bound the function owes its caller.
/// \return what it wrote.
std::string renderedSafely(double const value, std::uint8_t const decimalPlaces)
{
    Guarded buffer;
    auto const written(lexical_cast(value, decimalPlaces, buffer.begin()));

    // Nothing past the buffer the caller gave.
    CHECK(buffer.intact());

    // ...and the length handed back is one a caller may index with, which is
    // what both editor call sites immediately do with strcpy().
    CHECK(written < capacity);
    CHECK(written == std::strlen(buffer.begin()));

    return buffer.text();
}

//------------------------------------------------------------------------------
} // anonymous namespace
//------------------------------------------------------------------------------

TEST_CASE("An ordinary value prints the way it always did", "[utility][lexical-cast]")
{
    // The trailing-zero trim, which is the whole reason this is not snprintf.
    CHECK(renderedSafely(1.5, 1) == "1.5");
    CHECK(renderedSafely(1.0, 1) == "1");
    CHECK(renderedSafely(1.0, 4) == "1");
    CHECK(renderedSafely(0.25, 2) == "0.25");
    CHECK(renderedSafely(-6.5, 1) == "-6.5");
    CHECK(renderedSafely(100.0, 0) == "100");
    CHECK(renderedSafely(22050.0, 1) == "22050");

    // A value that rounds to zero at the precision shown is zero, sign and all.
    CHECK(renderedSafely(-7e-15, 1) == "0");
}

TEST_CASE("A value too wide for the buffer stays inside it", "[utility][lexical-cast][hostile]")
{
    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \note `%.1f` of 1e30 wants 33 characters and the buffer is 17. This is
    /// the case the trim walked off the end of -- reading buffer[32] and writing
    /// a terminator there, in a release build, on a 32-byte stack array in the
    /// editor.
    ///
    ////////////////////////////////////////////////////////////////////////////
    for (auto const decimalPlaces :
         {std::uint8_t{0}, std::uint8_t{1}, std::uint8_t{2}, std::uint8_t{4}, std::uint8_t{9}})
    {
        INFO("decimal places: " << unsigned(decimalPlaces));

        for (auto const value : {1e30, -1e30, 1e17, 1e8, 1e300, -1e300})
        {
            INFO("value: " << value);
            auto const text(renderedSafely(value, decimalPlaces));

            // Whatever it decided to print, it must be a number and it must be
            // the right one -- a truncated "1000000000000000" for 1e30 is inside
            // the buffer and still a lie.
            CHECK(!text.empty());
            auto const readBack(lexical_cast<double>(text.c_str()));
            CHECK(std::abs(readBack - value) <= std::abs(value) * 1e-5);
        }
    }
}

TEST_CASE("The extremes of the type stay inside the buffer", "[utility][lexical-cast][hostile]")
{
    for (auto const decimalPlaces : {std::uint8_t{0}, std::uint8_t{1}, std::uint8_t{9}})
    {
        INFO("decimal places: " << unsigned(decimalPlaces));

        // Not checked for what they read back as -- infinity and NaN have no
        // round trip through this. Only that they do not leave the buffer.
        Guarded buffer;
        auto const written(
            lexical_cast(std::numeric_limits<double>::infinity(), decimalPlaces, buffer.begin()));
        CHECK(buffer.intact());
        CHECK(written < capacity);

        Guarded nan;
        auto const nanWritten(
            lexical_cast(std::numeric_limits<double>::quiet_NaN(), decimalPlaces, nan.begin()));
        CHECK(nan.intact());
        CHECK(nanWritten < capacity);

        CHECK(renderedSafely(std::numeric_limits<double>::denorm_min(), decimalPlaces).size() > 0);
    }
}
