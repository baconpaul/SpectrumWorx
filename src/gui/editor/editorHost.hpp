////////////////////////////////////////////////////////////////////////////////
///
/// \file editorHost.hpp
/// --------------------
///
///   What the editor needs from whatever is hosting it.
///
///   The 2016 editor reached straight into the SpectrumWorx VST2/AU class:
/// effect() recovered it from the editor's own address, because the effect
/// owned the editor as a member. That class is gone, and the CLAP plugin cannot
/// take its place directly -- sw-impl links sw-gui, so sw-gui naming
/// SpectrumWorxCLAP would be a cycle.
///
///   So the dependency is inverted. Most of what the editor asked the effect
/// for was really the engine's, and is reached through core(); the rest -- the
/// side channel's sample file, presets, and the two persisted settings -- is
/// genuinely the host's, and is declared here.
///
/// \note Deliberately small, and deliberately not a home for anything the
/// engine already answers. Every function added here is one the editor cannot
/// be tested without a plugin behind it.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef editorHost_hpp__0C5A1E7B_9D34_4F82_A6E1_37B0C4D8F925
#define editorHost_hpp__0C5A1E7B_9D34_4F82_A6E1_37B0C4D8F925
//------------------------------------------------------------------------------
#include "le/utility/platformSpecifics.hpp"

#include <juce_core/juce_core.h>
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace SW
{
//------------------------------------------------------------------------------

class SpectrumWorxCore;
class Plugin2HostInteropControler;

namespace GUI
{
//------------------------------------------------------------------------------

class SpectrumWorxEditor;

////////////////////////////////////////////////////////////////////////////////
///
/// \class EditorHost
///
////////////////////////////////////////////////////////////////////////////////

class LE_NOVTABLE EditorHost
{
  public:
    /// The engine. Parameters, the module chain, the setup and the process lock
    /// all come from here.
    virtual SpectrumWorxCore &core() = 0;

    /// The other direction: telling the host that the user moved something.
    /// Gestures, automation notifications and module chain changes.
    virtual Plugin2HostInteropControler &automation() = 0;

    /// \note How the plugin comes to know about the editor at all. Called from
    /// the editor's constructor and destructor, on the UI thread -- opened()
    /// last, so nothing reaches a half-built editor, and closed() first, so
    /// nothing reaches a dying one.
    virtual void editorOpened(SpectrumWorxEditor &) = 0;
    virtual void editorClosed() = 0;

#ifndef LE_SW_DISABLE_SIDE_CHANNEL
    ////////////////////////////////////////////////////////////////////////////
    // The side channel's sample.
    //
    // \note Loading is asynchronous, and the editor shows "Loading..." until it
    // finishes -- hence the listener registration rather than a plain query.
    ////////////////////////////////////////////////////////////////////////////

    virtual juce::File currentSampleFile() const = 0;
    virtual void setNewSample(juce::File const &) = 0;
    virtual bool isSampleLoadInProgress() const = 0;
    virtual void registerSampleLoadedListener(SpectrumWorxEditor &) = 0;
    virtual void deregisterSampleLoadedListener(SpectrumWorxEditor const &) = 0;
#endif // !LE_SW_DISABLE_SIDE_CHANNEL

#ifndef LE_NO_PRESETS
    virtual bool loadPreset(juce::File const &, bool ignoreExternalSample, juce::String *comment,
                            char const *presetName) = 0;
    virtual char const *currentProgramName() const = 0;
#endif // !LE_NO_PRESETS

    ////////////////////////////////////////////////////////////////////////////
    // Settings.
    ////////////////////////////////////////////////////////////////////////////

    /// \note Was runningAsAU(): Audio Units negotiate their channel layout with
    /// the host, so the plugin must not offer the user a way to change it.
    virtual bool completelyDisableIOChanges() const = 0;

    virtual bool shouldLoadLastSessionOnStartup() const = 0;
    virtual void shouldLoadLastSessionOnStartup(bool) = 0;

  protected:
    /// Not deleted through this.
    ~EditorHost() = default;
}; // class EditorHost

//------------------------------------------------------------------------------
} // namespace GUI
//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // editorHost_hpp
