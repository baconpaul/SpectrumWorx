# sw-tests: unit coverage for sw-dsp.
#
# Stage 3.6 adds the golden fixtures for all 57 effects on top of this. What is
# here now is deliberately weighted towards the code stage 3 wrote or touched:
# the portable arms of the le/math primitives that had none, the buffer and
# stack-buffer replacements, and the effect list that is no longer generated.
#
# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(sw-tests
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

target_link_libraries(sw-tests PRIVATE sw-dsp sw-gui-resources Catch2::Catch2WithMain)

# \note LE_CHECKED_BUILD is deliberately *not* forced here. It decides whether
# NDEBUG survives, and forcing it on for the test objects only would leave them
# calling an assertionFailed() that a release libsw-dsp.a does not define. The
# test runner follows CMAKE_BUILD_TYPE like everything else: Debug runs the
# ~1200 asserts, Release renders the goldens at shipping speed.
target_compile_definitions(sw-tests PRIVATE
        SW_GOLDEN_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/goldens/data")

catch_discover_tests(sw-tests)
