////////////////////////////////////////////////////////////////////////////////
///
/// \file rvalueReferences.hpp
/// --------------------------
///
///   Was a std::move/std::forward/std::declval shim for the 2011 era standard
/// libraries that shipped without them - and, on any library that was not
/// libc++ or the MS STL, it defined them into namespace std itself. C++20
/// makes all of that both unnecessary and dangerous, so it is now just
/// <utility>. Kept as a header so the ~30 include sites stay valid.
///
/// Copyright (c) 2011 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef rvalueReferences_hpp__D83532BA_1B66_4B90_BC66_20F6AE32F0A5
#define rvalueReferences_hpp__D83532BA_1B66_4B90_BC66_20F6AE32F0A5
//------------------------------------------------------------------------------
#include <utility>
//------------------------------------------------------------------------------
#endif // rvalueReferences_hpp
