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
        math/fftTests.cpp
        math/scalarTests.cpp
        math/vectorTests.cpp
        utility/buffersTests.cpp
        utility/spanTests.cpp
)

target_link_libraries(sw-tests PRIVATE sw-dsp Catch2::Catch2WithMain)

# The assert handler routes to LE::Utility::assertionFailed, which
# assertionHandler.cpp defines. A failing assert in a test aborts the runner,
# which is what we want: the checked build is the interesting one.
target_compile_definitions(sw-tests PRIVATE LE_CHECKED_BUILD=1)

catch_discover_tests(sw-tests)
