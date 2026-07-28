////////////////////////////////////////////////////////////////////////////////
///
/// filesystem.cpp
/// --------------
///
/// Copyright (c) 2011 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "filesystem.hpp"
#include "filesystemImpl.hpp"

#include "platformSpecifics.hpp"
#include "trace.hpp"

// Boost sandbox
#include "boost/mmap/mapped_view/mapped_view.hpp"
#include "boost/mmap/mappble_objects/file/utility.hpp"
#ifndef BOOST_MMAP_HEADER_ONLY
#include "boost/mmap/amalgamated_lib.cpp"
#endif // BOOST_MMAP_HEADER_ONLY

#include "assert.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace Utility
{
//------------------------------------------------------------------------------

LE_OPTIMIZE_FOR_SIZE_BEGIN()

namespace
{
void unmap(File::MemoryMapping::Range const &rangePair)
{
    using mapped_view_reference_t = boost::mmap::mapped_view_reference<char const>;
    auto const range(mapped_view_reference_t::memory_range_t(rangePair.first, rangePair.second));
    auto const &mapped_view_reference(static_cast<mapped_view_reference_t const &>(range));
    mapped_view_reference_t::unmap(mapped_view_reference);
}
} // namespace

File::MemoryMapping::MemoryMapping() {}
File::MemoryMapping::MemoryMapping(Range const &range) : Range(range) {}
File::MemoryMapping::MemoryMapping(MemoryMapping &&other) : Range(other)
{
    static_cast<Range &>(other) = Range();
}
File::MemoryMapping::~MemoryMapping() { unmap(*this); }

File::MemoryMapping &File::MemoryMapping::operator=(File::MemoryMapping &&other)
{
    unmap(*this);
    static_cast<Range &>(*this) = static_cast<Range const &>(other);
    static_cast<Range &>(other) = Range();
    return *this;
}

namespace
{
int const invalidHandle(-1);
} // anonymous namespace

File::Stream::Stream() : handle_(invalidHandle) {}
File::Stream::Stream(int const fileDescriptor) : handle_(fileDescriptor) {}
File::Stream::Stream(Stream &&other) : handle_(other.handle_) { other.handle_ = -1; }
File::Stream::~Stream() { close(); }

LE_COLD void File::Stream::close()
{
    if (handle_ != invalidHandle)
    {
        LE_VERIFY(::close(handle_) == 0);
        handle_ = invalidHandle;
    }
}

bool File::Stream::operator!() const { return handle_ == invalidHandle; }

File::Stream &File::Stream::operator=(File::Stream &&other)
{
    close();
    this->handle_ = other.handle_;
    other.handle_ = invalidHandle;
    return *this;
}

std::uint32_t File::Stream::read(void *pBuffer, std::uint32_t numberOfBytesToRead)
{
    LE_ASSERT_MSG(handle_ != invalidHandle, "No file open");
    auto const result(::read(handle_, pBuffer, numberOfBytesToRead));
    LE_TRACE_IF(result < 0, "File read error (%d).", errno);
    return static_cast<std::uint32_t>(std::max(0, static_cast<std::int32_t>(result)));
}
std::uint32_t File::Stream::write(void const *pBuffer, std::uint32_t numberOfBytesToWrite)
{
    LE_ASSERT_MSG(handle_ != invalidHandle, "No file open");
    auto const result(::write(handle_, pBuffer, numberOfBytesToWrite));
    LE_TRACE_IF(result < 0, "File write error (%d).", errno);
    return static_cast<std::uint32_t>(std::max(0, static_cast<std::int32_t>(result)));
}

#ifdef _MSC_VER
std::uint32_t File::Stream::position() const { return static_cast<std::uint32_t>(::tell(handle_)); }
#else  //...mrmlj...no tell on android or ios...
std::uint32_t File::Stream::position() const
{
    return static_cast<std::uint32_t>(::lseek(handle_, 0, SEEK_CUR));
}
#endif // _MSC_VER
bool File::Stream::seek(std::int32_t const offset, std::uint8_t const whence)
{
    return ::lseek(handle_, offset, whence) != -1;
}

std::uint32_t File::Stream::size() const
{
#ifdef _MSC_VER
    return static_cast<std::uint32_t>(/*std*/ ::_filelength(handle_));
#else
    struct stat file_status;
    LE_VERIFY(::fstat(handle_, &file_status) == 0);
    return static_cast<std::uint32_t>(file_status.st_size);
#endif // _MSC_VER
}

int File::Stream::asPOSIXFile(::off_t &startOffset, std::size_t &size) const
{
    int const newDescriptor(::dup(handle_));
    if (newDescriptor == -1)
        return -1;
    startOffset = 0;
    size = this->size();
    return newDescriptor;
}

LE_OPTIMIZE_FOR_SIZE_END()

//------------------------------------------------------------------------------
} // namespace Utility
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
