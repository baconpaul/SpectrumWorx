////////////////////////////////////////////////////////////////////////////////
///
/// \file platformSpecifics.hpp
/// ---------------------------
///
///   An internal collection of macros that wrap platform specific details/non
/// standard extensions (expands the public parts exposed in abi.hpp).
///
/// Copyright (c) 2010 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef platformSpecifics_hpp__B98C0700_01F9_4B49_AADC_C1AF5BB3EB9B
#define platformSpecifics_hpp__B98C0700_01F9_4B49_AADC_C1AF5BB3EB9B
//------------------------------------------------------------------------------
#include "abi.hpp"

#include "assert.hpp"
//------------------------------------------------------------------------------

/// \note `!defined(__GNUC__)` rather than `!defined(__clang__)`, and abi.hpp
/// says why: the arms are dialects, and clang-cl speaks the first one.
/// __forceinline, __declspec(noinline), __declspec(selectany) and __assume are
/// all accepted by it -- measured, not assumed.
///                                       (09.08.2026.) (SW port)
#if defined(_MSC_VER) && !defined(__GNUC__)

#define LE_FORCEINLINE __forceinline
#define LE_NOINLINE __declspec(noinline)

#define LE_WEAK_SYMBOL __declspec(selectany)
#define LE_WEAK_SYMBOL_CONST __declspec(selectany) extern

#define LE_UNREACHABLE_CODE()                                                                      \
    LE_ASSERT_MSG(false, "This code should not be reached.");                                      \
    __assume(false)

#elif defined(__GNUC__)

#ifdef _DEBUG
#define LE_FORCEINLINE inline
#else
#define LE_FORCEINLINE __attribute__((always_inline)) inline
#endif
#define LE_NOINLINE __attribute__((noinline))

#define LE_WEAK_SYMBOL __attribute__((weak))
#define LE_WEAK_SYMBOL_CONST LE_WEAK_SYMBOL extern

/// \note The three-armed cascade this replaces chose between __builtin_assume,
/// __builtin_unreachable and neither, and carried a GCC 4.6 pessimisation
/// workaround. Both live arms defined LE_UNREACHABLE_CODE identically, and the
/// third answered a compiler that abi.hpp's own #error already rules out.
///                                       (07.08.2026.) (SW port)
#define LE_UNREACHABLE_CODE()                                                                      \
    LE_ASSERT_MSG(false, "This code should not be reached.");                                      \
    __builtin_unreachable()

/// \note The LE_OPTIMIZE_FOR_SIZE/SPEED and LE_FAST_MATH families stood here,
/// over 89 call sites, and expanded to nothing on Clang -- their GCC arm was
/// gated on a version test Clang answers with 42 -- so every golden this project
/// has ever rendered was rendered without them. Deleted 07.08.2026.
///
///   Worth keeping from that: per-translation-unit `-O3`/`-Os` is a size/speed
/// knob the build type already decides, and GCC's own documentation restricts
/// the underlying attribute to debugging. Fast-math is the one that was not
/// merely inert: GCC acts on `optimize("associative-math")` and vectorises float
/// reductions under it, so honouring it on Linux alone would reorder sums macOS
/// never reordered -- and make a golden difference impossible to attribute to
/// the FFT backend it is meant to be measuring.
///                                       (29.07.2026.) (SW port)

/// \note LE_DISABLE_LOOP_UNROLLING and LE_DISABLE_LOOP_VECTORIZATION stood here
/// over 21 loops. Unlike the rest of this file's portability layer these were
/// live on Clang -- `clang loop unroll(disable)` and
/// `vectorize(disable) interleave(disable)` -- so removing them let the
/// vectoriser into loops it had been kept out of since 2013. The goldens are
/// bit-identical across it, which is what says the exclusions were a size and
/// compile-time preference rather than a numerical one.
///                                       (07.08.2026.) (SW port)

#else

#error Unkown compiler

#endif

#define LE_DEFAULT_CASE_UNREACHABLE()                                                              \
    default:                                                                                       \
        LE_UNREACHABLE_CODE();                                                                     \
        break

#endif // platformSpecifics_hpp
