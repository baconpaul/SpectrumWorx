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
/// \note The buffer is one byte longer than the file and terminated, because
/// the parse is destructive and RapidXML stops at a NUL it does not itself
/// write. 193 of the 303 factory presets already end in one -- Preset::saveTo()
/// appends it and the 2016 writer put it on disk -- and 110 do not. Appending
/// unconditionally is correct for both: a second NUL after the first is never
/// reached.
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

    /// \todo Preset::saveTo() prints without a bound, so this buffer is a
    /// promise rather than a limit -- the 2016 sources already record that five
    /// TuneWorx modules breach it. 8.1 replaces the printer with one that knows
    /// its own length.
    ///                                       (31.07.2026.) (SW port)
    Preset::InMemoryPresetBuffer buffer;
    auto const presetSize(savePreset(&buffer[0], externalSampleFile, comment, program));
    LE_ASSERT(presetSize <= buffer.size());

    if (!writePresetFile(file, buffer.data(), presetSize))
        reportPresetProblem(PresetProblem::SaveFailed);
}

LE_OPTIMIZE_FOR_SIZE_END()

//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
