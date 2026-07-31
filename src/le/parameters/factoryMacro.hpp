////////////////////////////////////////////////////////////////////////////////
///
/// \file factoryMacro.hpp
/// ----------------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef factoryMacro_hpp__8F075C97_FE9A_47F0_A01B_A3632AA17AC9
#define factoryMacro_hpp__8F075C97_FE9A_47F0_A01B_A3632AA17AC9
//------------------------------------------------------------------------------
#include "parameter.hpp"
#include "parameterList.hpp"

#include "boost/preprocessor/comparison/greater.hpp"
#include "boost/preprocessor/control/iif.hpp"
#include "boost/preprocessor/seq/seq.hpp"
#include "boost/preprocessor/seq/for_each.hpp"
#include "boost/preprocessor/seq/enum.hpp"
#include "boost/preprocessor/seq/size.hpp"
#include "boost/preprocessor/seq/transform.hpp"
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace Parameters
{
//------------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
///
/// Helper verbosity reducing macros for parameter specifications.
/// --------------------------------------------------------------
///
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
///
/// \internal
/// \def LE_SKIP_ALREADY_DEFINED_PARAMETER
///
////////////////////////////////////////////////////////////////////////////////

#define LE_SKIP_ALREADY_DEFINED_PARAMETER(parameterSequence)

////////////////////////////////////////////////////////////////////////////////
///
/// \internal
/// \def LE_DEFINE_INDIVIDUAL_PARAMETER
/// \brief Calls LE_DEFINE_PARAMETER if the parameter is not already defined (if
/// its sequence has more than one element).
///
////////////////////////////////////////////////////////////////////////////////

#define LE_DEFINE_INDIVIDUAL_PARAMETER(r, dummy, parameterSequence)                                \
    BOOST_PP_IIF(BOOST_PP_GREATER(BOOST_PP_SEQ_SIZE(parameterSequence), 1), LE_DEFINE_PARAMETER,   \
                 LE_SKIP_ALREADY_DEFINED_PARAMETER)(parameterSequence)

////////////////////////////////////////////////////////////////////////////////
///
/// \internal
/// \def LE_DEFINE_MY_PARAMETERS
/// \brief Declares all the parameters from the parameter sequence.
///
////////////////////////////////////////////////////////////////////////////////

#define LE_DEFINE_MY_PARAMETERS(parameterSequence)                                                 \
    BOOST_PP_SEQ_FOR_EACH(LE_DEFINE_INDIVIDUAL_PARAMETER, 0, parameterSequence)

////////////////////////////////////////////////////////////////////////////////
///
/// \internal
/// \def LE_ENUMERATE_PARAMETER_NAMES
/// \brief The parameter sequence as the comma separated list of its names that
/// ParameterList takes -- the head of each parameter's own sequence.
///
////////////////////////////////////////////////////////////////////////////////

#define LE_PARAMETER_NAME(s, dummy, parameterSequence) BOOST_PP_SEQ_HEAD(parameterSequence)

#define LE_ENUMERATE_PARAMETER_NAMES(parameters)                                                   \
    BOOST_PP_SEQ_ENUM(BOOST_PP_SEQ_TRANSFORM(LE_PARAMETER_NAME, 0, parameters))

////////////////////////////////////////////////////////////////////////////////
///
/// \def LE_DEFINE_PARAMETERS
///
/// \brief Helper macro used to simplify the definition of an effect's
/// parameters, both the individual parameters and the Parameters collection/
/// container.
///
///   It accepts a "two dimensional" Boost preprocessor sequence
/// (http://www.boost.org/doc/libs/release/libs/preprocessor/doc/data/sequences.html),
/// in other words, a sequence whose each element represents one parameter and
/// is in fact also a sequence (of properties that define the particular
/// parameter).
///   The latter (the sequence that defines a parameter, a "parameter sequence"
/// from now on) can have one or more elements. If it has only one element it
/// is interpreted as the name of an already defined parameter which is then
/// simply 'inserted' into the parameters being defined. If it has more than
/// one element it, the sequence, is passed on to the LE_DEFINE_PARAMETER macro.
///
///   The macro automatically prepends any required namespace names.
///
////////////////////////////////////////////////////////////////////////////////
// Implementation note:
//   Because full specialization is allowed only at namespace scope the dummy
// partial specialization workaround is used for metafunctions and the typed
// pointer dispatch for functions.
//                                            (28.06.2011.) (Domagoj Saric)
////////////////////////////////////////////////////////////////////////////////

// Implementation note:
//   Parameters remains a class of its own rather than an alias for the
// ParameterList it is: presets.hpp forward declares GlobalParameters::Parameters,
// and two effects declaring the same parameters would otherwise share one type.
//                                            (31.07.2026.) (SW port)

#define LE_DEFINE_PARAMETERS(parameters)                                                           \
    LE_DEFINE_MY_PARAMETERS(parameters)                                                            \
    struct Parameters : ::LE::Parameters::ParameterList<LE_ENUMERATE_PARAMETER_NAMES(parameters)>  \
    {                                                                                              \
    };

//------------------------------------------------------------------------------
} // namespace Parameters
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
#endif // factoryMacro_hpp
