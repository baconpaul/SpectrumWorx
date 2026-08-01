# One call assembles the CLAP, the VST3, the AUv2 and the standalone out of
# sw-impl plus swClapEntry.cpp.
#
# Deliberately no project() call here: it would reset PROJECT_VERSION, and
# clap-wrapper reads that for the bundle versions.
#
# SPDX-License-Identifier: GPL-3.0-or-later

set(SW_CLAPFIRST_TARGET spectrumworx_clapfirst)

set(SW_FORMATS CLAP VST3)
if (APPLE)
    list(APPEND SW_FORMATS AUV2)
endif()

message(STATUS "SpectrumWorx plugin formats: ${SW_FORMATS}")

add_compile_definitions(CLAP_WRAPPER_LOGLEVEL=0)

make_clapfirst_plugins(
        TARGET_NAME ${SW_CLAPFIRST_TARGET}
        IMPL_TARGET sw-impl

        OUTPUT_NAME "${PRODUCT_NAME}"

        ENTRY_SOURCE swClapEntry.cpp

        BUNDLE_IDENTIFIER "${SW_CLAP_ID}"
        BUNDLE_VERSION ${PROJECT_VERSION}

        COPY_AFTER_BUILD ${SW_COPY_AFTER_BUILD}

        PLUGIN_FORMATS ${SW_FORMATS}

        WINDOWS_FOLDER_VST3 TRUE

        ASSET_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/sw_assets

        # target-postfix, output name, clap id
        STANDALONE_CONFIGURATIONS
        standalone "${PRODUCT_NAME}" "${SW_CLAP_ID}"
)

# Read back by the installer; clap-wrapper does not set it.
set_target_properties(${SW_CLAPFIRST_TARGET}_all PROPERTIES
        ARTEFACT_DIRECTORY ${CMAKE_BINARY_DIR}/sw_assets)

# \note The standalone's macOS bundle identifier is *not* ours. clap-wrapper
# hardcodes it in its own Info.plist.in as "${MACOSX_BUNDLE_BUNDLE_NAME}
# .standalone" -- so "SpectrumWorx.standalone", a bundle name with a suffix
# rather than a reverse DNS identifier -- and no CMake property overrides that
# one key. The only local remedy is to carry a whole copy of their template,
# which is a fork of a third-party file to change one line. Fix it upstream
# instead; it matters at notarisation and nowhere before.
