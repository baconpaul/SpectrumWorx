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
// readPresetFile()
// ----------------
//
////////////////////////////////////////////////////////////////////////////////
///
/// \note The buffer is one byte longer than the file and terminated, because the
/// parser is handed a C string. 193 of the 303 factory presets already end in a
/// NUL -- Preset::saveTo() appends one and the 2016 writer put it on disk -- and
/// 110 do not. Appending unconditionally is correct for both: a second NUL after
/// the first is never reached.
///
////////////////////////////////////////////////////////////////////////////////

Preset::InMemoryPreset readPresetFile(juce::File const &file)
{
    juce::MemoryBlock fileData;
    if (!file.loadFileAsData(fileData))
    {
        LE_TRACE("SW: failed to read preset file.");
        return {};
    }

    /// \note 4096 was `InMemoryPresetBuffer`'s size and therefore the largest
    /// preset that could be *written*; it is kept here, as a bare number and
    /// only as a trace, because "larger than anything the 2016 build could have
    /// produced" is still the interesting thing to say about a file.
    auto const presetSize(static_cast<unsigned int>(fileData.getSize()));
    LE_TRACE_IF(presetSize > 4096, "\tSW: suspiciously large preset.");

    Preset::InMemoryPreset pInMemoryPreset(new (std::nothrow) char[presetSize + 1]);
    if (pInMemoryPreset)
    {
        std::memcpy(pInMemoryPreset.get(), fileData.getData(), presetSize);
        pInMemoryPreset[presetSize] = '\0';
    }
    return pInMemoryPreset;
}

/// \note replaceWithData() writes a temporary and renames it, so an interrupted
/// save leaves the previous preset intact. The `map_file`-and-memcpy this
/// replaces truncated the file to the new length first, and a failure anywhere
/// after that lost it.
bool writePresetFile(juce::File const &file, char const *const data, unsigned int const size)
{
    if (file.replaceWithData(data, size))
        return true;
    LE_TRACE("SW: failed to write preset file.");
    return false;
}

void copyPresetName(char_t const *const name, std::span<char> const target)
{
    LE_ASSERT(!target.empty());
    if (target.empty())
        return;

    std::size_t written(0);
    if (name)
    {
        auto const limit(target.size() - 1);
        while ((written < limit) && (name[written] != '\0'))
        {
            target[written] = static_cast<char>(name[written]);
            ++written;
        }
        LE_TRACE_IF(name[written] != '\0', "\tSW: preset name truncated to %u characters.",
                    static_cast<unsigned int>(written));
    }
    target[written] = '\0';
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
