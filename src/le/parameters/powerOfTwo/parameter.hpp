////////////////////////////////////////////////////////////////////////////////
///
/// \file powerOfTwo/parameter.hpp
/// ------------------------------
///
/// Copyright (c) 2011 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#if defined(_MSC_VER) && !defined(__clang__) && (_MSC_VER < 1800)
#include "le/parameters/cpp03/powerOfTwo/parameter.hpp"
#endif // old MSVC
//------------------------------------------------------------------------------
#ifndef parameter_hpp__8CF0596B_4588_4B99_8488_5F77415359B
#define parameter_hpp__8CF0596B_4588_4B99_8488_5F77415359B
//------------------------------------------------------------------------------
#include "tag.hpp"

#include "le/math/conversion.hpp"
#include "le/math/math.hpp"
#include "le/parameters/parameter.hpp"

#include <cstdint>
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace Parameters
{
//------------------------------------------------------------------------------

namespace Detail ///< \internal
{
template <class... ArgumentPack> struct PowerOfTwoParameterTraits
{
  public:
    using Defaults = TraitPack<Traits::Unit<0, 0>>;
    using Traits = TraitPack<ArgumentPack...>; //...mrmlj...FMOD param info...

    using Tag = PowerOfTwoParameterTag;

    using value_type = std::uint16_t;
    using param_type = value_type;

    static value_type constexpr unscaledMinimum =
        GetTrait<Parameters::Traits::Tag::Minimum, ArgumentPack...>::type::value;
    static value_type constexpr unscaledMaximum =
        GetTrait<Parameters::Traits::Tag::Maximum, ArgumentPack...>::type::value;
    static value_type constexpr unscaledDefault =
        GetTrait<Parameters::Traits::Tag::Default, ArgumentPack...>::type::value;
    static value_type constexpr rangeValuesDenominator = 1;

    static value_type minimum() { return unscaledMinimum; }
    static value_type maximum() { return unscaledMaximum; }
    static value_type default_() { return unscaledDefault; }

    static value_type const discreteValueDistance = 0;

    static bool isValidValue(param_type const value)
    {
        return Math::isValueInRange(value, minimum(), maximum()) && Math::isPowerOfTwo(value);
    }

  protected:
    static void increment(value_type &value) { value *= 2; }
    static void decrement(value_type &value) { value /= 2; }

  private:
    static_assert(Math::IsPowerOfTwo<unscaledMinimum>::value, "Internal inconsistency");
    static_assert(Math::IsPowerOfTwo<unscaledMaximum>::value, "Internal inconsistency");
    static_assert(Math::IsPowerOfTwo<unscaledDefault>::value, "Internal inconsistency");

    static_assert(unscaledMinimum <= unscaledMaximum, "Internal inconsistency");
    static_assert(unscaledDefault <= unscaledMaximum, "Internal inconsistency");
    static_assert(unscaledMinimum <= unscaledDefault, "Internal inconsistency");
};
} // namespace Detail

////////////////////////////////////////////////////////////////////////////////
/// \internal
/// \class PowerOfTwoParameter
////////////////////////////////////////////////////////////////////////////////

template <typename... Traits>
using PowerOfTwoParameter = Parameter<Detail::PowerOfTwoParameterTraits<Traits...>>;

//------------------------------------------------------------------------------
} // namespace Parameters
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // parameter_hpp
