# leConfigurationAndODRHeader.h, applied to our sources and to nothing else.
#
# The header configures a 2016 codebase: LE_IMPL_NAMESPACE_BEGIN, LE_CHECKED_BUILD,
# the SSE feature macros, and on MSVC a set of CRT macros that have to be seen
# before any system header. Our headers assume all of it, and missing it does not
# fail cleanly -- LE_IMPL_NAMESPACE_BEGIN( Math ) then parses as a variable
# declaration, and le/math alone produces thirty-odd errors that name everything
# except the cause. So it is force-included rather than #included.
#
# It used to be a PUBLIC compile option on sw-dsp, which meant every translation
# unit of every target that links sw-dsp -- JUCE, fmt and clap-wrapper included.
# That produced five separate Windows build failures, none of them in our code
# and none of them naming the cause; see stage 7.5 in
# doc/tech/implementation_sequence.md for the list.
#
# Two things make a target-wide option the wrong tool here even when it is
# PRIVATE:
#
#   - Linking a JUCE module adds that module's own sources to the *consuming*
#     target, so a PRIVATE option on sw-dsp still lands on juce_graphics's
#     Sheenbidi, which is C.
#   - $<COMPILE_LANGUAGE:CXX> does not restrict compile options under the Visual
#     Studio generator, so the obvious guard is not one.
#
# Hence per source file, and hence the path filter below: membership is decided
# by where a file lives, which is a fact CMake cannot surprise us about.

if (MSVC)
    # MSVC wants the path attached; everything else takes two arguments. Given
    # the latter, MSVC took the header for a source file and reported that it
    # could not open it.
    set(SW_FORCE_INCLUDE_ODR_HEADER
            "/FI${CMAKE_SOURCE_DIR}/src/le/build/leConfigurationAndODRHeader.h")
else ()
    set(SW_FORCE_INCLUDE_ODR_HEADER
            "-include" "${CMAKE_SOURCE_DIR}/src/le/build/leConfigurationAndODRHeader.h")
endif ()

# Where our own code lives. A source outside these never gets the header.
set(SW_OWN_SOURCE_ROOTS "${CMAKE_SOURCE_DIR}/src" "${CMAKE_SOURCE_DIR}/tests"
                        "${CMAKE_SOURCE_DIR}/tools")

# Force-include the ODR header into every source of `target` that is ours.
#
# Call it after the last target_sources() for that target. Sources arriving
# through INTERFACE_SOURCES -- which is how JUCE modules and clap-wrapper get
# compiled into their consumers -- are not in the SOURCES property at configure
# time, and would be rejected by the path filter if they were.
function(sw_force_include_odr_header target)
    get_target_property(sources ${target} SOURCES)
    get_target_property(targetDirectory ${target} SOURCE_DIR)

    set(ourSources "")
    foreach (source IN LISTS sources)
        # Generator expressions and $<TARGET_OBJECTS:...> have no path to test.
        if (source MATCHES "\\$<")
            continue()
        endif ()
        if (NOT IS_ABSOLUTE "${source}")
            set(source "${targetDirectory}/${source}")
        endif ()
        foreach (root IN LISTS SW_OWN_SOURCE_ROOTS)
            # String prefix rather than a regex: a path is not a pattern, and
            # CMAKE_SOURCE_DIR may well contain regex metacharacters.
            string(FIND "${source}" "${root}/" position)
            if (position EQUAL 0)
                list(APPEND ourSources "${source}")
                break()
            endif ()
        endforeach ()
    endforeach ()

    if (NOT ourSources)
        message(FATAL_ERROR "sw_force_include_odr_header(${target}): no sources of "
                            "ours -- called before target_sources(), or the target moved "
                            "out of ${SW_OWN_SOURCE_ROOTS}.")
    endif ()

    # DIRECTORY, because source file properties are per directory scope and the
    # target need not live in the one we were called from -- sw-show-ui compiles
    # two sources out of src/.
    set_property(SOURCE ${ourSources} DIRECTORY ${targetDirectory}
                 APPEND PROPERTY COMPILE_OPTIONS ${SW_FORCE_INCLUDE_ODR_HEADER})
endfunction()
