# TEMPORARY SCAFFOLD — deleted in stage 7.
#
# Boost is not a dependency of SpectrumWorx. Three header-only libraries are
# all that survived the stage 2 sweep, and all three are load bearing for one
# thing only: the compile-time parameter system in le/parameters, which stage 7
# replaces with C++20 templates.
#
#   Boost.Fusion        parameter tuples, Parameters::for_each
#   Boost.MPL           the trait maps and mpl::string display names
#   Boost.Preprocessor  the DEFINE_PARAMETERS macro family
#   Boost.Intrusive     circular_list_algorithms, the module chain's list
#                       walker — the odd one out, and stage 5's problem
#
# It is fetched rather than vendored — no submodule, no libs/ entry — so that
# deleting this file and the two lines that include it removes Boost from the
# project entirely. If you find yourself reaching for a fourth Boost library,
# that is a signal to bring stage 7 forward, not to widen this list.
#
# See doc/tech/implementation_sequence.md, stages 3.2 and 7.
#
# SPDX-License-Identifier: GPL-3.0-or-later

include(${CMAKE_SOURCE_DIR}/libs/sst/sst-basic-blocks/cmake/CPM.cmake)

set(BOOST_INCLUDE_LIBRARIES preprocessor)
set(BOOST_ENABLE_CMAKE ON)
set(BOOST_SKIP_INSTALL_RULES ON)

CPMAddPackage(
        NAME Boost
        VERSION 1.91.0
        URL https://github.com/boostorg/boost/releases/download/boost-1.91.0-1/boost-1.91.0-1-cmake.tar.xz
        URL_HASH SHA256=cc5dc5006ecbdf0051f90979be31b4eee5987d9ae14ae9fb9c03cfa43fa3cdad
        EXCLUDE_FROM_ALL TRUE
        SYSTEM TRUE
)

# One target to link, so the grep for what still needs Boost is a single name.
add_library(sw-boost-scaffold INTERFACE)
target_link_libraries(sw-boost-scaffold INTERFACE
        Boost::preprocessor
)
