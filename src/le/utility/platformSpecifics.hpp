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

#if defined(_MSC_VER) && !defined(__clang__)

#define LE_NOVTABLE __declspec(novtable)

#define LE_ALIGN(alignment) __declspec(align(alignment))

#define LE_FORCEINLINE __forceinline
#define LE_NOINLINE __declspec(noinline)
#define LE_HOT
#define LE_COLD

#define LE_WEAK_SYMBOL __declspec(selectany)
#define LE_WEAK_SYMBOL_CONST __declspec(selectany) extern

#define LE_UNREACHABLE_CODE()                                                                      \
    LE_ASSERT_MSG(false, "This code should not be reached.");                                      \
    __assume(false)

// https://msdn.microsoft.com/en-us/library/hh923901.aspx
#define LE_DISABLE_LOOP_VECTORIZATION() __pragma(loop(no_vector))
#define LE_DISABLE_LOOP_UNROLLING()

#elif defined(__GNUC__)

//#define LE_NOVTABLE __declspec( novtable )...-fms-extensions
#define LE_NOVTABLE

#define LE_ALIGN(alignment) __attribute__((aligned(alignment)))

#ifdef _DEBUG
#define LE_FORCEINLINE inline
#else
#define LE_FORCEINLINE __attribute__((always_inline)) inline
#endif
#define LE_NOINLINE __attribute__((noinline))

// http://lists.cs.uiuc.edu/pipermail/cfe-commits/Week-of-Mon-20120507/057599.html
// http://clang-developers.42468.n3.nabble.com/PROPOSAL-per-function-optimization-level-control-td4031670.html
// https://gcc.gnu.org/onlinedocs/gcc/ARM-Function-Attributes.html
#define LE_HOT __attribute__((hot))
//...mrmlj...disable 'cold' until we split cold and minsize (as cold can slow down calling code)...
#if defined(__clang__)
#define LE_COLD __attribute__((/*cold,*/ minsize))
#else
/// \note `minsize` is a Clang attribute; GCC has no such thing and warned
/// "'minsize' attribute directive ignored" at every one of the several hundred
/// `LE_COLD` sites. It was therefore already a no-op there — but not an inert
/// one: GCC rejects a GNU attribute in the trailing declarator position of a
/// function *definition*, which is how `le/plugins/clap/tag.hpp` writes it, so
/// the macro had to expand to nothing rather than to something ignorable.
///
///   `__attribute__((cold))` is the GCC equivalent in spirit and is deliberately
/// *not* used: the comment above records that it was switched off on purpose,
/// because marking a callee cold pessimises the call sites too. If per-function
/// size optimisation is wanted back on GCC it needs `optimize("Os")`, which
/// GCC's own documentation restricts to debugging.
///                                           (29.07.2026.) (SW port)
#define LE_COLD
#endif

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

// http://llvm.org/docs/Vectorizers.html#pragma-loop-hint-directives
#define LE_DISABLE_LOOP_VECTORIZATION()                                                            \
    _Pragma("clang loop vectorize( disable ) interleave( disable )")
#define LE_DISABLE_LOOP_UNROLLING() _Pragma("clang loop unroll( disable )")

#else

#error Unkown compiler

#endif

#define LE_DEFAULT_CASE_UNREACHABLE()                                                              \
    default:                                                                                       \
        LE_UNREACHABLE_CODE();                                                                     \
        break

#endif // platformSpecifics_hpp
