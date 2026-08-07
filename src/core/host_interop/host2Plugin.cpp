////////////////////////////////////////////////////////////////////////////////
///
/// host2Plugin.cpp
/// ---------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "host2Plugin.hpp"

#include "le/utility/span.hpp"

namespace LE
{

#pragma warning(push)
#pragma warning(disable : 4127) // Conditional expression is constant.

//...mrmlj...orphan...
template <typename Char>
LE_COLD char *copyToBuffer(Char const *const string, LE::Utility::Span<char> const &buffer)
{
    //std::strncpy( buffer.begin(), string, buffer.size() - 1 );
    Char const *LE_RESTRICT pSourceCharacter(string);
    char *LE_RESTRICT pDestinationCharacter(buffer.begin());
    char const *LE_RESTRICT const pDestinationEnd(buffer.end() - 1);

    bool const same(
        !std::is_same<Char, char>::value &&
        (static_cast<void const *>(string) == static_cast<void const *>(pDestinationCharacter)));
    if (same)
    {
        LE_ASSERT(*pDestinationCharacter == '\0');
        return pDestinationCharacter;
    }
    if (string == nullptr)
    {
        *pDestinationCharacter = '\0';
        return pDestinationCharacter;
    }

    while (pDestinationCharacter != pDestinationEnd)
    {
        Char const sourceCharacter(*pSourceCharacter);
        if (sourceCharacter == '\0')
            break;
        *pDestinationCharacter = static_cast<char>(sourceCharacter);
        LE_ASSERT(*pDestinationCharacter == sourceCharacter);
        ++pSourceCharacter;
        ++pDestinationCharacter;
    }
    *pDestinationCharacter = '\0';

    return pDestinationCharacter;
}

#pragma warning(pop)

template char *copyToBuffer<char>(char const *, LE::Utility::Span<char> const &);
#ifdef _WIN32
template char *copyToBuffer<wchar_t>(wchar_t const *, LE::Utility::Span<char> const &);
#endif // _WIN32

namespace SW
{

} // namespace SW

} // namespace LE
