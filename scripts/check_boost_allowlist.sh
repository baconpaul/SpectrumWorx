#!/usr/bin/env bash
#
# Fails if src/ grows a Boost include outside the allowlist.
#
# Stage 7 finished the parameter system, and with it Boost stopped being a
# dependency of SpectrumWorx: cmake/temporary-boost.cmake is gone, nothing is
# fetched, and nothing that compiles includes a Boost header. What is left is
# deliberate, unreachable in this build, and each entry has an owner stage:
#
#   simd, dispatch, type_traits  the NT2 vector backend, and the restrict
#                                fix-ups it brings its own Boost for. Guarded by
#                                LE_HAS_NT2 / LE_MATH_USE_NT2, which this build
#                                does not define        -> stage 4 (tier 3b)
#
# Gone since this list was last written: mmap, which stage 8.0 took out of the
# preset reader. Before it: fusion, mpl and preprocessor -- the parameter
# system -- and intrusive, the module chain's circular_list_algorithms.
#
# Not scanned, all reachable only from src/legacy-build.cmake and named by no
# live target: src/nt2_static_fft, which is vendored NT2;
# le/build/precompiledHeaders.hpp and le/build/juceIncludeWrapper.hpp; and
# spectrumWorx.cpp, the 2016 VST2/AU plugin class the CLAP replaced, retained as
# the reference 5.0 still needs and holding the last boost::mmap in the tree.
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
