////////////////////////////////////////////////////////////////////////////////
///
/// \file threadCheck.hpp
/// ---------------------
///
///   Which thread is this, and may it do that.
///
///   CLAP annotates every entry point `[main-thread]`, `[audio-thread]` or
/// `[thread-safe]`, and those annotations are contractual. Nothing in this port
/// could check one: `GUI::isThisTheGUIThread()` asks JUCE, which is only an answer
/// once there is a message thread, and `currentThreadOwnsTheProcessLock()` returned
/// a hardcoded `true`. This is the instrument the threading redesign is read by --
/// see doc/tech/correct_the_threading.md.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef threadCheck_hpp__0B4E7D91_2C68_4A35_9F07_5E1D8A62B4C3
#define threadCheck_hpp__0B4E7D91_2C68_4A35_9F07_5E1D8A62B4C3
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace SW
{
//------------------------------------------------------------------------------
namespace Threading
{
//------------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
// The main thread
//
// \note Recorded rather than asked for. `clap.thread-check` is an optional
// extension and a host that does not offer it cannot be asked; `clap_plugin::init`
// is `[main-thread]` by contract, so the thread that runs it is the answer for the
// life of the plugin. A host that offers the extension is still the better source
// and callers that have one should prefer it -- this is what there is otherwise.
////////////////////////////////////////////////////////////////////////////////

/// \brief Records the calling thread as the main thread.
void markMainThread();

/// \brief False before markMainThread() has ever been called, which is the honest
/// answer for a process that has not yet been told (`sw-tests`, `sw-show-ui`).
bool isMainThread();

/// \brief Forgets the main thread. For tests that drive several plugins.
void forgetMainThread();

////////////////////////////////////////////////////////////////////////////////
// The audio thread
////////////////////////////////////////////////////////////////////////////////

/// \brief Whether the calling thread is *inside an audio callback*.
///
/// \note Deliberately not "is this thread the audio thread". A host with a worker
/// pool -- Bitwig and Reaper both have one -- delivers successive blocks of the
/// same plugin on different threads, so there is no such thread to name. What can
/// be answered is whether this call is happening underneath process(), which is
/// what every `[audio-thread]` rule actually means.
bool isAudioThread();

////////////////////////////////////////////////////////////////////////////////
///
/// \class ScopedAudioCallback
///
/// \brief One line at the top of process(). Two jobs:
///
///   - makes isAudioThread() true for the duration, on this thread only;
///   - opens a RealtimeSanitizer realtime region, so that an allocation, a lock or
///     a syscall reached from anywhere underneath is reported with a stack.
///
/// \note The sanitizer half is the runtime entry point rather than the
/// `[[clang::nonblocking]]` attribute. The attribute would do the same at runtime
/// *and* run a static analysis whose diagnostics this tree cannot answer yet --
/// stage 6 is what makes process() genuinely allocation free. The runtime region
/// costs nothing in a build without the sanitizer, where both calls compile away.
///
////////////////////////////////////////////////////////////////////////////////

class ScopedAudioCallback
{
  public:
    ScopedAudioCallback();
    ~ScopedAudioCallback();

    ScopedAudioCallback(ScopedAudioCallback const &) = delete; // makes non-copyable
    ScopedAudioCallback &operator=(ScopedAudioCallback const &) = delete;
}; // class ScopedAudioCallback

//------------------------------------------------------------------------------
} // namespace Threading
//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // threadCheck_hpp
