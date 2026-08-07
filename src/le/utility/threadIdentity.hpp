////////////////////////////////////////////////////////////////////////////////
///
/// \file threadIdentity.hpp
/// ------------------------
///
///   Naming a thread cheaply enough to do it on the audio one.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef threadIdentity_hpp__6D2A4F17_9E83_4C50_B1A6_37E5C0D9418B
#define threadIdentity_hpp__6D2A4F17_9E83_4C50_B1A6_37E5C0D9418B

namespace LE::Utility
{

////////////////////////////////////////////////////////////////////////////////
///
/// \brief A pointer unique to the calling thread, stable for its lifetime.
///
/// \note Not `std::thread::id`. Comparing those is fine, but *publishing* one so
/// that another thread can read it needs `std::atomic<std::thread::id>`, and
/// nothing guarantees that specialisation is lock free -- which matters, because
/// both users of this read and write from inside the audio callback.
/// `std::atomic<void const *>` is lock free everywhere this builds, and the
/// address of a thread-local is as good an identity as the id is.
///
/// \note The thread-local lives in an inline function, so there is exactly one
/// per thread across the whole program however many translation units ask.
///
////////////////////////////////////////////////////////////////////////////////

inline void const *currentThreadToken()
{
    static thread_local char token;
    return &token;
}

} // namespace LE::Utility

#endif // threadIdentity_hpp
