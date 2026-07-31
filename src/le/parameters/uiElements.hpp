////////////////////////////////////////////////////////////////////////////////
///
/// \file uiElements.hpp
/// --------------------
///
///   Defines "placeholders" for data and functional user interface elements for
/// a particular Parameter class.
///
///  "Placeholders", that is declarations without default implementations, are
/// used to ensure that the user/programmer provides a proper implementation for
/// his/hers parameter class (so the error is caught at link time with a missing
/// symbol error instead of at runtime through wrong behaviour caused by the
/// usage of a default implementation).
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef uiElements_hpp__E78E35E8_D163_442F_84C0_19427B8844BA
#define uiElements_hpp__E78E35E8_D163_442F_84C0_19427B8844BA
//------------------------------------------------------------------------------
#include "linear/parameter.hpp"

#include "le/utility/platformSpecifics.hpp"
#include "le/utility/tchar.hpp"

#include <array>
#include <cstdint>
#include <string_view>
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace SW
{
LE_IMPL_NAMESPACE_BEGIN(Engine)
class Setup;
LE_IMPL_NAMESPACE_END(Engine)
} // namespace SW
//------------------------------------------------------------------------------
namespace Parameters
{
//------------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
///
/// \class Name
///
/// \brief Placeholder for the name of the parameter (non-optional, must be
/// defined for each parameter).
///
////////////////////////////////////////////////////////////////////////////////
// Implementation note:
//   Outside code accesses a particular parameter name through the name() free
// function template while a parameter's name is defined by specializing the
// string_ static data member of the Name class template. This approach
// minimizes verbosity of both name-fetching and name-defining code.
//                                            (22.02.2011.) (Domagoj Saric)
////////////////////////////////////////////////////////////////////////////////

template <class Parameter> struct Name
{
    static char const string_[];
};

template <class Parameter> constexpr std::string_view name() { return Name<Parameter>::string_; }

////////////////////////////////////////////////////////////////////////////////
///
/// \struct DiscreteValues
///
/// \brief Placeholder for individual value strings for discrete-valued
/// parameters.
///
////////////////////////////////////////////////////////////////////////////////

template <class Parameter> struct DiscreteValues
{
    /// \note The element is not itself const -- valueStrings() below fills one
    /// of these during constant evaluation, and a const element cannot be
    /// assigned even there. The array object is const, which is what matters.
    using Strings = std::array<char const *, Parameter::numberOfDiscreteValues>;
    static Strings const strings;

    /// \note What ParameterInfo carries, beside the nullptr a parameter with no
    /// value strings gives it.
    static char const *LE_RESTRICT const *LE_RESTRICT const stringsBegin()
    {
        return strings.data();
    }
};

namespace Detail ///< \internal
{

////////////////////////////////////////////////////////////////////////////////
///
/// \struct ValueString
/// \internal
/// \brief One enumerated value and the string that names it, as
/// ENUMERATED_PARAMETER_STRINGS is given them.
///
////////////////////////////////////////////////////////////////////////////////

struct ValueString
{
    template <class Value>
    consteval ValueString(Value const value, char const *const string)
        : value_(static_cast<std::uint8_t>(value)), string_(string)
    {
    }

    std::uint8_t value_;
    char const *string_;
}; // struct ValueString

////////////////////////////////////////////////////////////////////////////////
///
/// \brief The strings of \p pairs, having checked that each one is against the
/// value at its own position.
///
////////////////////////////////////////////////////////////////////////////////
// Implementation note:
//   Value strings for enumerated parameters are defined in a .cpp file, away
// from the parameter, so nothing stops the two lists from drifting apart -- and
// a string against the wrong value is a wrong preset rather than a crash. The
// check used to be one static_assert per value, emitted by a Boost.PP walk over
// the pair sequence. It is the same check, said once, over a list the compiler
// can read: a consteval function that throws is a compile error naming the
// parameter, which is what the static_assert was for.
//                                            (31.07.2026.) (SW port)
////////////////////////////////////////////////////////////////////////////////

template <class Parameter, std::size_t count>
consteval typename DiscreteValues<Parameter>::Strings
valueStrings(ValueString const (&pairs)[count])
{
    static_assert(count == Parameter::numberOfDiscreteValues,
                  "Wrong number of enumerated parameter value strings");

    typename DiscreteValues<Parameter>::Strings strings{};
    for (std::size_t index{0}; index < count; ++index)
    {
        if (pairs[index].value_ != index)
            throw "Incorrect order of enumerated parameter value-string pairs";
        strings[index] = pairs[index].string_;
    }
    return strings;
}

} // namespace Detail

////////////////////////////////////////////////////////////////////////////////
///
/// 'Transforms' the value of a parameter into a value that should be used for
/// displaying on a user interface.
///
////////////////////////////////////////////////////////////////////////////////

namespace Detail
{
template <class TraitTag, class TraitsPack, class... DefaultTraits> struct GetTraitDefaulted;
}
template <class Parameter> struct DisplayValueTransformer
{
    template <typename Source>
    static Source const &transform(Source const &value, SW::Engine::Setup const &)
    {
        return value;
    }

    using Suffix = typename Detail::GetTraitDefaulted<Traits::Tag::Unit, typename Parameter::Traits,
                                                      typename Parameter::Defaults>::type;
}; // struct DisplayValueTransformer

/// \defgroup UIElementMacros UIElement verbosity reducing macros.
/// \ingroup UIElementMacros
/// \{

////////////////////////////////////////////////////////////////////////////////
///
/// \def ENUMERATED_PARAMETER_STRINGS
///
/// \brief Writes the declaration part of a parameter discrete value's string_
/// definition.
///
////////////////////////////////////////////////////////////////////////////////
// Implementation note:
//   Because value strings for enumerated parameters are defined separately (in
// a .cpp file) from the enumerated parameter we need a mechanism that would
// prevent one to unnoticeably change the order of values at the parameter
// definition site and forget to also change the order of value strings thus
// getting an incorrect mapping between values and their string representations.
//   A first solution (up to revision 4632) was to use a separate
// DiscreteValue<>::string instantiation for each value instead of a single
// std::array<char const *, <numberOfDiscreteValues>>. This required the
// developer to explicitly state for which parameter value a particular string
// is for thereby eliminating the above issue. However this required more
// verbose discrete value name definitions and it added compile time and runtime
// overhead (it required boost::switch_ to fetch strings for values at runtime).
// For this reason the current solution does use a plain array of strings but
// the macro for defining the array also adds static assertions that verify that
// the strings are defined in the proper order.
//                                            (15.07.2011.) (Domagoj Saric)
////////////////////////////////////////////////////////////////////////////////

///   The pairs are written { Value, "string" }, and the value is named
/// unqualified: the lambda opens a scope the parameter's enumerators are visible
/// in, which is the job the macro used to do by pasting `parameter::` in front
/// of each of them.
#define ENUMERATED_PARAMETER_STRINGS(parentNameSpaceOrClass, parameter, ...)                       \
    template <>                                                                                    \
    DiscreteValues<parentNameSpaceOrClass::parameter>::Strings const                               \
        DiscreteValues<parentNameSpaceOrClass::parameter>::strings{[] {                            \
            using enum parentNameSpaceOrClass::parameter::value_type;                              \
            return Detail::valueStrings<parentNameSpaceOrClass::parameter>({__VA_ARGS__});         \
        }()};

#define EFFECT_ENUMERATED_PARAMETER_STRINGS(parentClass, parameter, ...)                           \
    }                                                                                              \
    }                                                                                              \
    namespace Parameters                                                                           \
    {                                                                                              \
    ENUMERATED_PARAMETER_STRINGS(SW::Effects::parentClass, parameter, __VA_ARGS__)                 \
    }                                                                                              \
    namespace SW                                                                                   \
    {                                                                                              \
    namespace Effects                                                                              \
    {

////////////////////////////////////////////////////////////////////////////////
///
/// \def UI_NAME
///
/// \brief Writes the declaration part of a parameter's name definition.
///
////////////////////////////////////////////////////////////////////////////////

#define UI_NAME(parameter) template <> char const Name<parameter>::string_[]

#define EFFECT_PARAMETER_NAME(parameter, name)                                                     \
    }                                                                                              \
    }                                                                                              \
    namespace Parameters                                                                           \
    {                                                                                              \
    UI_NAME(SW::Effects::parameter) = name;                                                        \
    }                                                                                              \
    namespace SW                                                                                   \
    {                                                                                              \
    namespace Effects                                                                              \
    {

/// \} // UIElementMacros

//------------------------------------------------------------------------------
} // namespace Parameters
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------

namespace boost
{
template <std::size_t N>
char const *LE_RESTRICT (*addressof(char const *LE_RESTRICT (&strings)[N])) [N] { return &strings; }
} // namespace boost
#endif // uiElements_hpp
