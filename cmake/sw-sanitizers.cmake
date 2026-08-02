# SW_SANITIZER — one of address, thread, realtime, undefined, or empty.
#
# Directory scope, before any target is defined, and on both the compile and the
# link lines: a sanitizer that is not applied to every translation unit and to the
# final link reports things that are artefacts of the mixture rather than of the
# code. That includes the dependencies, which is the point -- JUCE and clap-wrapper
# are on the audio path too.
#
# What each is for, in this tree:
#
#   address   stage 5's heap corruption was found with it in one run, after an
#             afternoon of reading had not. build-asan/ predates this file and is
#             configured from an older CMake; a fresh -D SW_SANITIZER=address is
#             the supported way now.
#   thread    the instrument for the two-instance work. week_two.md §2.2's whole
#             audit is a list of things tsan should have been finding.
#   realtime  the acceptance test for doc/tech/correct_the_threading.md: with the
#             editor open, moving a knob must not allocate, lock or syscall on the
#             audio thread.
#
# RealtimeSanitizer needs a clang that ships the rtsan runtime, and Apple's does
# not -- AppleClang 21 accepts the flag and fails to link. Measured, not assumed:
# Homebrew clang 22.1.6 links it. So
#
#   cmake -B build-rtsan -D SW_SANITIZER=realtime \
#         -D CMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++
#
# SPDX-License-Identifier: GPL-3.0-or-later

set(SW_SANITIZER "" CACHE STRING "Sanitizer to build with: address, thread, realtime, undefined")
set_property(CACHE SW_SANITIZER PROPERTY STRINGS "" address thread realtime undefined)

if (NOT SW_SANITIZER)
    return()
endif()

include(CheckCXXSourceCompiles)

set(swSanitizerFlags "-fsanitize=${SW_SANITIZER}" -fno-omit-frame-pointer)

# check_cxx_compiler_flag() only compiles; a sanitizer that the compiler accepts
# and the runtime library cannot satisfy fails at link, which is exactly the case
# for -fsanitize=realtime on a toolchain without the rtsan runtime. So link.
set(CMAKE_REQUIRED_FLAGS "-fsanitize=${SW_SANITIZER}")
set(CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=${SW_SANITIZER}")
check_cxx_source_compiles("int main() { return 0; }" swSanitizerUsable)
unset(CMAKE_REQUIRED_FLAGS)
unset(CMAKE_REQUIRED_LINK_OPTIONS)

if (NOT swSanitizerUsable)
    message(FATAL_ERROR
            "SW_SANITIZER=${SW_SANITIZER} does not link with ${CMAKE_CXX_COMPILER_ID} "
            "${CMAKE_CXX_COMPILER_VERSION}. RealtimeSanitizer in particular needs a clang "
            "that ships the rtsan runtime, which Apple's does not: pass "
            "-D CMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ or equivalent.")
endif()

add_compile_options(${swSanitizerFlags})
add_link_options("-fsanitize=${SW_SANITIZER}")

# A sanitized build that also elides the frames is a sanitized build whose reports
# name the wrong function. Debug already implies -O0; this is for the optimised
# configurations, where realtime in particular is worth running -- an allocation
# the optimiser removes is one the shipping plugin does not make either.
add_compile_options(-g)

message(STATUS "SpectrumWorx: building with -fsanitize=${SW_SANITIZER}")
