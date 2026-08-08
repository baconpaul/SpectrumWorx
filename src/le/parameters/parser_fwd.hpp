////////////////////////////////////////////////////////////////////////////////
///
/// \file parser_fwd.hpp
/// --------------------
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef parser_fwd_hpp__1D9C4A72_58E3_4F60_9B18_3C7A25D0E4B6
#define parser_fwd_hpp__1D9C4A72_58E3_4F60_9B18_3C7A25D0E4B6
//------------------------------------------------------------------------------
#include <optional>

namespace LE
{

namespace SW::Engine
{
class Setup;
} // namespace SW::Engine

namespace Parameters
{

////////////////////////////////////////////////////////////////////////////////
///
/// \brief A parameter's own value, read back out of the text it displays as --
/// or nothing at all, when the text is not something that parameter could hold.
///
/// \note Nothing, rather than a number and a bool, because "nothing" is the
/// answer a host has to be given: `clap_plugin_params::text_to_value` returning
/// false leaves the host to its own editing, where returning a made up value
/// writes it into the engine. That distinction is the whole reason this exists.
///
////////////////////////////////////////////////////////////////////////////////

using ParsedValue = std::optional<float>;

////////////////////////////////////////////////////////////////////////////////
//
// parse()
// -------
//
////////////////////////////////////////////////////////////////////////////////
///
/// \brief print() backwards: a user friendly string representation back into the
/// value it represents.
///
/// \details Answers a value the parameter could actually hold -- clamped to its
/// range, whole where it is integral, a power of two where it must be -- so that
/// every caller gets one that will not assert on its way into the engine.
///
/// \throws nothing
///
////////////////////////////////////////////////////////////////////////////////

template <class Parameter> ParsedValue parse(char const *text, SW::Engine::Setup const &);

struct ParameterValueParser;

} // namespace Parameters

} // namespace LE

#endif // parser_fwd_hpp
