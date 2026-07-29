# sw-tests: unit coverage for sw-dsp.
#
# Stage 3.6 adds the golden fixtures for all 57 effects on top of this. What is
# here now is deliberately weighted towards the code stage 3 wrote or touched:
# the portable arms of the le/math primitives that had none, the buffer and
# stack-buffer replacements, and the effect list that is no longer generated.
#
# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(sw-tests
        clap/parameterModelTests.cpp
        clap/pluginTests.cpp
        effects/effectsListTests.cpp
        goldens/engineHarness.cpp
        goldens/goldenDigest.cpp
        goldens/goldenTests.cpp
        gui/skinTests.cpp
        math/fftTests.cpp
        math/scalarTests.cpp
        math/vectorTests.cpp
        utility/buffersTests.cpp
        utility/spanTests.cpp
)

# sw-impl brings sw-dsp with it, and brings the CLAP entry point the plugin
# tests drive. No dlopen: the tests link the same static library the .clap
# bundle does, so what they exercise is what ships.
target_link_libraries(sw-tests PRIVATE sw-impl sw-gui-resources Catch2::Catch2WithMain)

# \note LE_CHECKED_BUILD is deliberately *not* forced here. It decides whether
# NDEBUG survives, and forcing it on for the test objects only would leave them
# calling an assertionFailed() that a release libsw-dsp.a does not define. The
# test runner follows CMAKE_BUILD_TYPE like everything else: Debug runs the
# ~1200 asserts, Release renders the goldens at shipping speed.
target_compile_definitions(sw-tests PRIVATE
        SW_GOLDEN_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/goldens/data")

# So that clap/ can reach goldens/engineHarness.hpp: the CLAP-side tests need an
# instantiable SpectrumWorxCore, and that is exactly what the golden harness is.
target_include_directories(sw-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

catch_discover_tests(sw-tests)

# Guards the LE_SW_GUI configuration fork for as long as it exists. Compiles
# every sw-dsp source both ways and compares the objects: at LE_SW_GUI=1 four
# setters on Engine::ModuleParameters turn virtual, which in a *release* build
# gives the class a vptr and shifts every member by 8 bytes. Debug builds hide
# it entirely, so a source that quietly becomes configuration-dependent would
# pass every local test and corrupt memory in a shipped build.
#
# ~30s, so it carries the `slow` label: `ctest -LE slow` is the fast loop.
# It goes when the flag does.
find_package(Python3 COMPONENTS Interpreter)
if (Python3_FOUND)
    add_test(NAME gui-flag-parity
            COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_SOURCE_DIR}/scripts/check_gui_flag_parity.py" "${CMAKE_BINARY_DIR}")
    set_tests_properties(gui-flag-parity PROPERTIES LABELS slow TIMEOUT 600)
endif()
