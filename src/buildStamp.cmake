# sw-build-stamp: the one translation unit that is stale the moment it is
# compiled, and is therefore rewritten before every build.
#
# configure_file() is the usual way to get a version string into C++, and it is
# the wrong tool here: it runs when CMake configures, so the "build date" it
# produces is the date somebody last touched a CMakeLists. That is exactly the
# failure mode this exists to close -- a plugin that reports a stamp from last
# week is worse than one that reports none, because it is believed.
#
# So the substitution runs from a separate `cmake -P` process
# (cmake/writeBuildStamp.cmake) driven by a custom target, and a custom target is
# always out of date. The generated .cpp is that target's BYPRODUCT and this
# library's only source, so the chain per build is: refresh writes the file, the
# file's contents changed (the clock moved), the object recompiles, everything
# above it relinks.
#
# That relink is the price and it is charged on every build, including one with
# nothing else to do. Keeping it to one tiny target is what keeps the price to a
# single compile and the link steps: nothing else recompiles.
#
# \note Deliberately not sw_force_include_odr_header()ed. That function filters
# by path and this source is in the build tree, so it would find nothing of ours
# and fail; tests/checkODRHeaderScope.cmake filters by the same paths and so does
# not ask for it either. The file needs nothing from the header -- three pointer
# definitions and no system include -- which is the standard this tree holds an
# exemption to (see `exempt` in that script).
#
# SPDX-License-Identifier: GPL-3.0-or-later

set(swBuildStampSource "${CMAKE_BINARY_DIR}/gen/sw/buildStamp.cpp")

set(swWriteBuildStamp
        "${CMAKE_COMMAND}"
        -D "TEMPLATE=${CMAKE_CURRENT_SOURCE_DIR}/configuration/buildStamp.cpp.in"
        -D "OUTPUT=${swBuildStampSource}"
        -D "SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_SOURCE_DIR}/cmake/writeBuildStamp.cmake")

# Once here as well, so that the file exists before any generator looks at it.
# The custom target below overwrites it a moment later; what this buys is that
# the source is never merely promised.
execute_process(COMMAND ${swWriteBuildStamp} COMMAND_ERROR_IS_FATAL ANY)

add_custom_target(sw-build-stamp-refresh
        COMMAND ${swWriteBuildStamp}
        BYPRODUCTS "${swBuildStampSource}"
        COMMENT "Stamping the build date, time and commit"
        VERBATIM)

add_library(sw-build-stamp STATIC "${swBuildStampSource}")
add_dependencies(sw-build-stamp sw-build-stamp-refresh)

# src/, so that the generated source can say #include "configuration/buildStamp.hpp"
# the way a source in the tree would.
target_include_directories(sw-build-stamp PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
