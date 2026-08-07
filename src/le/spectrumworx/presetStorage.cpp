////////////////////////////////////////////////////////////////////////////////
///
/// presetStorage.cpp
/// -----------------
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "presetStorage.hpp"

#include "le/utility/assert.hpp"

#include <cstring>
#include <fstream>
#include <new>

namespace LE::SW
{

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
/// \note `<fstream>` where this was `juce::File::loadFileAsData`. Same bytes,
/// and it is what lets the reader sit beside the format in `sw-dsp` -- see the
/// note on the header.
///                                           (02.08.2026.) (SW port)
///
////////////////////////////////////////////////////////////////////////////////

Preset::InMemoryPreset readPresetFile(std::filesystem::path const &file)
{
    std::error_code error;
    auto const fileSize(std::filesystem::file_size(file, error));
    if (error)
        return {};

    auto const presetSize(static_cast<unsigned int>(fileSize));

    std::ifstream stream(file, std::ios::binary);
    if (!stream)
        return {};

    Preset::InMemoryPreset pInMemoryPreset(new (std::nothrow) char[presetSize + 1]);
    if (!pInMemoryPreset)
        return {};

    stream.read(pInMemoryPreset.get(), presetSize);
    if (stream.gcount() != static_cast<std::streamsize>(presetSize))
        return {};
    pInMemoryPreset[presetSize] = '\0';
    return pInMemoryPreset;
}

/// \note Writes a temporary and renames it, so an interrupted save leaves the
/// previous preset intact -- which is what `juce::File::replaceWithData` did and
/// what the `map_file`-and-memcpy before it did not: that one truncated the file
/// to the new length first, and a failure anywhere after that lost it.
bool writePresetFile(std::filesystem::path const &file, char const *const data,
                     unsigned int const size)
{
    auto temporary(file);
    temporary += ".tmp";

    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream.write(data, size))
        {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            return false;
        }
    }

    std::error_code error;
    std::filesystem::rename(temporary, file, error);
    if (error)
    {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return false;
    }
    return true;
}

void copyPresetName(char const *const name, std::span<char> const target)
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
            target[written] = name[written];
            ++written;
        }
    }
    target[written] = '\0';
}

LE_OPTIMIZE_FOR_SIZE_END()

} // namespace LE::SW
