////////////////////////////////////////////////////////////////////////////////
///
/// \file parameter.hpp
/// -------------------
///
/// Copyright (c) 2011 - 2015. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef parameter_hpp__D017E02E_44C0_40FE_BC6B_4493C53BF028
#define parameter_hpp__D017E02E_44C0_40FE_BC6B_4493C53BF028
//------------------------------------------------------------------------------
#include "tag.hpp"

#include "le/utility/assert.hpp"
#include "le/utility/ignoreUnused.hpp"
#include "boost/mpl/map/map0.hpp"
#include "boost/mpl/map/map10.hpp"
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace Parameters
{
//------------------------------------------------------------------------------

namespace Traits
{
template <unsigned int stringLiteralPart1, unsigned int stringLiteralPart2> struct Unit;
}

namespace Detail ///< \internal
{
struct BooleanParameterTraits
{
  public: // Types.
    typedef BooleanParameterTag Tag;

    typedef bool value_type;
    typedef value_type param_type;

    typedef boost::mpl::map1<Traits::Unit<0, 0>> Defaults;
    typedef boost::mpl::map0<> Traits; //...mrmlj...FMOD param info...

  public:
    static bool const unscaledMinimum = false;
    static bool const unscaledMaximum = true;
    static bool const unscaledDefault = false;

    static unsigned char const rangeValuesDenominator = 1;

  public: // Values.
    static bool minimum() { return unscaledMinimum; }
    static bool maximum() { return unscaledMaximum; }
    static bool default_() { return unscaledDefault; }

    static unsigned char const discreteValueDistance = 1;

    static bool isValidValue(value_type const value)
    {
        LE_ASSERT((value == false) || (value == true));
        LE::Utility::ignoreUnused(value);
        return true;
    }

  protected:
    static void increment(value_type &value) { value = true; }
    static void decrement(value_type &value) { value = false; }
};
} // namespace Detail

template <class Traits> class Parameter;

////////////////////////////////////////////////////////////////////////////////
/// \internal
/// \typedef Boolean
////////////////////////////////////////////////////////////////////////////////

typedef Parameter<Detail::BooleanParameterTraits> Boolean;

//------------------------------------------------------------------------------
} // namespace Parameters
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // parameter_hpp
