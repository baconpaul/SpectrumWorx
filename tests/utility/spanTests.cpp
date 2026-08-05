////////////////////////////////////////////////////////////////////////////////
///
/// spanTests.cpp
/// -------------
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "le/utility/span.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <numeric>
#include <span>
#include <type_traits>
#include <vector>
//------------------------------------------------------------------------------

using LE::Utility::makeSpan;
using LE::Utility::Span;

TEST_CASE("Span is a begin/end pointer pair", "[span]")
{
    // The engine reinterpret_casts Span<T> to Span<T const> in
    // SharedStorageBuffer::operator Span<T const> const &.
    static_assert(sizeof(Span<float>) == 2 * sizeof(void *));
    static_assert(sizeof(Span<float>) == sizeof(Span<float const>));
    // A plain pointer, and the exact type begin() returns. The alias used to be
    // `float *LE_RESTRICT`, which no accessor could return: a top level
    // qualifier on a return type is dropped, so the two differed everywhere the
    // alias was used.
    static_assert(std::is_same_v<Span<float>::iterator, float *>);
    static_assert(std::is_same_v<decltype(std::declval<Span<float> const &>().begin()),
                                 Span<float>::iterator>);

    float array[4]{1, 2, 3, 4};
    Span<float> const span(array);
    CHECK(span.data() == &array[0]);
    CHECK(span.begin() == &array[0]);
    CHECK(span.end() == &array[4]);
    CHECK(span.size() == 4);
}

TEST_CASE("An empty Span is falsy and a populated one is not", "[span]")
{
    float array[2]{};
    CHECK_FALSE(static_cast<bool>(Span<float>()));
    CHECK(Span<float>().empty());
    CHECK(static_cast<bool>(makeSpan(array)));
    CHECK_FALSE(makeSpan(array).empty());
}

TEST_CASE("Span converts from mutable to const but not back", "[span]")
{
    static_assert(std::is_convertible_v<Span<float>, Span<float const>>);
    static_assert(!std::is_convertible_v<Span<float const>, Span<float>>);

    float array[3]{7, 8, 9};
    Span<float const> const readOnly(makeSpan(array));
    CHECK(readOnly.size() == 3);
    CHECK(readOnly.front() == 7);
    CHECK(readOnly.back() == 9);
}

TEST_CASE("Span slides", "[span]")
{
    // This is why it is not std::span: the moving average, the vocoder envelope
    // and SharedStorageBuffer::resize all walk a window across a buffer.
    std::array<float, 8> data;
    std::iota(data.begin(), data.end(), 0.0f);

    Span<float> window(data.data(), std::size_t{3});
    CHECK(window.size() == 3);
    CHECK(window.front() == 0);
    CHECK(window.back() == 2);

    window.advance_begin(1);
    window.advance_end(1);
    CHECK(window.size() == 3);
    CHECK(window.front() == 1);
    CHECK(window.back() == 3);

    window.pop_front();
    CHECK(window.size() == 2);
    CHECK(window.front() == 2);

    window.pop_back();
    CHECK(window.size() == 1);
    CHECK(window.front() == 2);

    // advance_begin returns *this so it can be used inline.
    CHECK(window.advance_end(2).size() == 3);
}

TEST_CASE("Span subspans", "[span]")
{
    std::array<float, 6> data;
    std::iota(data.begin(), data.end(), 10.0f);
    auto const span(makeSpan(data));

    CHECK(span.subspan(2).size() == 4);
    CHECK(span.subspan(2).front() == 12);
    CHECK(span.subspan(1, 3).size() == 3);
    CHECK(span.subspan(1, 3).back() == 13);
    CHECK(span.subspan(6).empty());
}

TEST_CASE("makeSpan deduces from arrays, containers and pointer pairs", "[span]")
{
    float array[5]{};
    std::vector<int> vector{1, 2, 3};
    std::vector<int> const constVector{1, 2, 3, 4};

    static_assert(std::is_same_v<decltype(makeSpan(array)), Span<float>>);
    static_assert(std::is_same_v<decltype(makeSpan(vector)), Span<int>>);
    static_assert(std::is_same_v<decltype(makeSpan(constVector)), Span<int const>>);

    CHECK(makeSpan(array).size() == 5);
    CHECK(makeSpan(vector).size() == 3);
    CHECK(makeSpan(constVector).size() == 4);
    CHECK(makeSpan(&array[1], &array[4]).size() == 3);
    CHECK(makeSpan(&array[0], 2u).size() == 2);
}

TEST_CASE("Span converts to std::span so call sites can migrate one at a time", "[span]")
{
    std::array<float, 4> data{1, 2, 3, 4};
    std::span<float> const standard(makeSpan(data));
    CHECK(standard.size() == 4);
    CHECK(standard.data() == data.data());
}
