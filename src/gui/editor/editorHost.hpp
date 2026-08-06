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
#include "core/threading/messages.hpp"
#include "core/threading/valueMailbox.hpp"

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

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \brief The protocol, as the editor sees it: a queue to ask the engine for
    /// something, and a mailbox to read what it is currently doing.
    ///
    /// \note These belong to the plugin and not to the editor, because the host
    /// reads parameters through `clap_plugin_params` with the window shut. The
    /// editor is a *user* of them, which is why they arrive through this
    /// interface rather than through the editor's constructor.
    ///
    /// \note `const` on both, and a `const &` on the mailbox: the editor never
    /// owns either, and it only ever reads the second. Pushing to a queue is a
    /// mutation of the queue and not of the host, which is why the first is
    /// const-qualified and hands back a non-const queue.
    ///
    /// \see core/threading/messages.hpp, doc/tech/threading_model.md §3.
    ///
    ////////////////////////////////////////////////////////////////////////////

    virtual Threading::ToEngineQueue &toEngine() const = 0;
    virtual Threading::ValueMailbox const &modulatedValues() const = 0;

    /// \note How the plugin comes to know about the editor at all. Called from
    /// the editor's constructor and destructor, on the UI thread -- opened()
    /// last, so nothing reaches a half-built editor, and closed() first, so
    /// nothing reaches a dying one.
    virtual void editorOpened(SpectrumWorxEditor &) = 0;
    virtual void editorClosed() = 0;

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \brief Asks whoever owns the window to make it \p width x \p height.
    /// `[main-thread]`
    ///
    /// \return whether it will be. A refusal is not a failure and must not be
    /// treated as one: the editor lays its panel out over the module strips
    /// instead. \see SpectrumWorxEditor::PanelPlacement.
    ///
    /// \note `clap_host_gui::request_resize`, which is the only way a plugin can
    /// change the size of a window it does not own -- and the reason this is on
    /// the interface rather than something the editor does for itself. Nothing
    /// here says the editor may be dragged bigger: `can_resize` stays false and
    /// this is a request for one particular size.
    ///
    ////////////////////////////////////////////////////////////////////////////
    virtual bool requestEditorSize(int width, int height) = 0;

    ////////////////////////////////////////////////////////////////////////////
    // The side channel's sample: the external audio file that feeds it in place
    // of the host's side chain port.
    //
    // \note The listener registration is the 2016 shape and the last three are
    // vestigial while a load is synchronous -- isSampleLoadInProgress() is what
    // the editor asks before it registers, and today it is always false. Kept
    // whole because they are the interface a deferred load would come back
    // through, and the note on SpectrumWorxCLAP::setNewSample says what such a
    // load would need first.
    ////////////////////////////////////////////////////////////////////////////

    virtual juce::File currentSampleFile() const = 0;
    virtual void setNewSample(juce::File const &) = 0;
    virtual bool isSampleLoadInProgress() const = 0;
    virtual void registerSampleLoadedListener(SpectrumWorxEditor &) = 0;
    virtual void deregisterSampleLoadedListener(SpectrumWorxEditor const &) = 0;

    /// \note Presets were two more virtuals here, and are neither. Everything
    /// loading one needs is reachable through core() and automation(), so it is
    /// one free function over this interface -- presetLoading.hpp -- rather than
    /// something each plugin format reimplements. The program's name comes off
    /// the Program itself.

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
