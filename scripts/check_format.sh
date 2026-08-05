#!/usr/bin/env bash
#
# Fails if any of our own sources is not what clang-format would write.
#
# **The version is part of the check.** clang-format's defaults move between
# releases, so "the tree is clean" is only true against a stated one -- a newer
# major re-lays-out files nobody touched, and the gate then fails on somebody
# else's afternoon for a change they did not make. .clang-format names the
# version, this enforces it, and CI installs it by number rather than taking the
# runner's.
#
# Pass --fix to write the changes instead of reporting them.
#
# The file list is `git ls-files`, not a find: what is checked is what is
# committed, so a build tree or an untracked scratch file cannot fail the gate.
# libs/ is left out for free -- those are submodules and git does not list
# through them.
#
# SPDX-License-Identifier: GPL-3.0-or-later

set -uo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Must match the version named in .clang-format.
expected="21.1.5"

clangFormat="${CLANG_FORMAT:-clang-format}"

if ! command -v "${clangFormat}" > /dev/null; then
    echo "${clangFormat} not found. Set CLANG_FORMAT to the ${expected} binary." >&2
    exit 1
fi

found="$("${clangFormat}" --version | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1)"

if [ "${found}" != "${expected}" ]; then
    echo "clang-format ${found}, but this tree is formatted with ${expected}." >&2
    echo "Set CLANG_FORMAT to a ${expected} binary, or update .clang-format," >&2
    echo "scripts/$(basename "${BASH_SOURCE[0]}") and the tree together." >&2
    exit 1
fi

# .m files are MATLAB prototypes rather than Objective-C; .clang-format-ignore
# is what keeps clang-format off them, and it is read from the file's own
# directory upwards, so it applies however they are named on the command line.
#
# Read in a loop rather than with mapfile, which macOS's bash 3.2 does not have
# -- and macOS is where this is run by hand.
sources=()
while IFS= read -r source; do
    sources+=("${source}")
done < <(
    git -C "${root}" ls-files src tests tools |
        grep -E '\.(c|cc|cpp|h|hpp|inl|m|mm)$'
)

if [ "${#sources[@]}" -eq 0 ]; then
    echo "No sources found -- is this a checkout?" >&2
    exit 1
fi

if [ "${1:-}" = "--fix" ]; then
    (cd "${root}" && "${clangFormat}" -i "${sources[@]}")
    echo "Formatted ${#sources[@]} files with clang-format ${found}."
    exit 0
fi

if ! (cd "${root}" && "${clangFormat}" --dry-run -Werror "${sources[@]}"); then
    echo >&2
    echo "Run scripts/$(basename "${BASH_SOURCE[0]}") --fix" >&2
    exit 1
fi

echo "Formatting: clean (${#sources[@]} files, clang-format ${found})."
