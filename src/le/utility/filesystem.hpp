////////////////////////////////////////////////////////////////////////////////
///
/// \file filesystem.hpp
/// --------------------
///
/// Copyright (c) 2012 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef filesystem_hpp__656E7522_88F0_4EA5_9E7D_3944AC55FE77
#define filesystem_hpp__656E7522_88F0_4EA5_9E7D_3944AC55FE77
//------------------------------------------------------------------------------
#include "abi.hpp"
#include "span.hpp"

#include <sys/types.h>

#include <fcntl.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <utility>
//------------------------------------------------------------------------------
#ifdef __ANDROID__
#include <jni.h>
struct AAssetManager;
struct ANativeActivity;
namespace ndk_helper
{
class JNIHelper;
}
#endif // __ANDROID__
//------------------------------------------------------------------------------
#if defined(_MSC_VER) && !defined(LE_SDK_NO_AUTO_LINK)
#ifdef _WIN64
#pragma comment(lib, "LE_Utility_Win64_x86-64_SSE3.lib")
#else // _WIN32
#pragma comment(lib, "LE_Utility_Win32_x86-32_SSE2.lib")
#endif // _WIN32/64
#endif // _MSC_VER && !LE_SDK_NO_AUTO_LINK
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
/// \addtogroup Utility
/// \brief Shared, utility code used by LE SDKs and example apps.
/// \details Not to be confused with the documented API of the accompanying LE
/// SDK(s).
/// @{
//------------------------------------------------------------------------------
namespace Utility /// \brief Root namespace for all the shared LE thingies
{
//------------------------------------------------------------------------------

/// \brief Common file locations used by most OS UIs/shells to group user data.
/// \details
/// Certain OSs offer different ways/APIs to access some of the predefined
/// locations (e.g. some locations are not even classic filesystem directories
/// but rather compressed directories within resources archives).
/// For this reason, LE SDK classes that require file access provide function
/// templates that can be specialized with the values of this enum to select the
/// desired root location thereby offering a portable solution to the underlying
/// problem.
/// <BR><B>Android specific:</B> Using any location other than AbsolutePath or
/// CWD requires a prior call to setAppContext().

enum SpecialLocations
{
    AbsolutePath, ///< Interpret a given path as an absolute path (i.e. pass it unchanged to the underlying function)
    AppData, ///< <B>Android:</B> ANativeActivity::internalDataPath / (Java) Activity::getFilesDir()<BR>
             ///< <B>iOS:</B> NSHomeDirectory()<BR>
             ///< <B>OSX:</B> [[NSBundle mainBundle] bundlePath]<BR>
             ///< <B>Windows:</B> FOLDERID_ProgramData<BR>
    CWD ///< Current Working Directory (in effect equivalent to <VAR>AbsolutePath</VAR> on all platforms)
    = AbsolutePath,
    Resources ///< <B>Android:</B> APK assets<BR>
              ///< <B>iOS&OSX:</B> [[NSBundle mainBundle] resourcePath]<BR>
              ///< <B>Windows:</B> same as AppData<BR>
#ifdef DOXYGEN_ONLY
    ,
#else
    = AppData + 1,
#endif

    ExternalStorage, ///< <B>Android:</B> ANativeActivity::externalDataPath / (Java) Environment.getExternalStorageAppFilesDirectory()<BR>
                     ///< <B>iOS&OSX:</B> N/A<BR>
                     ///< <B>Windows:</B> N/A<BR>
    Temporaries,     ///< <B>Android:</B> (Java) Activity::getCacheDir()<BR>
                     ///< <B>iOS:</B> <VAR>AppData</VAR>/tmp<BR>
                     ///< <B>OSX:</B> /var/tmp<BR>
                     ///< <B>Windows:</B> GetTempPath()<BR>
    Documents,       ///< <B>Android:</B> same as AppData<BR>
                     ///< <B>iOS&OSX:</B> NSDocumentDirectory<BR>
                     ///< <B>Windows:</B> FOLDERID_Documents<BR>
    SharedDocuments  ///< <B>Android:</B> same as ExternalStorage<BR>
                     ///< <B>iOS&OSX:</B> NSHomeDirectory<BR>
                     ///< <B>Windows:</B> FOLDERID_PublicDocuments<BR>
#if defined(DOXYGEN_ONLY)
    ,
#elif defined(__ANDROID__)
    = ExternalStorage
#endif
    ,
    Library ///< <B>Android:</B> same as AppData<BR>
            ///< <B>iOS&OSX:</B> NSLibraryDirectory<BR>
            ///< <B>Windows:</B> FOLDERID_Libraries<BR>
#if defined(DOXYGEN_ONLY)
    ,
#elif defined(__ANDROID__)
    = Documents + 1
#endif
    ,
    ToolResources ///< A convenience/special version of the "Resources"
                  ///< location intended for simple apps/'tools' w/o an
                  ///< installer running in non sandboxed environments that
                  ///< expect their 'resources' to reside in the CWD.<BR>
                  ///< <B>Android&iOS:</B> same as Resources<BR>
                  ///< <B>OSX&Windows:</B> same as CWD<BR>
#if defined(DOXYGEN_ONLY)
    ,
#elif defined(__ANDROID__) || defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__)
    = Resources,
#else          // Windows & OSX
    = CWD,
#endif         // OS
    ToolOutput ///< A similar convenience location like ToolResources only
               ///< for output files.<BR>
///< <B>Android:</B> same as ExternalStorage if accessible, otherwise same as Documents<BR>
///< <B>iOS:</B> same as Documents<BR>
///< <B>OSX&Windows:</B> same as CWD<BR>
#if defined(DOXYGEN_ONLY)
    ,
#elif defined(__ANDROID__)
    = Library + 1,
#elif defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__)
    = Documents,
#else  // Windows & OSX
    = CWD,
#endif // OS
}; // enum SpecialLocation

#if defined(__ANDROID__) || defined(DOXYGEN_ONLY)
////////////////////////////////////////////////////////////////////////////////
///
/// \class ResourceFile
///
////////////////////////////////////////////////////////////////////////////////

class ResourceFile
{
  public:
    class MemoryMapping;
    class Stream;

    static LE_NOTHROWNOALIAS MemoryMapping map(char const *relativeFilePath);
    static LE_NOTHROWNOALIAS Stream open(char const *relativeFilePath);
}; // class ResourceFile
#endif // __ANDROID__

////////////////////////////////////////////////////////////////////////////////
///
/// \class File
///
////////////////////////////////////////////////////////////////////////////////

class File
{
  public:
    template <SpecialLocations> struct Impl
    {
        typedef File type;
    };

  public:
    class MemoryMapping;
    class Stream;

    template <SpecialLocations location>
    static LE_NOTHROWNOALIAS typename Impl<location>::type::MemoryMapping
    map(char const *relativeFilePath);
    template <SpecialLocations location>
    static LE_NOTHROWNOALIAS typename Impl<location>::type::Stream
    open(char const *relativeFilePath, std::uint32_t flags);
}; // class ResourceFile

////////////////////////////////////////////////////////////////////////////////
///
/// \class File::MemoryMapping
///
////////////////////////////////////////////////////////////////////////////////

class File::MemoryMapping : public std::pair<char const *, char const *>
{
  public:
    typedef char const value_type;

    typedef std::pair<value_type *, value_type *> Range;

    LE_NOTHROWNOALIAS MemoryMapping();
    LE_NOTHROW MemoryMapping(MemoryMapping &&) LE_NOEXCEPT;
    LE_NOTHROW ~MemoryMapping();

    LE_NOTHROWNOALIAS value_type *begin() const { return first; }
    LE_NOTHROWNOALIAS value_type *end() const { return second; }

    LE_NOTHROWNOALIAS std::uint32_t size() const
    {
        return static_cast<std::uint32_t>(end() - begin());
    }

    LE_NOTHROWNOALIAS value_type operator[](std::uint32_t const index) const
    {
        assert(index < size());
        return begin()[index];
    }

    LE_NOTHROW MemoryMapping &operator=(MemoryMapping &&) LE_NOEXCEPT;

#if !(defined(_MSC_VER) && (_MSC_VER < 1800))
    explicit
#endif // old MSVC
    operator bool() const LE_NOEXCEPT
    {
        return begin() != nullptr;
    }

    template <typename Element> operator Span<Element>() const { return {begin(), end()}; }

  private:
    friend class File;
    LE_NOTHROWNOALIAS explicit MemoryMapping(Range const &);

  private:
#if defined(_MSC_VER) && (_MSC_VER < 1800)
    MemoryMapping(MemoryMapping const &);
    void operator=(MemoryMapping const &);
#else
    MemoryMapping(MemoryMapping const &) = delete;
    void operator=(MemoryMapping const &) = delete;
#endif // _MSC_VER

    using Range::first;
    using Range::second;
}; // class MemoryMapping

////////////////////////////////////////////////////////////////////////////////
///
/// \class File::Stream
///
////////////////////////////////////////////////////////////////////////////////

class File::Stream
{
  public:
    LE_NOTHROWNOALIAS Stream();
    LE_NOTHROW Stream(Stream &&) LE_NOEXCEPT;
    LE_NOTHROW ~Stream();

    LE_NOTHROWNOALIAS std::uint32_t read(void *pBuffer, std::uint32_t numberOfBytesToRead);
    LE_NOTHROWNOALIAS std::uint32_t write(void const *pBuffer, std::uint32_t numberOfBytesToWrite);

    LE_NOTHROWNOALIAS std::uint32_t size() const;
    LE_NOTHROWNOALIAS std::uint32_t position() const;
    LE_NOTHROWNOALIAS bool seek(std::int32_t offset, std::uint8_t whence);

    LE_NOTHROWNOALIAS int asPOSIXFile(::off_t &startOffset, std::size_t &size) const;

    LE_NOTHROW void close();

    LE_NOTHROW Stream &operator=(Stream &&) LE_NOEXCEPT;
    LE_NOTHROWNOALIAS bool operator!() const;

    int nativeHandle() const { return handle_; }

  private:
    friend class File;
    LE_NOTHROWNOALIAS Stream(int);

#if defined(_MSC_VER) && (_MSC_VER < 1800)
  private:
    Stream(Stream const &);
#else
    Stream(Stream const &) = delete;
#endif // _MSC_VER

  private:
    int handle_;
}; // class Stream

#if defined(__ANDROID__) || defined(DOXYGEN_ONLY)

////////////////////////////////////////////////////////////////////////////////
///
/// \class ResourceFile::MemoryMapping
///
////////////////////////////////////////////////////////////////////////////////

class ResourceFile::MemoryMapping
{
  public:
    LE_NOTHROWNOALIAS MemoryMapping();
    LE_NOTHROW MemoryMapping(MemoryMapping &&) LE_NOEXCEPT;
    LE_NOTHROW ~MemoryMapping();

    using value_type = char const;

    LE_NOTHROWNOALIAS value_type *begin() const;
    LE_NOTHROWNOALIAS value_type *end() const;

    LE_NOTHROWNOALIAS std::uint32_t size() const;

    LE_NOTHROWNOALIAS char operator[](std::uint32_t index) const;

    LE_NOTHROW MemoryMapping &operator=(MemoryMapping &&) LE_NOEXCEPT;
    LE_NOTHROWNOALIAS bool operator!() const;
    LE_NOTHROWNOALIAS explicit operator bool() const;

  private:
    friend class ResourceFile;
    explicit MemoryMapping(void *);
    MemoryMapping(MemoryMapping const &) = delete;

  private:
    void *handle_;
}; // class MemoryMapping

////////////////////////////////////////////////////////////////////////////////
///
/// \class ResourceFile::Stream
///
////////////////////////////////////////////////////////////////////////////////

class ResourceFile::Stream
{
  public:
    LE_NOTHROWNOALIAS Stream();
    LE_NOTHROW Stream(Stream &&);
    LE_NOTHROW ~Stream();

    LE_NOTHROWNOALIAS std::uint32_t read(void *pBuffer, std::uint32_t numberOfBytesToRead);

    LE_NOTHROWNOALIAS std::uint32_t size() const;
    LE_NOTHROWNOALIAS std::uint32_t position() const;
    LE_NOTHROWNOALIAS bool seek(std::int32_t offset, std::uint8_t whence);

    LE_NOTHROWNOALIAS int asPOSIXFile(::off_t &startOffset, std::size_t &size) const;

    LE_NOTHROW Stream &operator=(Stream &&) LE_NOEXCEPT;
    LE_NOTHROWNOALIAS bool operator!() const;
    LE_NOTHROWNOALIAS explicit operator bool() const;

  private:
    friend class ResourceFile;
    explicit Stream(void *);
    Stream(Stream const &) = delete;

  private:
    void *handle_;
}; // class Stream

template <> struct File::Impl<Resources>
{
    using type = ResourceFile;
};
template <>
LE_NOTHROWNOALIAS inline typename File::Impl<Resources>::type::MemoryMapping
File::map<Resources>(char const *const relativeFilePath)
{
    return ResourceFile::map(relativeFilePath);
}
template <>
LE_NOTHROWNOALIAS inline typename File::Impl<Resources>::type::Stream
File::open<Resources>(char const *const relativeFilePath, std::uint32_t /*flags*/)
{
    return ResourceFile::open(relativeFilePath);
}

#else // other than Android

class ResourceFile : public File
{
  public:
    static LE_NOTHROWNOALIAS MemoryMapping map(char const *const relativeFilePath)
    {
        return File::map<Resources>(relativeFilePath);
    }
    static LE_NOTHROWNOALIAS Stream open(char const *const relativeFilePath)
    {
        return File::open<Resources>(relativeFilePath, O_RDONLY);
    }
}; // class ResourceFile

#endif // __ANDROID__ || DOXYGEN_ONLY

template <SpecialLocations location>
LE_NOTHROWNOALIAS char const *fullPath(char const *relativeFilePath);

template <SpecialLocations location, bool writeAccess> LE_NOTHROWNOALIAS bool accessible();

#if defined(__ANDROID__) || defined(DOXYGEN_ONLY)
/// \addtogroup Android Android specific
/// @{
/// This support functionality requires linking with the "android"
/// (system-provided) library.

/// In order to access the app's assets and app-specific directories, access to
/// the app's (A)AssetManager and activity objects is required which should
/// therefore be set/provided once on app startup using one of the below
/// functions.
///
/// The Android environment allows for more than one way to access the mentioned
/// objects and, as a courtesy, the LE.Utility library tries to cover all use
/// cases. The choice of which setAppContext() overload to use is solely one of
/// user preference.
LE_NOTHROW bool setAppContext(::JNIEnv &, ::jobject activity, ::jobject assetManager);
LE_NOTHROW bool setAppContext(::JNIEnv &, ::jobject activity); ///< \overload
LE_NOTHROW bool setAppContext(::ANativeActivity const &);      ///< \overload
LE_NOTHROW bool setAppContext(::ANativeActivity const &,
                              ::ndk_helper::JNIHelper const &); ///< \overload

/// <B>Effect:</B> Returns the app's native AssetManager instance.<BR>
/// <B>Preconditions:</B> a successful call to setAppContext().
LE_NOTHROW LE_CONST_FUNCTION ::AAssetManager &resourceManager();
/// @}  // group Android
#endif // __ANDROID__

//------------------------------------------------------------------------------
} // namespace Utility
//------------------------------------------------------------------------------
/// @} // group Utility
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // filsystem_hpp
