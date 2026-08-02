////////////////////////////////////////////////////////////////////////////////
///
/// threadCheck.cpp
/// ---------------
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "threadCheck.hpp"

#include "le/utility/threadIdentity.hpp"

#include <sst/cpputils/rtsan_support.h>

#include <atomic>
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

namespace
{
/// \note Process wide rather than per plugin, because it is. Every instance a host
/// loads shares one main thread, and the second instance to call markMainThread()
/// writes the same value the first did.
std::atomic<void const *> mainThread_{nullptr};

/// \note Per thread, and a depth rather than a flag: nothing nests process()
/// today, but a counter cannot be got wrong by a future caller that does.
thread_local unsigned int audioCallbackDepth_{0};
} // anonymous namespace

void markMainThread()
{
    mainThread_.store(Utility::currentThreadToken(), std::memory_order_release);
}

bool isMainThread()
{
    return mainThread_.load(std::memory_order_acquire) == Utility::currentThreadToken();
}

void forgetMainThread() { mainThread_.store(nullptr, std::memory_order_release); }

bool isAudioThread() { return audioCallbackDepth_ != 0; }

ScopedAudioCallback::ScopedAudioCallback()
{
    ++audioCallbackDepth_;
#if SST_CPPUTILS_HAS_RTSAN
    __rtsan_realtime_enter();
#endif
}

ScopedAudioCallback::~ScopedAudioCallback()
{
#if SST_CPPUTILS_HAS_RTSAN
    __rtsan_realtime_exit();
#endif
    --audioCallbackDepth_;
}

//------------------------------------------------------------------------------
} // namespace Threading
//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
