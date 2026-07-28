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
#ifdef _WIN32
#include <mutex>
#else // POSIX
#include <pthread.h>
#endif // OS
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace Utility
{
//------------------------------------------------------------------------------

#ifdef _WIN32

using CriticalSection = std::mutex;

#else // POSIX

class CriticalSection
{
  public:
    LE_COLD CriticalSection()
#ifdef PTHREAD_RECURSIVE_MUTEX_INITIALIZER
        : mutex_(PTHREAD_RECURSIVE_MUTEX_INITIALIZER)
    {
    }
#else  // PTHREAD_RECURSIVE_MUTEX_INITIALIZER ...mrmlj...for osx10.6 only? recheck...
    {
        ::pthread_mutexattr_t attributes;
        LE_VERIFY(::pthread_mutexattr_init(&attributes) == 0);
        LE_VERIFY(::pthread_mutexattr_settype(&attributes, PTHREAD_RECURSIVE_MUTEX) == 0);
        LE_VERIFY(::pthread_mutex_init(&mutex_, &attributes) == 0);
    }
#endif // PTHREAD_RECURSIVE_MUTEX_INITIALIZER
    LE_COLD ~CriticalSection() { LE_VERIFY(::pthread_mutex_destroy(&mutex_) == 0); }

    void lock() { LE_VERIFY(::pthread_mutex_lock(&mutex_) == 0); }
    void unlock() { LE_VERIFY(::pthread_mutex_unlock(&mutex_) == 0); }

    bool try_lock() { return ::pthread_mutex_trylock(&mutex_) == 0; }

    enum RecursiveType
    {
        NonRecursive
    }; //...mrmlj...
    LE_COLD explicit CriticalSection(RecursiveType)
#ifdef NDEBUG
        : mutex_(PTHREAD_MUTEX_INITIALIZER){}
#else
        : mutex_(PTHREAD_ERRORCHECK_MUTEX_INITIALIZER)
    {
    }
#endif // NDEBUG

          CriticalSection(CriticalSection && other)
        : mutex_(other.mutex_)
    {
        other.mutex_ = PTHREAD_RECURSIVE_MUTEX_INITIALIZER;
    }

    CriticalSection(CriticalSection const &) = delete;

  private:
    friend class ConditionVariable; //...mrmlj...
    ::pthread_mutex_t mutex_;
}; // class CriticalSection

#endif // OS

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
