#!/usr/bin/env bash
#
# Fails if src/ grows a Boost include outside the allowlist.
#
# Stage 7 finished the parameter system, and with it Boost stopped being a
# dependency of SpectrumWorx: cmake/temporary-boost.cmake is gone, nothing is
# fetched, and nothing that compiles includes a Boost header. What is left is
# deliberate, unreachable in this build, and each entry has an owner stage:
#
#   simd, dispatch, type_traits  the NT2 vector backend in le/math/vector.cpp,
#                                and the restrict fix-ups it brings its own
#                                Boost for. Guarded by LE_HAS_NT2 /
#                                LE_MATH_USE_NT2, which this build does not
#                                define -- and could not satisfy if it did,
#                                since Boost.SIMD is not vendored either.
#                                                       -> stage 4 (tier 3b)
#
# Gone since this list was last written: mmap, which stage 8.0 took out of the
# preset reader. Before it: fusion, mpl and preprocessor -- the parameter
# system -- and intrusive, the module chain's circular_list_algorithms.
#
# Not scanned, reachable only from src/legacy-build.cmake and named by no live
# target: spectrumWorx.cpp, the 2016 VST2/AU plugin class the CLAP replaced,
# retained as the reference 5.0 still needs and holding the last boost::mmap in
# the tree.
#
# src/nt2_static_fft -- vendored NT2, the stage 4 reference -- was also on that
# list until 05.08.2026, when it was deleted: stage 4 is done and
# tests/math/fftTests.cpp is the grading harness now.
#
set -uo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

allowed='^boost/(simd|dispatch|type_traits)/'

offenders="$(
    grep -rEIho '#[[:space:]]*include[[:space:]]*[<"]boost/[^">]+' "${root}/src" \
        --include='*.h' --include='*.hpp' --include='*.inl' \
        --include='*.c' --include='*.cpp' --include='*.mm' \
        --include='*.hpp.in' --include='*.cpp.in' \
        --exclude-dir=nt2_static_fft \
        --exclude=precompiledHeaders.hpp --exclude=juceIncludeWrapper.hpp \
        --exclude=spectrumWorx.cpp |
        sed -E 's/.*[<"]//' |
        grep -Ev "${allowed}" |
        sort -u
)"

if [ -n "${offenders}" ]; then
    echo "Boost includes outside the allowlist:" >&2
    echo "${offenders}" | sed 's/^/  /' >&2
    echo >&2
    echo "Add a std replacement instead, or extend the allowlist in $(basename "${BASH_SOURCE[0]}")" >&2
    echo "with the stage that will remove it." >&2
    exit 1
fi

echo "Boost allowlist: clean."
