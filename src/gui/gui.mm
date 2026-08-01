////////////////////////////////////////////////////////////////////////////////
///
/// gui.mm
/// ------
///
///   The one thing SpectrumWorx still needs Objective-C for on macOS: putting
/// Cocoa into multithreaded mode before JUCE starts.
///
/// \note This was 367 lines, most of it a Carbon `HIView` reimplementation of
/// JUCE's old `juce_VST_wrapper.mm` window attachment plus
/// `[NSWindow addChildWindow:]` glue for the owned windows. Stage 6.4 made the
/// preset browser and the settings panel ordinary child components of the
/// editor, and the CLAP shim parents the editor itself, so all of it went --
/// and `-framework Carbon` with it.
///                                           (01.08.2026.) (SW port)
///
/// Copyright (c) 2010 - 2013. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "le/utility/assert.hpp"

#include <Cocoa/Cocoa.h>
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

void initialiseMac() noexcept
{
    /// \note This used to assert multithreaded mode rather than ask for it,
    /// which held only because a DAW has spawned threads long before it opens
    /// an editor. It is not true of a plain single threaded process -- the
    /// sw-show-ui harness, say -- and the assert fired there for no fault of
    /// the code under it.
    ///
    ///   Cocoa becomes thread safe once a second NSThread has been spawned and
    /// never leaves that state, so a thread that returns immediately is the
    /// documented way to enter it. Detaching one posts
    /// NSWillBecomeMultiThreadedNotification before the thread runs, so the
    /// mode is on by the time this returns.
    ///                                       (29.07.2026.) (SW port)
    if (![NSThread isMultiThreaded])
        [NSThread detachNewThreadSelector:@selector(class)
                                 toTarget:[NSObject class]
                               withObject:nil];
    LE_ASSERT([NSThread isMultiThreaded]);
}

//------------------------------------------------------------------------------
} // namespace GUI
//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
