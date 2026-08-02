////////////////////////////////////////////////////////////////////////////////
///
/// presetFile.cpp
/// --------------
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "presetFile.hpp"

#include "configuration/versionConfiguration.hpp" // MB_ERROR

#include "le/utility/assert.hpp"
#include "le/utility/trace.hpp"

#include <juce_core/juce_core.h>

#include <cstring>
#include <new>
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace GUI
{
void warningMessageBox(std::string_view title, std::string_view message, bool canBlock);
}
//------------------------------------------------------------------------------
namespace SW
{
//------------------------------------------------------------------------------

LE_OPTIMIZE_FOR_SIZE_BEGIN()

////////////////////////////////////////////////////////////////////////////////
//
// \note What used to be here -- reading a file into the parser's buffer and
// writing one back -- is presetStorage.cpp now, over std::filesystem and below
// JUCE. What is left is the conversion, which is all this file was ever for once
// the two halves were separated. See presetStorage.hpp.
//                                            (02.08.2026.) (SW port)
//
////////////////////////////////////////////////////////////////////////////////

Preset::InMemoryPreset readPresetFile(juce::File const &file)
{
    return readPresetFile(std::filesystem::path(file.getFullPathName().toRawUTF8()));
}

bool writePresetFile(juce::File const &file, char const *const data, unsigned int const size)
{
    return writePresetFile(std::filesystem::path(file.getFullPathName().toRawUTF8()), data, size);
}

std::string savePreset(juce::File const &externalSampleFile, juce::String const &comment,
                       Program const &program, DawExtraState const *const pDawExtraState)
{
    /// \note Where the interface's strings become the format's bytes, and the
    /// only place they do. presets.cpp took a `juce::File` and a `juce::String`
    /// and converted them itself, which is what kept JUCE on `sw-dsp`.
    auto const path(externalSampleFile == juce::File() ? juce::String()
                                                       : externalSampleFile.getFullPathName());
    return savePreset(std::string_view(path.toRawUTF8(), path.getNumBytesAsUTF8()),
                      std::string_view(comment.toRawUTF8(), comment.getNumBytesAsUTF8()), program,
                      pDawExtraState);
}

void savePreset(juce::File const &file, juce::File const &externalSampleFile,
                juce::String const &comment, Program const &program)
{
    LE_ASSERT(file.getParentDirectory().isDirectory());

    /// \note No size limit any more, and so no "does not fit" to report. The
    /// 4096-byte `InMemoryPresetBuffer` that used to stand here was breached by
    /// five TuneWorx modules -- the 2016 sources say so -- and the writer builds
    /// the whole document in a string of its own regardless, so the buffer never
    /// bounded anything except what could be saved.
    ///
    /// \note The terminator is written to the file, because the 2016 writer put
    /// one there and 193 of the 303 committed presets end in a NUL byte.
    auto const preset(savePreset(externalSampleFile, comment, program));

    if (!writePresetFile(file, preset.c_str(), static_cast<unsigned int>(preset.size() + 1)))
        reportPresetProblem(PresetProblem::SaveFailed);
}

LE_OPTIMIZE_FOR_SIZE_END()

//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
