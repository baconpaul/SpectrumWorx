# Two test binaries, split by what they are allowed to link.
#
#   sw-dsp-tests    the engine, the effects and the preset format. Links sw-dsp
#                   and nothing else -- **no JUCE at all**, which is goal 3 of
#                   doc/tech/correct_the_threading.md and the reason the split
#                   exists. If a case here starts needing juce::File it will not
#                   link, which is the point: the boundary is checked by the
#                   build rather than by a reviewer.
#
#   sw-plugin-tests everything above that -- the CLAP plugin, the editor, the
#                   audio file decoder. Links sw-impl, which brings all of it.
#
# \note One binary, sw-tests, stood here. Splitting it does not add coverage; it
# turns "the engine could be built without JUCE" from a claim into a link.
# checkNoJuceInDSP.cmake says the same thing about the compile lines, and the two
# fail differently: the gate catches a JUCE module appearing on sw-dsp's link
# line, this catches a *test* quietly reaching above the layer it is testing.
#
# SPDX-License-Identifier: GPL-3.0-or-later

################################################################################
# sw-dsp-tests
################################################################################

add_executable(sw-dsp-tests
        core/engineOwnershipTests.cpp
        core/protocolTests.cpp
        effects/amplifyingEffectsTests.cpp
        effects/effectsListTests.cpp
        goldens/engineHarness.cpp
        goldens/goldenDigest.cpp
        goldens/goldenTests.cpp
        math/fftTests.cpp
        math/scalarTests.cpp
        math/vectorTests.cpp
        parameters/parameterLayoutTests.cpp
        parameters/streamingNameTests.cpp
        presets/presetCorpusTests.cpp
        presets/presetHarness.cpp
        presets/presetRenderTests.cpp
        presets/presetReportTests.cpp
        presets/presetRoundTripTests.cpp
        utility/buffersTests.cpp
        utility/circularListTests.cpp
        utility/spanTests.cpp
)

target_link_libraries(sw-dsp-tests PRIVATE sw-dsp Catch2::Catch2WithMain)

################################################################################
# sw-plugin-tests
################################################################################

add_executable(sw-plugin-tests
        clap/engineSetupStalenessTests.cpp
        clap/identityTests.cpp
        clap/parameterModelTests.cpp
        clap/pluginTests.cpp
        clap/stateTests.cpp
        # Drives a real clap_plugin through its C entry point.
        core/threadCheckTests.cpp
        external_audio/sampleTests.cpp
        goldens/engineHarness.cpp # the CLAP cases drive an instantiable core
        # The parameter table is the host-facing enumeration, which lives in
        # sw-impl -- plugin2Host.cpp -- rather than in the engine.
        parameters/parameterTableTests.cpp
        presets/presetHarness.cpp
        gui/pathsTests.cpp
        gui/skinTests.cpp
        gui/twoInstanceTests.cpp
)

# sw-impl brings sw-dsp with it, and brings the CLAP entry point the plugin
# tests drive. No dlopen: the tests link the same static library the .clap
# bundle does, so what they exercise is what ships.
target_link_libraries(sw-plugin-tests PRIVATE sw-impl sw-gui-resources Catch2::Catch2WithMain)

# clapwrapper/{auv2,vst3}.h, for the two sub-factories identityTests.cpp pins.
# sw-impl links this PRIVATE -- it is a header-only interface target and the
# entry stubs get it from clap-wrapper's own side -- so it does not arrive here
# across the link line. clap-helpers is the same shape, and is what makes
# spectrumWorxCLAP.hpp includable: several cases need the class itself rather
# than the C vtable, to reach what a host cannot name.
target_link_libraries(sw-plugin-tests PRIVATE clap-helpers clap-wrapper-extensions)

################################################################################
# What both need
################################################################################

foreach (testTarget sw-dsp-tests sw-plugin-tests)
    # The tests include the engine's headers, so they need what those headers
    # assume just as much as the engine does. It used to arrive across the link
    # line from sw-dsp, along with everything else that linked it.
    sw_force_include_odr_header(${testTarget})

    # \note LE_CHECKED_BUILD is deliberately *not* forced here. It decides
    # whether NDEBUG survives, and forcing it on for the test objects only would
    # leave them calling an assertionFailed() that a release libsw-dsp.a does not
    # define. The test runner follows CMAKE_BUILD_TYPE like everything else:
    # Debug runs the ~1200 asserts, Release renders the goldens at shipping
    # speed.
    target_compile_definitions(${testTarget} PRIVATE
            SW_GOLDEN_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/goldens/data"
            SW_PARAMETER_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/parameters/data"
            SW_PRESET_SNAPSHOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/presets/data"
            # \note The shipping banks themselves, not a copy: the corpus test is
            # about *those* files, and a copy is one more thing that can rot.
            SW_PRESET_DATA_DIR="${CMAKE_SOURCE_DIR}/assets/presets"
            # Where a test that has to write a file writes it. The build tree,
            # not the system temporary directory: it is cleaned with the build,
            # two configurations cannot collide in it, and a sandboxed CI can
            # reach it.
            SW_TEST_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-output")

    # So that clap/ can reach goldens/engineHarness.hpp: the CLAP-side tests need
    # an instantiable SpectrumWorxCore, and that is exactly what the golden
    # harness is.
    target_include_directories(${testTarget} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

    catch_discover_tests(${testTarget})
endforeach ()

################################################################################
# Properties of the build rather than of the code
################################################################################

# Not a Catch2 case: what it checks is a property of the build, not of the code,
# and it has to run against the generated compile lines rather than against
# anything that got compiled.
add_test(NAME odr-header-stays-in-our-sources
        COMMAND ${CMAKE_COMMAND}
        -D "BUILD_DIR=${CMAKE_BINARY_DIR}"
        -D "SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/checkODRHeaderScope.cmake")

# Likewise: the engine must be linkable without JUCE, which is goal 3 of
# doc/tech/correct_the_threading.md. sw-dsp-tests above is the other half of the
# same claim -- this one catches a JUCE module arriving on sw-dsp's link line
# even while nothing includes a JUCE header yet.
add_test(NAME engine-links-no-juce
        COMMAND ${CMAKE_COMMAND}
        -D "BUILD_DIR=${CMAKE_BINARY_DIR}"
        -D "SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/checkNoJuceInDSP.cmake")

# \note check_gui_flag_parity.py was wired in here, to compile every sw-dsp
# source at both settings of LE_SW_GUI and compare the objects. It did its job:
# it kept the two configurations honest for exactly as long as there were two.
# The macro is gone, so the check went with it.
