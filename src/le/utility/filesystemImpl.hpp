////////////////////////////////////////////////////////////////////////////////
///
/// \file filesystemImpl.hpp
/// ------------------------
///
/// Copyright (c) 2011 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef filesystemImpl_hpp__33637349_32C0_4924_9A7A_6DF8307C41EF
#define filesystemImpl_hpp__33637349_32C0_4924_9A7A_6DF8307C41EF
//------------------------------------------------------------------------------
#include "filesystem.hpp"
#include "platformSpecifics.hpp"

#include <memory>

#include <fcntl.h>
#include <sys/stat.h>
#ifdef _MSC_VER
#pragma warning(                                                                                   \
    disable                                                                                        \
    : 4996) // '<...>': The POSIX name for this item is deprecated. Instead, use the ISO C++ conformant name: _<...>
#include <direct.h>
#include <io.h>
int const binaryFlag(_O_BINARY | _O_SEQUENTIAL | _O_NOINHERIT);
int const accessFlags(_S_IREAD | _S_IWRITE);
#else // POSIX implementation
#include <unistd.h>
#include <sys/resource.h>
#include <sys/time.h>
int const binaryFlag(0);
int const accessFlags(S_IRUSR | S_IWUSR | S_IROTH | S_IWOTH | S_IRGRP | S_IWGRP);
#endif // POSIX implementation

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace Utility
{
//------------------------------------------------------------------------------

using CString = std::unique_ptr<char[]>;

#ifdef _WIN32
using Path = std::array<char, 260 /*MAX_PATH*/>;
#else
using Path = std::array<char, 2048 /*PATH_MAX*/>;
#endif

template <SpecialLocations rootLocation> struct PathResolver
{
    template <class Result, class Functor> static Result apply(char const *relativePath, Functor);
};

namespace Detail
{
/// \note Was boost::mmap::map_read_only_file()/mapped_view_reference::unmap(),
/// from a Boost sandbox library that was never released. Two calls in total, so
/// they are just mmap()/munmap() (and the Win32 pair) now.
///                                       (28.07.2026.) (SW port)
File::MemoryMapping::Range mapReadOnlyFile(char const *fullPath);
void unmapFile(File::MemoryMapping::Range const &);
} // namespace Detail

template <>
template <class Result, class Functor>
LE_FORCEINLINE Result PathResolver<AbsolutePath>::apply(char const *const absolutePath,
                                                        Functor const f)
{
    return f(absolutePath);
}

//------------------------------------------------------------------------------
} // namespace Utility
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // filesystemImpl_hpp
