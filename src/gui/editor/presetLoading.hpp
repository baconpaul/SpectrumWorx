////////////////////////////////////////////////////////////////////////////////
///
/// \file presetLoading.hpp
/// -----------------------
///
///   Loading a preset into whatever is hosting the editor.
///
///   presets.hpp does the reading: XML in, module chain and parameter values
/// out. What it needs from a caller is a "PresetConsumer" -- where to put the
/// new program, how to lock the audio thread out while the chain is swapped,
/// and how to build a module's UI region as each slot is filled.
///
///   All of that is reachable through `EditorHost` and the editor itself, so
/// there is one implementation rather than one per plugin format. The 2016 code
/// had it as a member of the VST2/AU plugin class, which is why it looked like
/// something only a plugin could supply.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef presetLoading_hpp__5E29B7D4_1A63_4C08_B7F2_9D4E60C381AB
#define presetLoading_hpp__5E29B7D4_1A63_4C08_B7F2_9D4E60C381AB
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
namespace GUI
{
//------------------------------------------------------------------------------

class EditorHost;
class SpectrumWorxEditor;

////////////////////////////////////////////////////////////////////////////////
///
/// loadPreset()
/// ------------
///
////////////////////////////////////////////////////////////////////////////////
///
/// \brief Reads \p presetFile and makes it the host's current program.
///
/// \param comment  the preset's comment, if it has one and the caller wants it.
/// \param presetName goes into Program::name(), truncated to fit.
///
/// \note `[main-thread]`. The audio thread is locked out only for the two
/// moments the module chain is actually swapped; everything else -- parsing,
/// creating modules, building their UI -- happens outside the lock.
///
////////////////////////////////////////////////////////////////////////////////

bool loadPreset(EditorHost &, SpectrumWorxEditor &, juce::File const &presetFile,
                bool ignoreExternalSample, juce::String *comment, char const *presetName);

//------------------------------------------------------------------------------
} // namespace GUI
//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // presetLoading_hpp
