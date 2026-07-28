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

#include "assert.hpp"

#ifdef _WIN32
#include "windowsLite.hpp"
#else
#include <sys/mman.h>
#endif // _WIN32

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

File::MemoryMapping::Range Detail::mapReadOnlyFile(char const *const fullPath)
{
    using Range = File::MemoryMapping::Range;
#ifdef _WIN32
    auto const file(::CreateFileA(fullPath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
    if (file == INVALID_HANDLE_VALUE)
        return Range();
    LARGE_INTEGER size;
    auto const mapping(::GetFileSizeEx(file, &size) && size.QuadPart
                           ? ::CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr)
                           : nullptr);
    ::CloseHandle(file);
    if (!mapping)
        return Range();
    auto const pBegin(static_cast<char const *>(::MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0)));
    ::CloseHandle(mapping); // the view keeps the mapping alive
    if (!pBegin)
        return Range();
    return Range(pBegin, pBegin + size.QuadPart);
#else
    auto const descriptor(::open(fullPath, O_RDONLY));
    if (descriptor < 0)
        return Range();
    struct stat status;
    auto const size((::fstat(descriptor, &status) == 0) ? static_cast<std::size_t>(status.st_size)
                                                        : 0);
    auto const pMapping(size ? ::mmap(nullptr, size, PROT_READ, MAP_PRIVATE, descriptor, 0)
                             : MAP_FAILED);
    LE_VERIFY(::close(descriptor) == 0);
    if (pMapping == MAP_FAILED)
        return Range();
    auto const pBegin(static_cast<char const *>(pMapping));
    return Range(pBegin, pBegin + size);
#endif // _WIN32
}

void Detail::unmapFile(File::MemoryMapping::Range const &range)
{
    if (!range.first)
        return;
#ifdef _WIN32
    LE_VERIFY(::UnmapViewOfFile(range.first));
#else
    LE_VERIFY(::munmap(const_cast<char *>(range.first),
                       static_cast<std::size_t>(range.second - range.first)) == 0);
#endif // _WIN32
}

namespace
{
void unmap(File::MemoryMapping::Range const &rangePair) { Detail::unmapFile(rangePair); }
} // namespace

File::MemoryMapping::MemoryMapping() {}
File::MemoryMapping::MemoryMapping(Range const &range) : Range(range) {}
File::MemoryMapping::MemoryMapping(MemoryMapping &&other) noexcept : Range(other)
{
    static_cast<Range &>(other) = Range();
}
File::MemoryMapping::~MemoryMapping() { unmap(*this); }

File::MemoryMapping &File::MemoryMapping::operator=(File::MemoryMapping &&other) noexcept
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
File::Stream::Stream(Stream &&other) noexcept : handle_(other.handle_) { other.handle_ = -1; }
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

File::Stream &File::Stream::operator=(File::Stream &&other) noexcept
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
