#!/usr/bin/env bash
#
# Fails if src/ grows a Boost include outside the allowlist.
#
# Stage 2 removed the tier-1 Boost usage (doc/tech/implementation_sequence.md).
# What is left is deliberate and each entry here has an owner stage:
#
#   fusion, mpl, preprocessor  the parameter system   -> stage 7 (tier 3a)
#   simd, dispatch             NT2 vector backend     -> stage 4 (tier 3b)
#   mmap                       the installer .paths file -> stage 6
#   intrusive                  circular_list_algorithms in the module chain
#   type_traits                restrict fixups, NT2 builds only
#
# src/nt2_static_fft is vendored NT2 and is not scanned.
#
set -uo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

allowed='^boost/(fusion|mpl|preprocessor|simd|dispatch|mmap|intrusive|type_traits)/'

offenders="$(
    grep -rEIho '#[[:space:]]*include[[:space:]]*[<"]boost/[^">]+' "${root}/src" \
        --include='*.h' --include='*.hpp' --include='*.inl' \
        --include='*.c' --include='*.cpp' --include='*.mm' \
        --include='*.hpp.in' --include='*.cpp.in' \
        --exclude-dir=nt2_static_fft |
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
