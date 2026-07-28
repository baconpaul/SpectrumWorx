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
