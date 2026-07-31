////////////////////////////////////////////////////////////////////////////////
///
/// \file presetFile.hpp
/// --------------------
///
///   Where a preset comes from, and where it goes.
///
///   presets.hpp is the *format*: XML in a writable buffer, in and out of the
/// parameter tree. This header is the only place that opens a file -- which is
/// the separation `LE_NO_PRESETS` stood in for. presets.cpp used to do both,
/// reading and writing through `boost::mmap`, a library stage 0 deleted, in the
/// same translation unit as the serialisation `sw-dsp` needs; so `sw-dsp` could
/// have neither.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef presetFile_hpp__0C4A6D19_8E37_4B52_9A6D_1F73B0C5E842
#define presetFile_hpp__0C4A6D19_8E37_4B52_9A6D_1F73B0C5E842
//------------------------------------------------------------------------------
#include "presets.hpp"

#include <span>
//------------------------------------------------------------------------------
namespace juce
{
class File;
class String;
} // namespace juce
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace SW
{
//------------------------------------------------------------------------------

class Program;

/// \brief Reads a preset file into the writable, NUL-terminated buffer that the
/// destructive parse in Preset::loadFrom() requires.
/// \return an empty pointer if the file cannot be read.
Preset::InMemoryPreset readPresetFile(juce::File const &);

/// \brief Replaces a preset file's contents, creating the file if it is absent.
bool writePresetFile(juce::File const &, char const *data, unsigned int size);

/// \brief Truncating copy of a NUL-terminated name into a Program::Name.
void copyPresetName(char_t const *name, std::span<char> target);

////////////////////////////////////////////////////////////////////////////////
///
/// loadPreset()
/// ------------
///
////////////////////////////////////////////////////////////////////////////////

template <class PresetConsumer>
bool LE_COLD loadPreset(juce::File const &presetFile, bool const ignoreExternalSample,
                        juce::String *const pComment, char_t const *const presetName,
                        PresetConsumer consumer)
{
    auto const pPresetData(readPresetFile(presetFile));
    if (!pPresetData)
        return false;
    consumer
        .notifyHostAboutPresetChangeBegin(); //...mrmlj...assumes host initiated change != loading from file
    bool const success(loadPreset(pPresetData.get(), ignoreExternalSample, pComment, consumer));
    if (success)
    {
        /// \note The program name mattered under VST2.4, where hosts reacted to
        /// audioMasterUpdateDisplay only if it had changed. That protocol is
        /// gone; the name is kept because the editor still shows it.
        ///                                   (12.09.2014.) (Domagoj Saric)
        copyPresetName(presetName, consumer.program().name());
    }
    consumer.notifyHostAboutPresetChangeEnd();
    return success;
}

void savePreset(juce::File const &, juce::File const &externalSampleFile,
                juce::String const &comment, Program const &);

//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // presetFile_hpp
