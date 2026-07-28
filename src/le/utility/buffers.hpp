////////////////////////////////////////////////////////////////////////////////
///
/// \file buffers.hpp
/// -----------------
///
/// Holds generic buffer classes.
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef buffers_hpp__83BAA722_FD75_4C6D_9CE6_3194647206CE
#define buffers_hpp__83BAA722_FD75_4C6D_9CE6_3194647206CE
//------------------------------------------------------------------------------
#include "le/utility/intrinsics.hpp"
#include "le/utility/platformSpecifics.hpp"
#include "le/utility/tchar.hpp"

#ifdef LE_HAS_NT2
// NT2/Boost.SIMD
#include "boost/simd/memory/aligned_free.hpp"
#include "boost/simd/memory/aligned_malloc.hpp"
#include "boost/simd/memory/aligned_object.hpp"
#include "boost/simd/memory/aligned_reuse.hpp"
#endif // LE_HAS_NT2

#include "assert.hpp"
#include "span.hpp"

#include <cstdint>
#include <type_traits>
#include <utility>
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
LE_IMPL_NAMESPACE_BEGIN(Math)
void *align(void *);
LE_IMPL_NAMESPACE_END(Math)
//------------------------------------------------------------------------------
namespace Utility
{
//------------------------------------------------------------------------------

//...mrmlj...move/rename to some better location/namespace...
inline unsigned int align(unsigned int const storageBytes)
{
    using Utility::Constants::vectorAlignment;
    unsigned int totalBytes((storageBytes + vectorAlignment - 1) & ~(vectorAlignment - 1));
    return totalBytes;
}

#ifdef LE_HAS_NT2
////////////////////////////////////////////////////////////////////////////////
///
/// \class AlignedBuffer
///
/// \brief A template class providing an aligned block of (statically allocated)
/// storage.
///
////////////////////////////////////////////////////////////////////////////////

template <typename Element, unsigned int numberOfElements,
          bool defaultAutomaticInitialization = true,
          unsigned int alignmentSize = std::alignment_of<Element>::value>
class AlignedBuffer : public boost::simd::aligned_object<alignmentSize>
{
    static_assert(std::is_pod<Element>::value, "Only PODs supported");

  public:
    typedef Element value_type;

    typedef Element *LE_RESTRICT iterator;
    typedef Element const *LE_RESTRICT const_iterator;
    typedef Element &reference;
    typedef Element const &const_reference;

    typedef Element Array[numberOfElements];

    static unsigned int const static_size = numberOfElements;

  public:
    explicit AlignedBuffer(bool const initialize = defaultAutomaticInitialization)
    {
        if (initialize)
            clear();
    }

    iterator begin() { return buffer(); }
    const_iterator begin() const { return const_cast<AlignedBuffer &>(*this).begin(); }

    iterator end() { return begin() + size(); }
    const_iterator end() const { return const_cast<AlignedBuffer &>(*this).end(); }

    static unsigned int size() { return static_size; }

    Element &operator[](size_t const index)
    {
        LE_ASSERT_MSG(index < numberOfElements, "Buffer index out of range!");
        return buffer()[index];
    }
    Element const &operator[](size_t const index) const
    {
        return const_cast<AlignedBuffer &>(*this).operator[](index);
    }

    void clear() { std::memset(begin(), 0, size() * sizeof(Element)); }

    Array &c_array() { return reinterpret_cast<Array &>(*buffer()); }
    Array const &c_array() const { return reinterpret_cast<Array const &>(*buffer()); }

  private:
    Element *buffer()
    {
        LE_ASSERT_MSG((reinterpret_cast<std::size_t>(&storage_) % alignmentSize) == 0,
                      "Aligned buffer misaligned.");
        return reinterpret_cast<Element *>(&storage_);
    }

  private:
    // Implementation note:
    //   std::aligned_storage implementation does not give properly aligned
    // storage for alignments larger than the largest builtin type, hence
    // boost::aligned_storage originally and an explicit alignas now.
    //                                        (15.11.2010.) (Domagoj Saric)
    struct alignas(alignmentSize) Storage
    {
        char bytes[sizeof(Element) * numberOfElements];
    };
    Storage storage_;

    static_assert(sizeof(Storage) >= (sizeof(Element) * numberOfElements),
                  "Internal inconsistency");
}; // class AlignedBuffer

////////////////////////////////////////////////////////////////////////////////
///
/// \class AlignedHeapBuffer
///
////////////////////////////////////////////////////////////////////////////////

template <typename T> class AlignedHeapBuffer : public Span<T>
{
  public:
    typedef Span<T> Range;
    typedef typename Range::value_type value_type;

    AlignedHeapBuffer(AlignedHeapBuffer const &) = delete; // makes non-copyable
    AlignedHeapBuffer &operator=(AlignedHeapBuffer const &) = delete;

    AlignedHeapBuffer() { LE_ASSERT(this->empty() && !this->data()); }
    AlignedHeapBuffer(AlignedHeapBuffer &&source) : Range(source)
    {
        static_cast<Range &>(source) = Range();
    }
#if !defined(__APPLE__) && !defined(_WIN64)
    ~AlignedHeapBuffer() { boost::simd::aligned_free(this->data()); }
#else
    ~AlignedHeapBuffer() { std ::free(this->data()); }
#endif

    unsigned int size() const { return static_cast<unsigned int>(Range::size()); }

    bool LE_COLD resize(unsigned int const numberOfElements)
    {
#if defined(_MSC_VER) && defined(BOOST_SIMD_MEMORY_USE_BUILTINS)
        //...mrmlj...MSVC10 aligned_realloc seems to reallocate even if the size
        //...mrmlj...does not change...reinvestigate...
        if (numberOfElements == this->size())
            return true;
#endif // _MSC_VER && BOOST_SIMD_MEMORY_USE_BUILTINS
#ifdef __APPLE__
        /// \note OSX std::realloc does not return a nullptr with 0 sizes so we
        /// explicitly handle this case out of paranoia in case some code uses
        /// Range::data() != nullptr instead of Range::empty() to check for
        /// validity.
        /// https://www.securecoding.cert.org/confluence/display/seccode/MEM30-C.+Do+not+access+freed+memory
        /// http://stackoverflow.com/questions/11455317/realloc-memory-for-a-pointer-which-has-been-freed
        /// http://www.open-std.org/jtc1/sc22/wg14/www/docs/dr_400.htm
        ///                                   (26.08.2014.) (Domagoj Saric)
        if (numberOfElements == 0)
        {
            *this = AlignedHeapBuffer();
            return true;
        }
#endif // __APPLE__
        value_type *const pNewMemory(static_cast<value_type *>(
#if !defined(__APPLE__) && !defined(_WIN64)
            boost::simd::aligned_reuse(this->data(), numberOfElements * sizeof(value_type),
                                       Utility::Constants::vectorAlignment)
#else
            std::realloc(this->data(), numberOfElements * sizeof(value_type))
#endif
                ));
        if (pNewMemory || !numberOfElements)
        {
            LE_ASSERT_MSG((pNewMemory != nullptr) == (numberOfElements != 0),
                          "Unexpected realloc result");
            LE_ASSERT_MSG((reinterpret_cast<std::size_t>(pNewMemory) %
                           Utility::Constants::vectorAlignment) == 0,
                          "Aligned allocation misaligned.");
            static_cast<Range &>(*this) = Range(pNewMemory, pNewMemory + numberOfElements);
            LE_ASSERT(size() == numberOfElements);
            return true;
        }
        else
        {
            return false;
        }
    }

    AlignedHeapBuffer &operator=(AlignedHeapBuffer &&source)
    {
        std::swap(static_cast<Range &>(*this), static_cast<Range &>(source));
        return *this;
    }

  private:
    using Range::advance_begin;
    using Range::advance_end;
    using Range::pop_back;
    using Range::pop_front;
}; // class AlignedHeapBuffer
#endif // LE_HAS_NT2

////////////////////////////////////////////////////////////////////////////////
///
/// \class SharedStorageBuffer
///
/// \brief A buffer that allocates no storage of its own, rather it sizes and
/// positions itself in separately preallocated and externally provided storage.
///
/// Preallocating storage in a single place and then positioning all auxiliary
/// buffers in this storage improves locality of reference. This holds both in
/// the case where all buffers would be separately dynamically allocated (as
/// they may end up in separate regions of memory) as well as when the buffers
/// are statically preallocated for their maximum size (then unused gaps appear
/// between the used regions when buffers are sized below their maximum size,
/// which is our most common use case).
///
////////////////////////////////////////////////////////////////////////////////

typedef Span<char> Storage;

#pragma warning(push)
#pragma warning(disable : 4127) // Conditional expression is constant.

template <typename T> class SharedStorageBuffer : public Span<T>
{
  public:
    SharedStorageBuffer() {}

    using Range = Span<T>;

    LE_NOINLINE LE_NOTHROWNOALIAS LE_COLD void clear()
    {
        static_assert(__has_trivial_constructor(T) || std::is_scalar<T>::value,
                      "SharedStorageBuffer supports only primitive types");
        std::memset(Range::begin(), 0, size() * sizeof(T));
    }

    LE_NOINLINE LE_NOTHROW LE_COLD void resize(std::uint32_t const newSize, Storage &storage)
    {
        LE_ASSERT_MSG(newSize % sizeof(T) == 0, "Invalid size.");
        LE_ASSERT_MSG(static_cast<std::size_t>(storage.size()) >= newSize,
                      "Not enough shared storage space.");

        using iterator = T *LE_RESTRICT;

        bool const doAlign(!std::is_pointer<T>::value);

        /// \note static_casting through void * (instead of reinterpret_casting
        /// throguh std::size_t) fails with compiler errors on GCC 4.9 on ranges
        /// of pointers (it complains about __restrict being inapliccable to
        /// void).
        ///                                   (24.12.2014.) (Domagoj Saric)
        iterator const newBeginning(reinterpret_cast<T *>(reinterpret_cast<std::size_t>(
            doAlign ? Math::align(storage.begin()) : storage.begin())));
        auto const alignmentFixup(
            static_cast<std::uint8_t>(reinterpret_cast<std::size_t>(newBeginning) -
                                      reinterpret_cast<std::size_t>(storage.begin())));
        LE_ASSERT_MSG(static_cast<std::size_t>(storage.size()) >= alignmentFixup + newSize,
                      "Not enough shared storage space.");
        storage.advance_begin(alignmentFixup + newSize);
        iterator const newEnd(
            reinterpret_cast<T *>(reinterpret_cast<std::size_t>(storage.begin())));
        LE_ASSERT_MSG(newBeginning <= newEnd, "Failed to generate a valid range.");
        LE_ASSERT_MSG(
            !doAlign ||
                (reinterpret_cast<std::size_t>(newBeginning) % Constants::vectorAlignment == 0),
            "Failed to generate a properly aligned range.");
        static_cast<Range &>(*this) = Range(newBeginning, newEnd);
        LE_ASSERT_MSG(size() == newSize / sizeof(T), "Generated range has an invalid size.");

        if (!__has_trivial_constructor(T))
        {
            T *LE_RESTRICT pT(this->begin());
            while (pT != this->end())
            {
                LE_ASSUME(pT);
                T *const pNewT(new (pT) T);
                LE_ASSUME(pNewT);
                ++pT;
            }
        }
    }

    std::uint32_t size() const { return static_cast<std::uint32_t>(Range::size()); }

    void alias(SharedStorageBuffer const &other)
    {
        static_cast<Range &>(*this) = static_cast<Range const &>(other);
    }

    /// \note Span<T> and Span<T const> are both a begin/end pointer pair, so
    /// this stays the free reinterpretation it was with boost::iterator_range.
    operator Span<T const> const &() const
    {
        return reinterpret_cast<Span<T const> const &>(*this);
    }

  private:
    static_assert(
#if defined(_MSC_VER) && (_MSC_VER < 1900) //...mrmlj...
        std::has_trivial_destructor<T>::value,
#else
        __has_trivial_destructor(T),
#endif
        "SharedStorageBuffer supports only primitive types");

    SharedStorageBuffer(SharedStorageBuffer const &);

    using Range::advance_begin;
    using Range::advance_end;
    using Range::pop_back;
    using Range::pop_front;
    using Range::operator=;
}; // class SharedStorageBuffer

#pragma warning(pop)

//------------------------------------------------------------------------------
} // namespace Utility
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // buffers_hpp
