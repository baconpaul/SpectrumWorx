////////////////////////////////////////////////////////////////////////////////
///
/// \file filesystemImpl.inl
/// ------------------------
///
/// Copyright (c) 2011 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef filesystemImpl_inl__C580EBB7_A92A_4E8C_BB8E_E660933A9583
#define filesystemImpl_inl__C580EBB7_A92A_4E8C_BB8E_E660933A9583
//------------------------------------------------------------------------------
#include "filesystemImpl.hpp"

#include "platformSpecifics.hpp"

#include "boost/mmap/mappble_objects/file/utility.hpp"
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace Utility
{
//------------------------------------------------------------------------------

LE_OPTIMIZE_FOR_SIZE_BEGIN()

template <SpecialLocations rootDirectory>
LE_NOTHROWNOALIAS typename File::Impl<rootDirectory>::type::MemoryMapping
File::map(char const *const relativeFilePath)
{
    return PathResolver<rootDirectory>::template apply<File::MemoryMapping>(
        relativeFilePath, [=](char const *const fullPath) {
            auto const mapping_reference(boost::mmap::map_read_only_file(fullPath));
            LE_TRACE_IF(!mapping_reference, "Failed to mmap file %s (errno: %d)", relativeFilePath,
                        errno);
            return File::MemoryMapping(
                File::MemoryMapping::Range(mapping_reference.begin(), mapping_reference.end()));
        });
}

template <SpecialLocations rootDirectory>
LE_NOTHROWNOALIAS typename File::Impl<rootDirectory>::type::Stream
File::open(char const *const relativeFilePath, unsigned int const flags)
{
    return PathResolver<rootDirectory>::template apply<File::Stream>(
        relativeFilePath, [=](char const *const fullPath) {
            return File::Stream(::open(fullPath, flags | binaryFlag, accessFlags));
        });
}

template LE_NOTHROWNOALIAS File::Impl<AbsolutePath>::type::MemoryMapping
File::map<AbsolutePath>(char const *);
template LE_NOTHROWNOALIAS File::Impl<AppData>::type::MemoryMapping
File::map<AppData>(char const *);
template LE_NOTHROWNOALIAS File::Impl<Documents>::type::MemoryMapping
File::map<Documents>(char const *);
template LE_NOTHROWNOALIAS File::Impl<Library>::type::MemoryMapping
File::map<Library>(char const *);
template LE_NOTHROWNOALIAS File::Impl<Resources>::type::MemoryMapping
File::map<Resources>(char const *);
template LE_NOTHROWNOALIAS File::Impl<ExternalStorage>::type::MemoryMapping
File::map<ExternalStorage>(char const *);
template LE_NOTHROWNOALIAS File::Impl<Temporaries>::type::MemoryMapping
File::map<Temporaries>(char const *);
#ifdef __ANDROID__
template LE_NOTHROWNOALIAS File::Impl<ToolOutput>::type::MemoryMapping
File::map<ToolOutput>(char const *);
#endif // __ANDROID__

template
    LE_NOTHROWNOALIAS File::Impl<AbsolutePath>::type::Stream File::open<AbsolutePath>(char const *,
                                                                                      unsigned int);
template LE_NOTHROWNOALIAS File::Impl<AppData>::type::Stream File::open<AppData>(char const *,
                                                                                 unsigned int);
template LE_NOTHROWNOALIAS File::Impl<Documents>::type::Stream File::open<Documents>(char const *,
                                                                                     unsigned int);
template LE_NOTHROWNOALIAS File::Impl<Library>::type::Stream File::open<Library>(char const *,
                                                                                 unsigned int);
template LE_NOTHROWNOALIAS File::Impl<Resources>::type::Stream File::open<Resources>(char const *,
                                                                                     unsigned int);
template LE_NOTHROWNOALIAS File::Impl<ExternalStorage>::type::Stream
File::open<ExternalStorage>(char const *, unsigned int);
template LE_NOTHROWNOALIAS File::Impl<Temporaries>::type::Stream
File::open<Temporaries>(char const *, unsigned int);
#ifdef __ANDROID__
template
    LE_NOTHROWNOALIAS File::Impl<ToolOutput>::type::Stream File::open<ToolOutput>(char const *,
                                                                                  unsigned int);
#endif // __ANDROID__

template <SpecialLocations location>
LE_NOTHROWNOALIAS char const *fullPath(char const *const relativePath)
{
    static Path path;
    PathResolver<location>::template apply<void>(
        relativePath, [](char const *const fullPath) { std::strcpy(&path[0], fullPath); });
    return &path[0];
}

template LE_NOTHROWNOALIAS char const *fullPath<AbsolutePath>(char const *);
template LE_NOTHROWNOALIAS char const *fullPath<AppData>(char const *);
template LE_NOTHROWNOALIAS char const *fullPath<Documents>(char const *);
template LE_NOTHROWNOALIAS char const *fullPath<Library>(char const *);
template LE_NOTHROWNOALIAS char const *fullPath<Resources>(char const *);
template LE_NOTHROWNOALIAS char const *fullPath<ExternalStorage>(char const *);
template LE_NOTHROWNOALIAS char const *fullPath<Temporaries>(char const *);
#ifdef __ANDROID__
template LE_NOTHROWNOALIAS char const *fullPath<ToolOutput>(char const *);
#endif // __ANDROID__

template <SpecialLocations location, bool writeAccess> LE_NOTHROWNOALIAS bool accessible()
{
#ifdef _MSC_VER
    static unsigned int const W_OK = 02;
    static unsigned int const R_OK = 04;
#endif // _MSC_VER
    return PathResolver<location>::template apply<bool>("", [](char const *const fullPath) {
        return ::access(fullPath, writeAccess ? R_OK | W_OK : R_OK) != -1;
    });
}

template LE_NOTHROWNOALIAS bool accessible<AbsolutePath, false>();
template LE_NOTHROWNOALIAS bool accessible<AbsolutePath, true>();
template LE_NOTHROWNOALIAS bool accessible<AppData, false>();
template LE_NOTHROWNOALIAS bool accessible<AppData, true>();
template LE_NOTHROWNOALIAS bool accessible<Documents, false>();
template LE_NOTHROWNOALIAS bool accessible<Documents, true>();
template LE_NOTHROWNOALIAS bool accessible<Library, false>();
template LE_NOTHROWNOALIAS bool accessible<Library, true>();
template LE_NOTHROWNOALIAS bool accessible<Resources, false>();
template LE_NOTHROWNOALIAS bool accessible<Resources, true>();
template LE_NOTHROWNOALIAS bool accessible<ExternalStorage, false>();
template LE_NOTHROWNOALIAS bool accessible<ExternalStorage, true>();
template LE_NOTHROWNOALIAS bool accessible<Temporaries, false>();
template LE_NOTHROWNOALIAS bool accessible<Temporaries, true>();

LE_OPTIMIZE_FOR_SIZE_END()

//------------------------------------------------------------------------------
} // namespace Utility
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // filesystemImpl_inl
