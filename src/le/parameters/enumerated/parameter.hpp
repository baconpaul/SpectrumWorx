////////////////////////////////////////////////////////////////////////////////
///
/// \file enumerated/parameter.hpp
/// ------------------------------
///
/// Copyright (c) 2011 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef parameter_hpp__5820E6B3_7684_4DF4_BC99_B0A5CCB0F3E9
#define parameter_hpp__5820E6B3_7684_4DF4_BC99_B0A5CCB0F3E9
//------------------------------------------------------------------------------
#include "tag.hpp"

#include "le/parameters/linear/parameter.hpp"

#include <cstdint>

namespace LE::Parameters
{

template <typename... Traits> struct TraitPack;

namespace Detail ///< \internal
{
template <std::uint8_t numberOfValues>
struct EnumeratedParameterTraits : LinearParameterTraitsBase<0, numberOfValues - 1, 0>
{
  public: // Types.
    using Tag = EnumeratedParameterTag;

    using value_type = std::uint8_t;
    using param_type = value_type;
    using binary_type = value_type;

    using Defaults = TraitPack<Traits::Unit<"">>;
    using Traits = TraitPack<>;

  public: // Values
    static unsigned int const rangeValuesDenominator = 1;

    static value_type minimum() { return 0; }
    static value_type maximum() { return numberOfValues - 1; }
    static value_type default_() { return 0; }

    static value_type const discreteValueDistance = 1;

    static value_type const numberOfDiscreteValues = numberOfValues;

    static bool isValidValue(value_type const value)
    {
        return isValueInRange<param_type>(value, minimum(), maximum());
    }

  protected:
    static void increment(value_type &value) { ++value; }
    static void decrement(value_type &value) { --value; }
}; // struct EnumeratedParameterTraits
} // namespace Detail

////////////////////////////////////////////////////////////////////////////////
/// \internal
/// \class EnumeratedParameter
////////////////////////////////////////////////////////////////////////////////

template <std::uint8_t numberOfValues>
using EnumeratedParameter = Parameter<Detail::EnumeratedParameterTraits<numberOfValues>>;

////////////////////////////////////////////////////////////////////////////////
///
/// \def LE_ENUMERATED_PARAMETER
///
/// \brief Helps to define a parameter that has a discrete set of allowed
/// values.
///
///   It will assign automatically generated values to all the named values
/// specified in the valueSequence and will create a member enum with the enum
/// constants/"members" named just as specified in the valueSequence parameter.
///
////////////////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER
#define LE_ENUMERATED_PARAMETER_WARNING_BEGIN() __pragma(warning(push))
__pragma(warning(disable : 4480)) /* Nonstandard extension*/

#define LE_ENUMERATED_PARAMETER_WARNING_END() __pragma(warning(pop))
#else
#define LE_ENUMERATED_PARAMETER_WARNING_BEGIN()
#define LE_ENUMERATED_PARAMETER_WARNING_END()
#endif // _MSC_VER

// Implementation note:
//   The value list was a Boost.PP sequence, ( Replace )( Sum ), for two reasons:
// BOOST_PP_SEQ_ENUM turned it into enumerators and BOOST_PP_SEQ_SIZE counted
// them, and the count is a template argument of the base class -- so it has to
// exist before the enum the class declares does.
//
//   The scoped enum below is that count, and it is the same list: an extra
// enumerator past the end of a list numbered from zero *is* the length. It costs
// a name beside the parameter and no argument-counting macro, whose only other
// spelling is a ladder of numbered arguments with a ceiling to raise later.
//                                            (31.07.2026.) (SW port)

#define LE_ENUMERATED_PARAMETER(parameterName, ...)                                                \
    enum class parameterName##Values_ : std::uint8_t{__VA_ARGS__, numberOfValues_};                \
    class parameterName : public LE::Parameters::EnumeratedParameter<static_cast<std::uint8_t>(    \
                              parameterName##Values_::numberOfValues_)>                            \
    {                                                                                              \
      private:                                                                                     \
        using Base = type;                                                                         \
                                                                                                   \
      public:                                                                                      \
        parameterName(type::param_type const initialValue = Base::default_()) : Base(initialValue) \
        {                                                                                          \
        }                                                                                          \
        LE_ENUMERATED_PARAMETER_WARNING_BEGIN()                                                    \
        enum value_type : std::uint8_t                                                             \
        {                                                                                          \
            __VA_ARGS__                                                                            \
        };                                                                                         \
        LE_ENUMERATED_PARAMETER_WARNING_END()                                                      \
        operator value_type() const { return static_cast<value_type>(Base::getValue()); }          \
    }

} // namespace LE::Parameters

#endif // parameter_hpp
