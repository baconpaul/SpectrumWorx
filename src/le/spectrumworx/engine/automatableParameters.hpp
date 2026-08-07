////////////////////////////////////////////////////////////////////////////////
///
/// \file automatableParameters.hpp
/// -------------------------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef automatableParameters_hpp__FF0C3B97_B4C7_4326_B5B9_659183A9AA22
#define automatableParameters_hpp__FF0C3B97_B4C7_4326_B5B9_659183A9AA22
//------------------------------------------------------------------------------
#include "le/spectrumworx/engine/configuration.hpp"
#include "le/parameters/enumerated/parameter.hpp"
#include "le/parameters/powerOfTwo/parameter.hpp"
#include "le/parameters/uiElements.hpp" // the UIElements below

namespace LE
{

namespace SW::Engine
{

// Implementation note:
//   Unlike with other enumerate/"discrete values parameters", we do not use the
// LE_ENUMERATED_PARAMETER macro to define the window function parameter
// because its possible values are already defined with a plain enum in the
// SW SDK which we simply reuse here.
//                                            (01.04.2011.) (Domagoj Saric)
// Implementation note:
//   As a first choice, based on the comment at the bottom of this page
// http://www.katjaas.nl/FFTwindow/FFTwindow&filtering.html, the Hann
// window was chosen as the default/"overall best" window.
//                                            (20.01.2010.) (Domagoj Saric)
/// \todo Further investigate the Hann <-> Hanning debate/confusion. In this
/// http://www.hydrogenaudio.org/forums/lofiversion/index.php/t29439.html
/// discussion it is claimed that both Hann and Hanning windows exist.
///                                           (25.01.2010.) (Domagoj Saric)

using WindowFunction = Parameters::EnumeratedParameter<Constants::NumberOfWindows>;

using FFTSize =
    Parameters::PowerOfTwoParameter<Parameters::Traits::Minimum<Constants::minimumFFTSize>,
                                    Parameters::Traits::Maximum<Constants::maximumFFTSize>,
                                    Parameters::Traits::Default<Constants::defaultFFTSize>>;

using OverlapFactor =
    Parameters::PowerOfTwoParameter<Parameters::Traits::Minimum<Constants::minimumOverlapFactor>,
                                    Parameters::Traits::Maximum<Constants::maximumOverlapFactor>,
                                    Parameters::Traits::Default<Constants::defaultOverlapFactor>>;

#if LE_SW_ENGINE_WINDOW_PRESUM
/// \note We need to be able to forward declare the WindowSizeFactor
/// parameter for the special temporary handling of pre 2.7 presets so we cannot
/// use a simple typedef for the time being.
///                                           (24.04.2012.) (Domagoj Saric)
//using WindowSizeFactor = Parameters::PowerOfTwoParameter
//<
//    Parameters::Traits::Minimum<1>,
//    Parameters::Traits::Maximum<8>,
//    Parameters::Traits::Default<4>
//>;

struct WindowSizeFactor : Parameters::PowerOfTwoParameter<Parameters::Traits::Minimum<1>,
                                                          Parameters::Traits::Maximum<8>,
                                                          Parameters::Traits::Default<1>>
{
    /// \note The base named through a typedef rather than as
    /// `PowerOfTwoParameter(...)`. Unqualified, that names nothing here -- the
    /// template is LE::Parameters::PowerOfTwoParameter and this is
    /// LE::SW::Engine -- so the mem-initialiser did not compile at all. Nothing
    /// noticed for twelve years because nothing could define the macro over it.
    ///                                       (04.08.2026.) (SW port)
    using Base = Parameters::PowerOfTwoParameter<Parameters::Traits::Minimum<1>,
                                                 Parameters::Traits::Maximum<8>,
                                                 Parameters::Traits::Default<1>>;

    explicit WindowSizeFactor(unsigned int const initialValue = WindowSizeFactor::default_())
        : Base(initialValue)
    {
    }
};
#endif // LE_SW_ENGINE_WINDOW_PRESUM

} // namespace SW::Engine

namespace Parameters
{

////////////////////////////////////////////////////////////////////////////////
//
// Engine UIElements definitions.
//
////////////////////////////////////////////////////////////////////////////////

UI_NAME(SW::Engine::FFTSize, "FFT size")
UI_NAME(SW::Engine::OverlapFactor, "Overlap factor")
UI_NAME(SW::Engine::WindowFunction, "Window type")
#if LE_SW_ENGINE_WINDOW_PRESUM
UI_NAME(SW::Engine::WindowSizeFactor, "Window size factor")
#endif // LE_SW_ENGINE_WINDOW_PRESUM

//...mrmlj...this does not work yet because the Window enum is not a member
//...of the WindowFunction parameter class...fix this...
//ENUMERATED_PARAMETER_STRINGS
//(
//    SW::Engine, WindowFunction,
//    (( Hamming       , "Hamming"         ))
//    (( Hann          , "Hann"            ))
//    (( Rectangle     , "Rectangle"       ))
//    (( Triangle      , "Triangle"        ))
//    (( Blackman      , "Blackman"        ))
//    (( BlackmanHarris, "Blackman Harris" ))
//    (( Welch         , "Welch"           ))
//    (( FlatTop       , "Flat top"        ))
//    (( Gaussian      , "Gaussian"        ))
//)

/// \note Written out rather than through ENUMERATED_PARAMETER_STRINGS for the
/// reason above: that macro checks each string against the enumerator it names,
/// and this parameter has no enumerators to name.
template <>
constexpr DiscreteValues<SW::Engine::WindowFunction>::Strings
    DiscreteValues<SW::Engine::WindowFunction>::strings{"Hann",           "Hamming",  "Blackman",
                                                        "BlackmanHarris", "Gaussian", "FlatTop",
                                                        "Welch",          "Triangle", "Rectangle"};

} // namespace Parameters

} // namespace LE

#endif // automatableParameters_hpp
