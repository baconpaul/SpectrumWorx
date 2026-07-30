////////////////////////////////////////////////////////////////////////////////
///
/// \file criticalSection.hpp
/// -------------------------
///
/// Copyright (c) 2012 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef criticalSection_hpp__FA935666_72C2_4CED_934B_2B9CE788D195
#define criticalSection_hpp__FA935666_72C2_4CED_934B_2B9CE788D195
//------------------------------------------------------------------------------
#include "le/utility/platformSpecifics.hpp"

/// \note std::mutex (boost::signals2::mutex before it) is not recursive.
///                                           (27.09.2013.) (Domagoj Saric)
/// \todo  Cleanup the code base so that it does not require recursive mutexes
/// or at least minimise and document where they are required...
///                                           (22.03.2016.) (Domagoj Saric)
///
/// \note One `std::recursive_mutex`, on every platform, replacing what were two
/// disagreeing implementations: `std::mutex` on Windows and a hand-rolled
/// pthread wrapper everywhere else. Three reasons.
///
///   - **The pthread arm did not build on glibc.** Its recursive initialiser was
///     spelled `PTHREAD_RECURSIVE_MUTEX_INITIALIZER`, which is Apple's name;
///     glibc's is `..._NP`. The fallback for platforms lacking it named
///     `PTHREAD_RECURSIVE_MUTEX`, which is not a constant on *any* platform —
///     the type value is `PTHREAD_MUTEX_RECURSIVE` — so that branch had never
///     compiled anywhere.
///   - **The two arms disagreed on the one property that matters.** POSIX got a
///     recursive mutex, Windows a non-recursive one, from the same type name. If
///     anything in the code base does relock, that is a macOS-works /
///     Windows-deadlocks split, and the comment above says recursion is what was
///     wanted.
///   - Nothing needed the parts that had to go: the `NonRecursive` constructor
///     has no callers, and `ConditionVariable` (the `friend` that reached for the
///     raw `pthread_mutex_t`) is included by nothing. `lock`/`unlock`/`try_lock`
///     are the whole of the used interface, and `std::recursive_mutex` has them.
///
/// `SpectrumWorxCore::currentThreadOwnsTheProcessLock` reinterpret_casts this to
/// a `CRITICAL_SECTION` under `_WIN32`. That was already invalid for the MS STL's
/// `std::mutex` and stays invalid here; it is Windows bring-up's to fix, and it
/// is a debug helper that already returns a hardcoded `true` off Windows.
///                                           (29.07.2026.) (SW port)
#include <mutex>
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace Utility
{
//------------------------------------------------------------------------------

using CriticalSection = std::recursive_mutex;

class CriticalSectionLock
{
  public:
    CriticalSectionLock(CriticalSectionLock const &) = delete; // makes non-copyable
    CriticalSectionLock &operator=(CriticalSectionLock const &) = delete;

#if defined(__GNUC__) || _MSC_VER >= 1900 //...mrmlj...no 'RVO@compiletime'...
  public:
    explicit CriticalSectionLock(CriticalSection &cs) : pCriticalSection_(&cs)
    {
        pCriticalSection_->lock();
    }
    ~CriticalSectionLock()
    {
        if (pCriticalSection_)
            pCriticalSection_->unlock();
    }
    CriticalSectionLock(CriticalSectionLock &&lock) : pCriticalSection_(lock.pCriticalSection_)
    {
        lock.pCriticalSection_ = nullptr;
    }

  private:
    CriticalSection *LE_RESTRICT pCriticalSection_;
#else
  public:
    explicit CriticalSectionLock(CriticalSection &cs) : criticalSection_(cs)
    {
        criticalSection_.lock();
    }
    ~CriticalSectionLock() { criticalSection_.unlock(); }

  private:
    CriticalSection &criticalSection_;
#endif // GCC or Clang
}; // class CriticalSectionLock

//------------------------------------------------------------------------------
} // namespace Utility
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // criticalSection_hpp
