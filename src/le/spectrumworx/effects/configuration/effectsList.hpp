////////////////////////////////////////////////////////////////////////////////
///
/// \file effectsList.hpp
/// ---------------
///
/// The single source of truth for which effects exist, in which order, and in
/// which menu group. **The order is ABI**: presets and host automation refer to
/// effects by index, so entries may be appended but never reordered or removed.
///
/// This used to be effectsList.cmake, which string-concatenated nine headers
/// through configure_file() into the source tree. The "edition" mechanism it
/// existed for -- shipping a subset of the effects per SKU -- went with the
/// licence manager, so every effect is now always built.
///
/// Consumers define a four-argument macro and expand LE_SW_EFFECT_LIST over it:
///
/// \code
///     #define LE_SW_AUX_IMPL(folder, module, name, group) name##Impl,
///     using EffectImpls = std::tuple<LE_SW_EFFECT_LIST(LE_SW_AUX_IMPL) void>;
///     #undef LE_SW_AUX_IMPL
/// \endcode
///
/// Copyright (c) 2011 - 2016. Little Endian Ltd.
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef effectsList_hpp__F3A21C64_9E17_4A0B_8D55_1E7C0B94A2D6
#define effectsList_hpp__F3A21C64_9E17_4A0B_8D55_1E7C0B94A2D6
//------------------------------------------------------------------------------

/// \brief x( folder, module, EffectName, GroupName )
///
/// \note One entry per line, and clang-format is told to leave it that way.
/// This is not only about reading it: tools/show-ui/CMakeLists.txt parses this
/// table to register one UI-render test per effect, and its regex matches the
/// first x(...) after a newline. Reflowed into a staircase with three entries
/// on some lines and one broken across two others -- which is exactly what
/// clang-format 21.1.5 does to it -- the parse silently drops to 40 of the 57
/// and the suite loses seventeen tests without failing. It did, on 05.08.2026.
/// WhitespaceSensitiveMacros does not cover this, because that governs *uses*
/// of a macro and the table is a definition; the off/on pair is what does.
// clang-format off
#define LE_SW_EFFECT_LIST(x)                                                          \
    /* Pitch */                                                                        \
    x(pitch_shifter,           pitchShifter,          PitchShifter,          Pitch)    \
    x(pitch_follower,          pitchFollower,         PitchFollower,         Pitch)    \
    x(tune_worx,               tuneWorx,              TuneWorx,              Pitch)    \
    x(pitch_magnet,            pitchMagnet,           PitchMagnet,           Pitch)    \
    x(sumo_pitch,              sumoPitch,             SumoPitch,             Pitch)    \
    x(pitch_spring,            pitchSpring,           PitchSpring,           Pitch)    \
    x(octaver,                 octaver,               Octaver,               Pitch)    \
                                                                                       \
    /* Timbre */                                                                       \
    x(bandpass,                bandpass,              Bandpass,              Timbre)   \
    x(bandpass,                bandpass,              Bandstop,              Timbre)   \
    x(ah_ah,                   ahAh,                  AhAh,                  Timbre)   \
    x(smoother,                smoother,              Smoother,              Timbre)   \
    x(sharper,                 sharper,               Sharper,               Timbre)   \
    x(centroid_extractor,      centroidExtractor,     CentroidExtractor,     Timbre)   \
    x(tonal,                   tonal,                 Tonal,                 Timbre)   \
    x(tonal,                   tonal,                 Atonal,                Timbre)   \
                                                                                       \
    /* Time */                                                                         \
    x(freeze,                  freeze,                Freeze,                Time)     \
    x(slicer,                  slicer,                Slicer,                Time)     \
    x(wobbler,                 wobbler,               Wobbler,               Time)     \
    x(reverser,                reverser,              Reverser,              Time)     \
    x(eximploder,              exImploder,            Imploder,              Time)     \
    x(eximploder,              exImploder,            Exploder,              Time)     \
                                                                                       \
    /* Space */                                                                        \
    x(frecho,                  frecho,                Frecho,                Space)    \
    x(frecho,                  frecho,                Frevcho,               Space)    \
    x(freqverb,                freqverb,              Freqverb,              Space)    \
                                                                                       \
    /* Phase */                                                                        \
    x(robotizer,               robotizer,             Robotizer,             Phase)    \
    x(whisperer,               whisperer,             Whisperer,             Phase)    \
    x(phasevolution,           phasevolution,         Phasevolution,         Phase)    \
    x(phlip,                   phlip,                 Phlip,                 Phase)    \
                                                                                       \
    /* Loudness */                                                                     \
    x(gain,                    gain,                  Gain,                  Loudness) \
    x(exaggerator,             exaggerator,           Exaggerator,           Loudness) \
    x(denoiser,                denoiser,              Denoiser,              Loudness) \
    x(quiet_boost,             quietBoost,            QuietBoost,            Loudness) \
    x(freqnamics,              freqnamics,            Freqnamics,            Loudness) \
                                                                                       \
    /* Combine */                                                                      \
    x(talking_wind,            talkingWind,           TalkingWind,           Combine)  \
    x(convolver,               convolver,             Convolver,             Combine)  \
    x(ethereal,                ethereal,              Ethereal,              Combine)  \
    x(vaxateer,                vaxateer,              Vaxateer,              Combine)  \
    x(shapeless,               shapeless,             Shapeless,             Combine)  \
    x(colorifer,               colorifer,             Colorifer,             Combine)  \
    x(merger,                  merger,                Merger,                Combine)  \
    x(blender,                 blender,               Blender,               Combine)  \
    x(inserter,                inserter,              Inserter,              Combine)  \
    x(burrito,                 burrito,               Burrito,               Combine)  \
                                                                                       \
    /* PV Domain */                                                                    \
    x(phase_vocoder_analysis,  phaseVocoderAnalysis,  PhaseVocoderAnalysis,  PVD)      \
    x(pitch_shifter,           pitchShifter,          PVPitchShifter,        PVD)      \
    x(pitch_follower,          pitchFollower,         PitchFollowerPVD,      PVD)      \
    x(tune_worx,               tuneWorx,              TuneWorxPVD,           PVD)      \
    x(pitch_magnet,            pitchMagnet,           PitchMagnetPVD,        PVD)      \
    x(pitch_spring,            pitchSpring,           PitchSpringPVD,        PVD)      \
    x(eximploder,              exImploder,            PVImploder,            PVD)      \
    x(eximploder,              exImploder,            PVExploder,            PVD)      \
    x(phase_vocoder_synthesis, phaseVocoderSynthesis, PhaseVocoderSynthesis, PVD)      \
                                                                                       \
    /* Misc */                                                                         \
    x(armonizer,               armonizer,             Armonizer,             Misc)     \
    x(slew_limiter,            slewLimiter,           SlewLimiter,           Misc)     \
    x(shifter,                 shifter,               Shifter,               Misc)     \
    x(swappah,                 swappah,               Swappah,               Misc)     \
    x(quantizer,               quantizer,             Quantizer,             Misc)
// clang-format on

#define LE_SW_NUMBER_OF_EFFECTS 57
#define LE_SW_NUMBER_OF_EFFECT_GROUPS 9

//------------------------------------------------------------------------------
#endif // effectsList_hpp
