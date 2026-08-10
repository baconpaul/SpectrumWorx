////////////////////////////////////////////////////////////////////////////////
///
/// \file jucePath.hpp
/// ------------------
///
///   The boundary between `juce::File` and `fs::path`, and the only place in
/// `src/` that is allowed to name the former -- see tests/checkNoJuceFile.cmake,
/// which fails the build if it appears anywhere but here and the two
/// `juce::FileChooser` call sites that hand one back.
///
/// \note **Five conversions and nothing else.** The `fs` predicates are *not*
/// wrapped here. `fs::exists( p )` and friends throw on a failed syscall where
/// the `juce::File` ones returned false, so every call site takes the
/// `std::error_code` overload -- written out, in the init-`if` form
/// `presetStorage.cpp` and `presetBrowser.cpp` already use. Wrapping them would
/// have put a second dialect above `sw-dsp`, which cannot include this header at
/// all, for the sake of one line; and a wrapper named `exists` would have been
/// ambiguous with `fs::exists` by ADL at every unqualified call, `fs::path`
/// being a `std::filesystem` type.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef jucePath_hpp__79456284_25AE_41EF_B472_C426CB918D7F
#define jucePath_hpp__79456284_25AE_41EF_B472_C426CB918D7F
//------------------------------------------------------------------------------
#include <juce_core/juce_core.h>

// `fs`, and the -fno-char8_t that makes u8string() a std::string. See below.
#include "filesystem/import.h"

#include <string>
#include <string_view>
//------------------------------------------------------------------------------

namespace LE::IO
{

////////////////////////////////////////////////////////////////////////////////
///
/// \note **`fromUTF8()` and the wide route, never `String( char const * )`.**
/// That one word is the whole of issue #28, and the long note in gui.cpp has the
/// mechanism: sst-plugininfra's `filesystem` target carries `-fno-char8_t` in its
/// INTERFACE compile options, so `std::u8string` *is* `std::string`, the
/// `String( char8_t const * )` overload that would decode UTF-8 does not exist to
/// be chosen, and `juce::String( path.u8string().c_str() )` lands on the ASCII
/// constructor -- which widens every *byte* into its own code point.
///
///   That conversion used to sit in `rootPath()`, where it created a mojibake
/// Documents folder on a `ja_JP.UTF-8` desktop. `rootPath()` answers with an
/// `fs::path` now and does not convert at all; what is left of the hazard is
/// these five functions, which is why they have a test file to themselves.
///
/// \note **`wstring()` on Windows, not a cast off `u16string()`.** `fs::path`'s
/// native encoding *is* `std::wstring` there, so there is nothing to reinterpret;
/// the shortcircuit-xt and OB-Xf helpers this is modelled on both cast, because
/// they were written against a `fs` that might have been ghc's. And no
/// `fs::u8path()` anywhere: it is deprecated in C++20 and the wide route does not
/// need it.
///
////////////////////////////////////////////////////////////////////////////////

inline fs::path juceStringToPath(juce::String const &string)
{
#ifdef _WIN32
    return fs::path(string.toWideCharPointer());
#else
    return fs::path(std::string(string.toRawUTF8(), string.getNumBytesAsUTF8()));
#endif // _WIN32
}

inline fs::path juceFileToPath(juce::File const &file)
{
    return juceStringToPath(file.getFullPathName());
}

/// \brief UTF-8 bytes off the wire -- a preset's stored sample name -- as a path.
///
/// \note Not `fs::path( std::string )`, which decodes with the active code page
/// on Windows and would mangle every non-ASCII sample name a preset carries.
inline fs::path utf8ToPath(std::string_view const utf8)
{
#ifdef _WIN32
    return juceStringToPath(juce::String::fromUTF8(utf8.data(), static_cast<int>(utf8.size())));
#else
    return fs::path(std::string(utf8));
#endif // _WIN32
}

inline juce::String pathToJuceString(fs::path const &path)
{
#ifdef _WIN32
    return juce::String(path.wstring().c_str());
#else
    return juce::String::fromUTF8(path.u8string().c_str());
#endif // _WIN32
}

/// \note `juce::File` asserts on a relative path, so this is for something the
/// file system has already agreed exists. A bare factory sample name --
/// "Carrier.mp3", which `Sample` uses as a resource key rather than as a path --
/// must not reach it. \see Sample::factorySamples().
inline juce::File pathToJuceFile(fs::path const &path)
{
    return juce::File(pathToJuceString(path));
}

/// \brief The inverse of utf8ToPath(), for the preset format and for anything
/// else that wants a path as bytes.
///
/// \note A named function for `u8string()` because *which* of `path`'s five
/// string accessors is correct is exactly the thing that gets picked wrong:
/// `string()` is the active code page on Windows and silently right everywhere
/// else, which is how it survives review.
inline std::string pathToUTF8(fs::path const &path) { return path.u8string(); }

} // namespace LE::IO

//------------------------------------------------------------------------------
#endif // jucePath_hpp
