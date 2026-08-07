////////////////////////////////////////////////////////////////////////////////
///
/// \file presetStorage.hpp
/// -----------------------
///
///   A preset file, read and written, with no JUCE in sight.
///
///   presets.hpp is the *format* -- XML in a writable buffer, in and out of the
/// parameter tree -- and deliberately opens nothing. This is the other half of
/// what `LE_NO_PRESETS` used to stand in for, over `std::filesystem::path` and
/// `<fstream>` so that it can live in `sw-dsp` beside the format it serves.
///
/// \note presetFile.hpp is the same two functions with a `juce::File` on them,
/// for the interface, and it is above `sw-dsp`. Both directions matter:
/// doc/tech/threading_model.md §6 requires *preset loading tests* to link
/// without JUCE, and reading the file is most of what such a test does.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef presetStorage_hpp__3D75A0C6_41E9_4F28_B1D3_8C60E7A4295B
#define presetStorage_hpp__3D75A0C6_41E9_4F28_B1D3_8C60E7A4295B
//------------------------------------------------------------------------------
#include "presets.hpp"

#include <filesystem>
#include <span>

namespace LE::SW
{

/// \brief Reads a preset file into the writable, NUL-terminated buffer that the
/// destructive parse in Preset::loadFrom() requires.
/// \return an empty pointer if the file cannot be read.
Preset::InMemoryPreset readPresetFile(std::filesystem::path const &);

/// \brief Replaces a preset file's contents, creating the file if it is absent.
bool writePresetFile(std::filesystem::path const &, char const *data, unsigned int size);

/// \brief Truncating copy of a NUL-terminated name into a Program::Name.
void copyPresetName(char const *name, std::span<char> target);

} // namespace LE::SW

#endif // presetStorage_hpp
