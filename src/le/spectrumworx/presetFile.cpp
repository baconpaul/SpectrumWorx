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

    auto const presetSize(static_cast<unsigned int>(fileData.getSize()));
    LE_TRACE_IF(presetSize > Preset::InMemoryPresetBuffer().size(),
                "\tSW: suspiciously large preset.");

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

    /// \note A preset that does not fit returns 0 and is reported rather than
    /// written; it used to run off the end of this buffer, which the 2016
    /// sources already record five TuneWorx modules doing.
    Preset::InMemoryPresetBuffer buffer;
    auto const presetSize(savePreset(buffer, externalSampleFile, comment, program));

    if (!presetSize || !writePresetFile(file, buffer.data(), presetSize))
        reportPresetProblem(PresetProblem::SaveFailed);
}

LE_OPTIMIZE_FOR_SIZE_END()

//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
