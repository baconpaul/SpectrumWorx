# leConfigurationAndODRHeader.h, applied to our sources and to nothing else.
#
# The header configures a 2016 codebase: LE_CHECKED_BUILD and the NDEBUG policy
# that decides whether the asserts exist, the SSE feature macros, and on Windows
# the API-version and CRT macros that have to be seen before any system header.
# It is force-included rather than #included because those have to be in effect
# before the first #include of a translation unit, not partway down it.
#
# It used to also define LE_IMPL_NAMESPACE_BEGIN, which 47 files used without
# declaring where it came from, so a source that missed the header failed to
# compile -- confusingly, but loudly. That macro is gone (stage 7), and measured
# on 04.08.2026 all 148 of our translation units compile without the header. What
# they would not do is compile the same: missing it now silently turns the
# asserts on or off. tests/checkODRHeaderScope.cmake is what catches that.
#
# It used to be a PUBLIC compile option on sw-dsp, which meant every translation
# unit of every target that links sw-dsp -- JUCE, fmt and clap-wrapper included.
# That produced five separate Windows build failures, none of them in our code
# and none of them naming the cause; see stage 7.5 in
# doc/tech/old/implementation_sequence.md for the list.
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

# Where our own code lives. A source outside these gets neither the header nor
# the warning baseline below.
set(SW_OWN_SOURCE_ROOTS "${CMAKE_SOURCE_DIR}/src" "${CMAKE_SOURCE_DIR}/tests"
                        "${CMAKE_SOURCE_DIR}/tools")

################################################################################
# The warning baseline, and -Werror.
#
# Same reasoning as the force-include and the same mechanism: these are our
# standards for our code, and a dependency compiled to somebody else's standards
# is not misbehaving. Six third-party sites already warn -- rtaudio, rtmidi and
# the VST3 SDK, all arriving through clap-wrapper's CPM fetches -- and none of
# them is ours to patch.
#
# -Wno-unused-parameter: an unused parameter is how this codebase spells "the
# interface has one and this override does not need it", several hundred times.
#
# -Wno-unknown-pragmas: 288 `#pragma warning(...)` lines, all MSVC diagnostic
# control, which is *meant* to be inert off MSVC -- 3772 of the 3902 warnings the
# baseline produced. Wrapping each in a macro was the alternative and it buys
# nothing: the one real pragma bug in the tree was a `push` where a `pop` was
# meant (assertionHandler.cpp), and an unknown-pragma warning cannot tell those
# apart either. It was found by reading.
#
# MSVC gets nothing here on purpose. Nobody can run one from this machine --
# Windows arrives as a build log -- and a warning level nobody has ever compiled
# with is a fault injected into somebody else's afternoon. It belongs with the CI
# matrix (doc/tech/todo.md), where the first run is free.
################################################################################

if (MSVC)
    set(swWarningBaseline "")
else ()
    set(swWarningBaseline -Wall -Wextra -Wno-unused-parameter -Wno-unknown-pragmas)
endif ()

# On by default where the development happens, so that a warning is noticed the
# moment it is introduced rather than in a log a week later. Off elsewhere: a new
# compiler version turning up a new warning should not stop somebody's work on a
# platform they did not write it for. CI passes -DSW_WERROR=ON for all of them.
#
# Linux joined macOS on 05.08.2026, once GCC 15.2 built the tree clean: the 849
# warnings it had to say over the Apple Clang baseline were five causes in our
# own code -- restrict and const qualifiers on return types, which are ignored
# there; `LE_ASSUME( &reference )`, which asserts what the language guarantees;
# an unnamed enum meeting an effect index in a conditional; an FFT bin count
# counted in int and compared against a std::size_t; and a knob paint() helper
# hiding the virtual it overloads. Each was worth the edit on its own, which is
# the test of whether a warning belongs in the baseline.
if ((APPLE OR LINUX) AND NOT MSVC)
    set(swWerrorDefault ON)
else ()
    set(swWerrorDefault OFF)
endif ()
option(SW_WERROR "Treat warnings in our own sources as errors" ${swWerrorDefault})

if (SW_WERROR AND NOT MSVC)
    list(APPEND swWarningBaseline -Werror)
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # The two #pragma message build banners -- which backend le/math/vector.cpp
        # compiled and whether the asserts are live -- are information, and
        # -Werror would make each of them fatal. GCC already reports them as notes.
        list(APPEND swWarningBaseline "-Wno-error=#pragma-messages") # quoted: # comments
    endif ()
endif ()

# The sources of `target` that are ours, by path.
function(_sw_our_sources target outSources outDirectory)
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

    set(${outSources} "${ourSources}" PARENT_SCOPE)
    set(${outDirectory} "${targetDirectory}" PARENT_SCOPE)
endfunction()

# Force-include the ODR header into every source of `target` that is ours, and
# compile those sources to our warning standards.
#
# Call it after the last target_sources() for that target. Sources arriving
# through INTERFACE_SOURCES -- which is how JUCE modules and clap-wrapper get
# compiled into their consumers -- are not in the SOURCES property at configure
# time, and would be rejected by the path filter if they were.
function(sw_force_include_odr_header target)
    _sw_our_sources(${target} ourSources targetDirectory)

    if (NOT ourSources)
        message(FATAL_ERROR "sw_force_include_odr_header(${target}): no sources of "
                            "ours -- called before target_sources(), or the target moved "
                            "out of ${SW_OWN_SOURCE_ROOTS}.")
    endif ()

    # DIRECTORY, because source file properties are per directory scope and the
    # target need not live in the one we were called from -- sw-show-ui compiles
    # two sources out of src/.
    set_property(SOURCE ${ourSources} DIRECTORY ${targetDirectory}
                 APPEND PROPERTY COMPILE_OPTIONS
                 ${SW_FORCE_INCLUDE_ODR_HEADER} ${swWarningBaseline})
endfunction()
