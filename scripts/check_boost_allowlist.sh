#!/usr/bin/env bash
#
# Fails if src/ grows a Boost include. There is no allowlist any more.
#
# Stage 7 finished the parameter system and with it Boost stopped being a
# dependency: cmake/temporary-boost.cmake is gone, nothing is fetched, and
# nothing that compiles includes a Boost header. Three entries outlived that as
# deliberate exceptions -- simd, dispatch and type_traits, all belonging to the
# NT2 vector backend -- and on 07.08.2026 the NT2 arm was deleted from
# le/math/vector.cpp and the fix-ups from le/utility/tchar.hpp. So the list is
# empty, and the check is now simply "no Boost".
#
# Gone before that: mmap, which stage 8.0 took out of the preset reader; fusion,
# mpl and preprocessor -- the parameter system -- and intrusive, the module
# chain's circular_list_algorithms.
#
# Not scanned, and named by no target: spectrumWorx.cpp, the 2016 VST2/AU plugin
# class the CLAP replaced, retained as the reference 5.0 still needs and holding
# the last boost::mmap in the tree.
#
# \note There is deliberately no `grep -Ev "${allowed}"` stage here. An empty
# allowlist through that filter is `grep -Ev ''`, which matches every line and
# inverts to nothing -- so the check would pass for ever, including on a
# genuinely new Boost include. An empty list has to mean no filter, not an empty
# pattern.
#
set -uo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

offenders="$(
    grep -rEIho '#[[:space:]]*include[[:space:]]*[<"]boost/[^">]+' "${root}/src" \
        --include='*.h' --include='*.hpp' --include='*.inl' \
        --include='*.c' --include='*.cpp' --include='*.mm' \
        --include='*.hpp.in' --include='*.cpp.in' \
        --exclude=precompiledHeaders.hpp --exclude=juceIncludeWrapper.hpp \
        --exclude=spectrumWorx.cpp |
        sed -E 's/.*[<"]//' |
        sort -u
)"

if [ -n "${offenders}" ]; then
    echo "Boost includes in src/:" >&2
    echo "${offenders}" | sed 's/^/  /' >&2
    echo >&2
    echo "Boost is no longer a dependency of this project. Add a std replacement" >&2
    echo "instead of reintroducing it." >&2
    exit 1
fi

echo "No Boost in src/."
