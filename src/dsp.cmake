# sw-dsp: the engine, the effects and everything they need. No host.
#
# This is the target the golden tests run against, and the one stage 4 swaps the
# SIMD/FFT backend underneath.
#
# \note It used to say "no GUI", and forbid JUCE, by way of LE_SW_GUI=0. That
# macro is gone: it decided whether four setters on Engine::ModuleParameters were
# virtual, which in a release build decided the layout of every module object, so
# the engine had two ABIs and only the debug ones matched. The GUI configuration
# is the one the plugin needs, the UI reference that forces the virtuals is an
# artefact this design will lose to a two-queue model anyway, and one engine that
# is always right beats two that agree only in debug.
#
# The consequence is that JUCE is now on this target's include path, and the
# module widget set is on its link line. Nothing here may include clap or
# src/core/host_interop.
#
# SPDX-License-Identifier: GPL-3.0-or-later

add_library(sw-dsp STATIC
        # le/math
        le/math/conversion.cpp
        le/math/math.cpp
        le/math/vector.cpp
        le/math/windows.cpp
        le/math/dft/domainConversion.cpp
        le/math/dft/fft.cpp

        # le/analysis
        le/analysis/musical_scales/musicalScales.cpp
        le/analysis/peak_detector/peakDetector.cpp
        le/analysis/pitch_detector/pitchDetector.cpp

        # le/parameters
        le/parameters/implDetails.cpp
        le/parameters/lfoImpl.cpp
        le/parameters/trigger/parameter.cpp

        # le/utility
        le/utility/assertionHandler.cpp
        le/utility/filesystem.cpp
        le/utility/lexicalCast.cpp
        le/utility/trace.cpp

        # le/spectrumworx -- the preset format, the one file that opens files,
        # and the factory banks that are in the binary rather than either
        le/spectrumworx/factoryPresets.cpp
        le/spectrumworx/presetFile.cpp
        le/spectrumworx/presets.cpp

        # le/spectrumworx/engine
        le/spectrumworx/engine/automatableParameters.cpp
        le/spectrumworx/engine/channelBuffers.cpp
        le/spectrumworx/engine/channelData.cpp
        le/spectrumworx/engine/channelDataAmPh.cpp
        le/spectrumworx/engine/channelDataReIm.cpp
        le/spectrumworx/engine/module.cpp
        le/spectrumworx/engine/moduleChainImpl.cpp
        le/spectrumworx/engine/moduleParameters.cpp
        le/spectrumworx/engine/parameters.cpp
        le/spectrumworx/engine/processor.cpp
        le/spectrumworx/engine/setup.cpp

        # le/spectrumworx/effects — shared
        # \note The two *UIElements.cpp are named for the GUI but hold the
        # parameter name and enumerated-value strings, and ParameterInfo
        # carries the name because presets serialise parameters by it.
        le/spectrumworx/effects/baseParametersUIElements.cpp
        le/spectrumworx/effects/commonParametersUIElements.cpp
        le/spectrumworx/effects/configuration/effectNames.cpp
        le/spectrumworx/effects/effects.cpp
        le/spectrumworx/effects/historyBuffer.cpp
        le/spectrumworx/effects/indexRange.cpp
        le/spectrumworx/effects/phase_vocoder/shared.cpp
        le/spectrumworx/effects/vibrato.cpp

        # le/spectrumworx/effects — one per shipped effect, per effectsList.hpp
        le/spectrumworx/effects/ah_ah/ahAhImpl.cpp
        le/spectrumworx/effects/armonizer/armonizerImpl.cpp
        le/spectrumworx/effects/bandpass/bandpassImpl.cpp
        le/spectrumworx/effects/blender/blenderImpl.cpp
        le/spectrumworx/effects/burrito/burritoImpl.cpp
        le/spectrumworx/effects/centroid_extractor/centroidExtractorImpl.cpp
        le/spectrumworx/effects/colorifer/coloriferImpl.cpp
        le/spectrumworx/effects/convolver/convolverImpl.cpp
        le/spectrumworx/effects/denoiser/denoiserImpl.cpp
        le/spectrumworx/effects/ethereal/etherealImpl.cpp
        le/spectrumworx/effects/exaggerator/exaggeratorImpl.cpp
        le/spectrumworx/effects/eximploder/exImploderImpl.cpp
        le/spectrumworx/effects/frecho/frechoImpl.cpp
        le/spectrumworx/effects/freeze/freezeImpl.cpp
        le/spectrumworx/effects/freqnamics/freqnamicsImpl.cpp
        le/spectrumworx/effects/freqverb/freqverbImpl.cpp
        le/spectrumworx/effects/gain/gainImpl.cpp
        le/spectrumworx/effects/inserter/inserterImpl.cpp
        le/spectrumworx/effects/merger/mergerImpl.cpp
        le/spectrumworx/effects/octaver/octaverImpl.cpp
        le/spectrumworx/effects/phase_vocoder_analysis/phaseVocoderAnalysisImpl.cpp
        le/spectrumworx/effects/phase_vocoder_synthesis/phaseVocoderSynthesisImpl.cpp
        le/spectrumworx/effects/phasevolution/phasevolutionImpl.cpp
        le/spectrumworx/effects/phlip/phlipImpl.cpp
        le/spectrumworx/effects/pitch_follower/pitchFollowerImpl.cpp
        le/spectrumworx/effects/pitch_magnet/pitchMagnetImpl.cpp
        le/spectrumworx/effects/pitch_shifter/pitchShifterImpl.cpp
        le/spectrumworx/effects/pitch_spring/pitchSpringImpl.cpp
        le/spectrumworx/effects/quantizer/quantizerImpl.cpp
        le/spectrumworx/effects/quiet_boost/quietBoostImpl.cpp
        le/spectrumworx/effects/reverser/reverserImpl.cpp
        le/spectrumworx/effects/robotizer/robotizerImpl.cpp
        le/spectrumworx/effects/shapeless/shapelessImpl.cpp
        le/spectrumworx/effects/sharper/sharperImpl.cpp
        le/spectrumworx/effects/shifter/shifterImpl.cpp
        le/spectrumworx/effects/slew_limiter/slewLimiterImpl.cpp
        le/spectrumworx/effects/slicer/slicerImpl.cpp
        le/spectrumworx/effects/smoother/smootherImpl.cpp
        le/spectrumworx/effects/sumo_pitch/sumoPitchImpl.cpp
        le/spectrumworx/effects/swappah/swappahImpl.cpp
        le/spectrumworx/effects/talking_wind/talkingWindImpl.cpp
        le/spectrumworx/effects/tonal/tonalImpl.cpp
        le/spectrumworx/effects/tune_worx/tuneWorxImpl.cpp
        le/spectrumworx/effects/vaxateer/vaxateerImpl.cpp
        le/spectrumworx/effects/whisperer/whispererImpl.cpp
        le/spectrumworx/effects/wobbler/wobblerImpl.cpp

        # core — the module chain, minus its host and GUI halves
        core/automatedModuleChain.cpp
        core/spectrumWorxCore.cpp
        core/modules/automatedModule.cpp
        core/modules/factory.cpp
)

# configure_file into the build tree, never back into src/ — that is what the
# 2016 build did and what stage 3.4 removed.
set(swGeneratedIncludeDir "${CMAKE_CURRENT_BINARY_DIR}/geninclude")
set(versionMajor ${PROJECT_VERSION_MAJOR})
set(versionMinor ${PROJECT_VERSION_MINOR})
set(versionPatch ${PROJECT_VERSION_PATCH})
set(versionDescription "${GIT_IMPLIED_DISPLAY_VERSION}")
set(fullVersionString "${PROJECT_VERSION}")
set(editionString "") # editions went with the licence manager
configure_file(
        configuration/versionConfiguration.hpp.in
        "${swGeneratedIncludeDir}/configuration/versionConfiguration.hpp"
        @ONLY
)

target_include_directories(sw-dsp PUBLIC . "${swGeneratedIncludeDir}")

# The 2016 build force-included this and every header assumes it. Ours only --
# see cmake/sw-odr-header.cmake for why a target-wide option cannot express that,
# PUBLIC or PRIVATE.
sw_force_include_odr_header(sw-dsp)

# The preset parser. PUBLIC because presets.hpp holds a TiXmlDocument by value.
#
# \note sst-plugininfra's, not a submodule of our own: it is already configured
# and built (SST_PLUGININFRA_PROVIDE_TINYXML defaults ON), and a second XML
# library to pin and vendor-audit is a poor trade for an API preference. It is
# TinyXML 1, which the plan called tinyxml2 -- the plan was writing from the
# option's name.
target_link_libraries(sw-dsp PUBLIC sst-plugininfra::tinyxml)

# LE::Utility::assertionFailed lives in assertionHandler.cpp; without this the
# asserts degrade to the CRT's and the DAW never sees them.
target_compile_definitions(sw-dsp PUBLIC LE_ENABLE_ASSERT_HANDLER)

# The module class is SW::Module now, which embeds a GUI::ModuleUI, so the
# headers reach JUCE and the skin. PUBLIC: they are in sw-dsp's own headers.
#
# \note The optional<ModuleUI> stays empty until createGUI(), and the per-effect
# widget storage is raw until ModuleWidgets::create(), so a headless test still
# constructs all 57 modules and processes audio without touching JUCE.
target_link_libraries(sw-dsp PUBLIC juce::juce_gui_basics sw-gui-resources)

# The factory banks. PRIVATE: factoryPresets.cpp is the only thing that names
# cmrc, and its header says nothing about where a preset comes from.
target_link_libraries(sw-dsp PRIVATE sw::assets)

# Linking a JUCE module compiles that module's own sources into the consuming
# target, so sw-dsp -- not the shim -- is what builds juce_core.cpp. It therefore
# has to carry the module settings itself: add_clap_juce_shim() puts these on
# clap_juce_shim_requirements and clap_juce_shim, neither of which is on this
# link line.
#
# It shows up as a Linux-only link failure. JUCE's macOS URL backend is
# NSURLSession, its Linux one is libcurl, so juce_core.cpp reached for -lcurl on
# a plugin that does no networking. PUBLIC, so every translation unit that sees
# a JUCE header agrees with the ones the shim compiles.
#
# \note Only the two. The shim also sets JUCE_MODAL_LOOPS_PERMITTED=0,
# JUCE_USE_CAMERA and JUCE_REPORT_APP_USAGE; those would change what compiles in
# gui/, and the preset browser's two async save-path callers (stage 6.4) are
# exactly the code that would notice. Not this commit's business.
target_compile_definitions(sw-dsp PUBLIC JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)

# \note LE_NO_PRESETS stood here. It compiled out ModuleParameters::{load,save}
# PresetParameters -- the whole preset serialisation -- because presets.cpp read
# and wrote preset files through boost::mmap, a library stage 0 deleted, in the
# same translation unit. Stage 8.0 moved the file half to presetFile.cpp and put
# juce::File under it, so the engine can have the serialisation without it.

# No external audio file as a side channel, for now. This is the *file* loader,
# not the host's sidechain port -- that one is live and the CLAP feeds it. The
# only macOS Sample::doLoad is external_audio/sampleMac.cpp over ExtAudioFile
# and FSRef, neither of which builds against a current SDK. Stage 5.0 rewrites
# it over juce::AudioFormatManager, ~50 lines, and this goes.
#
# PUBLIC, and next to LE_NO_PRESETS, for the same reason: both change the layout
# of SpectrumWorxEditor, so every translation unit that sees the header has to
# agree on them.
target_compile_definitions(sw-dsp PUBLIC LE_SW_DISABLE_SIDE_CHANNEL)

if (APPLE)
    # le/math/vector.cpp and le/math/dft/fft.cpp are vDSP/vForce on Apple.
    target_link_libraries(sw-dsp PUBLIC "-framework Accelerate")
else()
    # ...and pffft everywhere else, per le/math/dft/fft.hpp's LE_PFFFT. PRIVATE:
    # fft.hpp forward declares pffft::PFFFT_Setup rather than including pffft.h,
    # so nothing outside fft.cpp needs the include path.
    target_link_libraries(sw-dsp PRIVATE pffft)
endif()
