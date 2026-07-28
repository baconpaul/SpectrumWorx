////////////////////////////////////////////////////////////////////////////////
///
/// buffersTests.cpp
/// ----------------
///
///   Covers the allocation paths stage 3 rewrote: AlignedBuffer no longer
/// derives from boost::simd::aligned_object, AlignedHeapBuffer no longer calls
/// boost::simd::aligned_reuse, and the NT2 stack-buffer macros are gone.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "le/utility/buffers.hpp"
#include "le/utility/intrinsics.hpp"
#include "le/utility/stackBuffer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
//------------------------------------------------------------------------------

namespace
{
constexpr auto alignment{LE::Utility::Constants::vectorAlignment};

bool aligned(void const *const pointer)
{
    return (reinterpret_cast<std::uintptr_t>(pointer) % alignment) == 0;
}
} // anonymous namespace

TEST_CASE("AlignedBuffer honours its alignment without a Boost base class", "[buffers]")
{
    LE::Utility::AlignedBuffer<float, 37, true, alignment> buffer;
    CHECK(buffer.size() == 37);
    CHECK(aligned(buffer.begin()));
    // defaultAutomaticInitialization is true, so it starts zeroed.
    CHECK(std::all_of(buffer.begin(), buffer.end(), [](float const v) { return v == 0; }));

    buffer[0] = 1;
    buffer[36] = 2;
    CHECK(buffer.begin()[0] == 1);
    CHECK(buffer.end()[-1] == 2);

    buffer.clear();
    CHECK(std::all_of(buffer.begin(), buffer.end(), [](float const v) { return v == 0; }));
}

TEST_CASE("AlignedBuffer can skip initialisation", "[buffers]")
{
    LE::Utility::AlignedBuffer<float, 8, false, alignment> buffer(false);
    CHECK(buffer.size() == 8);
    CHECK(aligned(buffer.begin()));
}

TEST_CASE("AlignedHeapBuffer allocates over-aligned storage and keeps the data on resize",
          "[buffers]")
{
    // std::max_align_t is 8 on arm64, half of what the SIMD paths need, so
    // this cannot go through plain malloc/realloc.
    LE::Utility::AlignedHeapBuffer<float> buffer;
    CHECK(buffer.empty());
    CHECK(buffer.data() == nullptr);

    REQUIRE(buffer.resize(100));
    CHECK(buffer.size() == 100);
    CHECK(aligned(buffer.data()));
    std::iota(buffer.begin(), buffer.end(), 0.0f);

    REQUIRE(buffer.resize(200));
    CHECK(buffer.size() == 200);
    CHECK(aligned(buffer.data()));
    for (unsigned int index(0); index < 100; ++index)
        CHECK(buffer[index] == static_cast<float>(index));

    REQUIRE(buffer.resize(10));
    CHECK(buffer.size() == 10);
    CHECK(aligned(buffer.data()));
    for (unsigned int index(0); index < 10; ++index)
        CHECK(buffer[index] == static_cast<float>(index));

    REQUIRE(buffer.resize(0));
    CHECK(buffer.empty());
}

TEST_CASE("AlignedHeapBuffer moves without freeing twice", "[buffers]")
{
    LE::Utility::AlignedHeapBuffer<float> source;
    REQUIRE(source.resize(16));
    source[0] = 42;
    auto const *const pOriginal(source.data());

    LE::Utility::AlignedHeapBuffer<float> destination(std::move(source));
    CHECK(destination.data() == pOriginal);
    CHECK(destination.size() == 16);
    CHECK(destination[0] == 42);
    CHECK(source.data() == nullptr); // NOLINT(bugprone-use-after-move)
}

TEST_CASE("Utility::align rounds byte counts up to the vector width", "[buffers]")
{
    CHECK(LE::Utility::align(0) == 0);
    CHECK(LE::Utility::align(1) == alignment);
    CHECK(LE::Utility::align(alignment) == alignment);
    CHECK(LE::Utility::align(alignment + 1) == 2 * alignment);
}

TEST_CASE("The stack buffers are sized and aligned", "[buffers][stackBuffer]")
{
    // Clang's alloca does not align, which is why the macro aligns by hand and
    // why even the pointer-array call sites in processor.cpp take this one.
    LE_ALIGNED_STACK_BUFFER(floats, float, 33);
    CHECK(floats.size() == 33);
    CHECK(aligned(floats.data()));
    std::fill(floats.begin(), floats.end(), 1.0f);
    CHECK(floats.front() == 1);
    CHECK(floats.back() == 1);

    LE_ALIGNED_SCOPED_STACK_BUFFER(bytes, char, 7);
    CHECK(bytes.size() == 7);
    CHECK(aligned(bytes.data()));

    LE_STACK_BUFFER(pointers, float *, 4);
    for (unsigned int index(0); index < 4; ++index)
        pointers[index] = floats.data() + index;
    CHECK(pointers[3] == floats.data() + 3);
}

TEST_CASE("The stack buffer evaluates its count once", "[buffers][stackBuffer]")
{
    unsigned int calls(0);
    auto const count([&] {
        ++calls;
        return 5u;
    });
    LE_ALIGNED_STACK_BUFFER(buffer, float, count());
    CHECK(calls == 1);
    CHECK(buffer.size() == 5);
}
