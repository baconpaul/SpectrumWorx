# sw-tests: unit coverage for sw-dsp.
#
# Stage 3.6 adds the golden fixtures for all 57 effects on top of this. What is
# here now is deliberately weighted towards the code stage 3 wrote or touched:
# the portable arms of the le/math primitives that had none, the buffer and
# stack-buffer replacements, and the effect list that is no longer generated.
#
# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(sw-tests
        clap/engineSetupStalenessTests.cpp
        clap/identityTests.cpp
        clap/parameterModelTests.cpp
        clap/pluginTests.cpp
        clap/stateTests.cpp
        core/processLockTests.cpp
        core/protocolTests.cpp
        core/threadCheckTests.cpp
        effects/amplifyingEffectsTests.cpp
        effects/effectsListTests.cpp
        external_audio/sampleTests.cpp
        goldens/engineHarness.cpp
        goldens/goldenDigest.cpp
        goldens/goldenTests.cpp
        gui/pathsTests.cpp
        gui/skinTests.cpp
        gui/twoInstanceTests.cpp
        math/fftTests.cpp
        math/scalarTests.cpp
        math/vectorTests.cpp
        parameters/parameterLayoutTests.cpp
        parameters/parameterTableTests.cpp
        parameters/streamingNameTests.cpp
        presets/presetCorpusTests.cpp
        presets/presetHarness.cpp
        presets/presetRoundTripTests.cpp
        utility/buffersTests.cpp
        utility/circularListTests.cpp
        utility/spanTests.cpp
)

# The tests include the engine's headers, so they need what those headers assume
# just as much as the engine does. It used to arrive across the link line from
# sw-dsp, along with everything else that linked it.
sw_force_include_odr_header(sw-tests)

# sw-impl brings sw-dsp with it, and brings the CLAP entry point the plugin
# tests drive. No dlopen: the tests link the same static library the .clap
# bundle does, so what they exercise is what ships.
target_link_libraries(sw-tests PRIVATE sw-impl sw-gui-resources Catch2::Catch2WithMain)

# clapwrapper/{auv2,vst3}.h, for the two sub-factories identityTests.cpp pins.
# sw-impl links this PRIVATE -- it is a header-only interface target and the
# entry stubs get it from clap-wrapper's own side -- so it does not arrive here
# across the link line. clap-helpers is the same shape, and is what makes
# spectrumWorxCLAP.hpp includable: processLockTests.cpp needs the class itself,
# not the C vtable, to reach the processing lock a host cannot name.
target_link_libraries(sw-tests PRIVATE clap-helpers clap-wrapper-extensions)

# \note LE_CHECKED_BUILD is deliberately *not* forced here. It decides whether
# NDEBUG survives, and forcing it on for the test objects only would leave them
# calling an assertionFailed() that a release libsw-dsp.a does not define. The
# test runner follows CMAKE_BUILD_TYPE like everything else: Debug runs the
# ~1200 asserts, Release renders the goldens at shipping speed.
target_compile_definitions(sw-tests PRIVATE
        SW_GOLDEN_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/goldens/data"
        SW_PARAMETER_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/parameters/data"
        SW_PRESET_SNAPSHOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/presets/data"
        # \note The shipping banks themselves, not a copy: the corpus test is
        # about *those* files, and a copy is one more thing that can rot.
        SW_PRESET_DATA_DIR="${CMAKE_SOURCE_DIR}/assets/presets"
        # Where a test that has to write a file writes it. The build tree, not
        # the system temporary directory: it is cleaned with the build, two
        # configurations cannot collide in it, and a sandboxed CI can reach it.
        SW_TEST_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-output")

# So that clap/ can reach goldens/engineHarness.hpp: the CLAP-side tests need an
# instantiable SpectrumWorxCore, and that is exactly what the golden harness is.
target_include_directories(sw-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

catch_discover_tests(sw-tests)

# Not a Catch2 case: what it checks is a property of the build, not of the code,
# and it has to run against the generated compile lines rather than against
# anything that got compiled.
add_test(NAME odr-header-stays-in-our-sources
        COMMAND ${CMAKE_COMMAND}
        -D "BUILD_DIR=${CMAKE_BINARY_DIR}"
        -D "SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/checkODRHeaderScope.cmake")

# \note check_gui_flag_parity.py was wired in here, to compile every sw-dsp
# source at both settings of LE_SW_GUI and compare the objects. It did its job:
# it kept the two configurations honest for exactly as long as there were two.
# The macro is gone, so the check went with it.
