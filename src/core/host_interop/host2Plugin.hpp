////////////////////////////////////////////////////////////////////////////////
///
/// \file host2Plugin.hpp
/// ---------------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef hostInterop_hpp__33697BD4_CAA6_473F_B3D3_330E3644E2EA
#define hostInterop_hpp__33697BD4_CAA6_473F_B3D3_330E3644E2EA
//------------------------------------------------------------------------------
#include "parameters.hpp"
#include "configuration/constants.hpp"
#include "configuration/versionConfiguration.hpp"
#include "core/parameterID.hpp"

#include "le/spectrumworx/engine/configuration.hpp"
#include "le/utility/platformSpecifics.hpp"
#include "le/utility/span.hpp"
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace SW
{
//------------------------------------------------------------------------------

class AutomatedModuleChain;

class LE_NOVTABLE Host2PluginInteropControler
{
  public:
    using ModuleChain = AutomatedModuleChain;

    using Parameters = GlobalParameters::Parameters;

#if LE_SW_ENGINE_INPUT_MODE >= 1
    using InputMode = GlobalParameters::InputMode;
#endif // LE_SW_ENGINE_INPUT_MODE >= 1
    using MixPercentage = GlobalParameters::MixPercentage;
    using FFTSize = GlobalParameters::FFTSize;
#if LE_SW_ENGINE_WINDOW_PRESUM
    using WindowSizeFactor = GlobalParameters::WindowSizeFactor;
#endif // LE_SW_ENGINE_WINDOW_PRESUM
    using OverlapFactor = GlobalParameters::OverlapFactor;
    using WindowFunction = GlobalParameters::WindowFunction;

  protected:
    Host2PluginInteropControler() : blockAutomation_(false) {}

  public:
    class AutomationBlocker;

    LE_PURE_FUNCTION bool blockAutomation() const { return blockAutomation_; }
    LE_PURE_FUNCTION bool presetLoadingInProgress() const
    {
        return blockAutomation();
    } //...mrmlj...

  private:
    mutable bool blockAutomation_;
}; // Host2PluginInteropControler

class Host2PluginInteropControler::AutomationBlocker
{
  public:
    AutomationBlocker(AutomationBlocker const &) = delete; // makes non-copyable
    AutomationBlocker &operator=(AutomationBlocker const &) = delete;

    AutomationBlocker(Host2PluginInteropControler const &effect)
        : pBlockAutomation_(&effect.blockAutomation_)
    {
        LE_ASSUME(*pBlockAutomation_ == false);
        *pBlockAutomation_ = true;
    }
    ~AutomationBlocker()
    {
        LE_ASSUME(*pBlockAutomation_ == true);
        *pBlockAutomation_ = false;
    }

#if defined(__clang__) || _MSC_VER >= 1900
    /// \note Clang lame RVO support workaround.
    ///                                        (02.07.2014.) (Domagoj Saric)
    AutomationBlocker(AutomationBlocker &&other) : pBlockAutomation_(other.pBlockAutomation_)
    {
        static bool dummy;
        dummy = true;
        other.pBlockAutomation_ = &dummy;
    }
#endif // __clang__

  private:
    bool *LE_RESTRICT pBlockAutomation_;
}; // class Host2PluginInteropControler::AutomationBlocker

//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------

//...mrmlj...orphan...
template <typename Char>
LE_NOTHROWNOALIAS char *copyToBuffer(Char const *string, LE::Utility::Span<char> const &buffer);

template <typename Char, std::size_t N>
char *copyToBuffer(Char const *const string, std::array<char, N> &buffer)
{
    return copyToBuffer<Char>(string, LE::Utility::makeSpan(&buffer[0], buffer.size()));
}

//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // hostInterop_hpp
