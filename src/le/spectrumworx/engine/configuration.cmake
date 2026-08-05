################################################################################
#
# LEL.Engine configuration.cmake
#
# Copyright (c) 2014 - 2015. Little Endian Ltd.
# SPDX-License-Identifier: GPL-3.0-or-later
#
#
# **Record, not build.** Nothing includes this file, and nothing may -- it is
# reachable only from src/legacy-build.cmake, which nothing includes either. It
# is the 2016 build's own account of which engine features were switchable and
# what they defaulted to, and it is worth keeping for exactly that.
#
# A definition here reaches no compiler, and this file is where stage 7 found out
# what that costs: LE_SW_ENGINE_INPUT_MODE and LE_SW_ENGINE_WINDOW_PRESUM were
# set here and tested with #if in thirty-odd compiled translation units, where an
# undefined name reads 0 in silence. Both have been settled -- see src/dsp.cmake,
# which is a file the build reads.
#
################################################################################

set( LE_SW_ENGINE_SIDE_CHANNEL true CACHE BOOL "include side chaining functionality" )
#...mrmlj...legacy LE_SW_DISABLE_SIDE_CHANNEL variable...
#LE_configureFeatureOption( LE_SW_ENGINE_SIDE_CHANNEL )
mark_as_advanced( LE_SW_ENGINE_SIDE_CHANNEL )
if ( NOT LE_SW_ENGINE_SIDE_CHANNEL )
    add_definitions( -DLE_SW_DISABLE_SIDE_CHANNEL )
endif()

set( LE_SW_ENGINE_INPUT_MODE full CACHE STRING "I/O mode configurability" )
set_property( CACHE LE_SW_ENGINE_INPUT_MODE PROPERTY STRINGS full read-only disabled )
mark_as_advanced( LE_SW_ENGINE_INPUT_MODE )
if ( LE_SW_ENGINE_INPUT_MODE STREQUAL full )
    add_definitions( -DLE_SW_ENGINE_INPUT_MODE=2 )
elseif()
    add_definitions( -DLE_SW_ENGINE_INPUT_MODE=1 )
else()
    add_definitions( -DLE_SW_ENGINE_INPUT_MODE=0 )
endif()

set( LE_SW_ENGINE_WINDOW_PRESUM false CACHE BOOL "enable \"window presum\" capability" )
LE_configureFeatureOption( LE_SW_ENGINE_WINDOW_PRESUM )
