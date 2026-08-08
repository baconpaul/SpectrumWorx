////////////////////////////////////////////////////////////////////////////////
///
/// \file localeHarness.hpp
/// -----------------------
///
///   A host that has put the process into a comma-decimal locale, which is the
/// one piece of global state a plugin cannot set and has to survive.
///
/// \note Two globals, because there are two of them and a host may move either.
/// `std::setlocale` is what `snprintf` and `strtod` read; `std::locale::global`
/// is what a default-constructed stream is imbued with. Moving the C one alone
/// reproduces what the printing side did before it imbued the classic locale
/// explicitly, and what `strtod` still does without a locale argument.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef localeHarness_hpp__0C4E9B27_8A15_4D63_BF80_2E5A7C119D34
#define localeHarness_hpp__0C4E9B27_8A15_4D63_BF80_2E5A7C119D34
//------------------------------------------------------------------------------
#include <clocale>
#include <cstdio>
#include <cstring>
#include <locale>
#include <stdexcept>

namespace SWTest
{

////////////////////////////////////////////////////////////////////////////////
///
/// \class CommaDecimalHost
///
/// \brief The process' locale, for as long as one of these is alive.
///
/// \note Whether it took is asked rather than assumed -- a machine can have a
/// locale of that name whose numbers still use a point, and a test that quietly
/// ran in the C locale would pass for the wrong reason.
///
////////////////////////////////////////////////////////////////////////////////

class CommaDecimalHost
{
  public:
    CommaDecimalHost()
    {
        for (auto const *const name :
             {"de_DE.UTF-8", "de_DE", "fr_FR.UTF-8", "German_Germany.1252", "de-DE"})
        {
            if (!std::setlocale(LC_ALL, name))
                continue;

            try
            {
                std::locale::global(std::locale(name));
            }
            catch (std::runtime_error const &)
            {
                /// \note The C++ locale of that name need not exist where the C
                /// one does. The C half is the half that broke things.
            }

            char point[8]{};
            std::snprintf(point, sizeof(point), "%.1f", 1.5);
            if (std::strchr(point, ','))
            {
                installed_ = true;
                return;
            }
        }
    }

    ~CommaDecimalHost()
    {
        std::locale::global(std::locale::classic());
        std::setlocale(LC_ALL, "C");
    }

    CommaDecimalHost(CommaDecimalHost const &) = delete; // makes non-copyable

    /// Whether a comma-decimal locale was actually installed.
    explicit operator bool() const { return installed_; }

  private:
    bool installed_{false};
}; // class CommaDecimalHost

} // namespace SWTest

//------------------------------------------------------------------------------
#endif // localeHarness_hpp
