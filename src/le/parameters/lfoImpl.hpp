////////////////////////////////////////////////////////////////////////////////
///
/// \file lfo.hpp
/// -------------
///
/// Copyright (c) 2010 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef lfoImpl_hpp__506C13FB_1220_44C6_BFC0_C6C9844F02B0
#define lfoImpl_hpp__506C13FB_1220_44C6_BFC0_C6C9844F02B0
//------------------------------------------------------------------------------
#include "lfo.hpp"

#include "le/math/conversion.hpp"
#include "le/parameters/boolean/parameter.hpp"
#include "le/parameters/dynamic/tag.hpp"
#include "le/parameters/factoryMacro.hpp"
#include "le/parameters/linear/parameter.hpp"
#include "le/parameters/symmetric/parameter.hpp"
#include "le/parameters/uiElements.hpp" // the UIElements below

#include <atomic>
#include <cstdint>
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace Plugins //...mrmlj...
{
using AutomatedParameterValue = float;
}
//------------------------------------------------------------------------------
namespace Parameters
{
//------------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
///
/// \class LFO
///
////////////////////////////////////////////////////////////////////////////////

class LFOImpl : public LFO
{
  public:
    static unsigned int const minimumValue = 0;
    static unsigned int const maximumValue = 1;

    static unsigned int const maximumPeriodInNumberOfBars = 16;
    static unsigned int const minimumPeriodAsMaximumBeatDenominator = 8;

  private:
    struct PeriodScaleParameterTraits;

  public:
    using value_type = float;
    using SnappedPeriod = std::pair<float, SyncType>;
    using PeriodScale = Parameters::Parameter<PeriodScaleParameterTraits>;

  public: // Parameters
    // Implementation note:
    //   To enable LFO automation, settings are stored as
    // LE::Parameters::Parameters<>.
    //                                        (18.02.2011.) (Domagoj Saric)

    ////////////////////////////////////////////////////////////////////////////
    /// \class SyncTypes
    /// \brief Implements the Parameter<> concept for the SyncTypes parameter.
    ////////////////////////////////////////////////////////////////////////////

    class SyncTypes : public Parameters::Parameter<Parameters::LinearUnsignedInteger::Modify<
                          Parameters::Traits::Minimum<Free>, Parameters::Traits::Maximum<All>,
                          Parameters::Traits::Default<Quarter>>::type>
    {
      public:
        SyncTypes() : type(default_()) {}
        static value_type default_();
        using type::operator=;
    }; // class SyncTypes

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

    using Waveform = Parameters::EnumeratedParameter<LFO::Waveform::NumberOfWaveforms>;

    ////////////////////////////////////////////////////////////////////////////
    /// \class PeriodScale
    /// \brief Implements the Parameter<> concept for the PeriodScale parameter.
    ////////////////////////////////////////////////////////////////////////////
    //...mrmlj...a 'dynamic' (bounds) parameter...
  private:
    struct PeriodScaleParameterTraits
        : Parameters::Detail::LinearFloatParameterTraits<
              Parameters::TraitPack //...mrmlj...
              <Parameters::Traits::Minimum<1>, Parameters::Traits::Maximum<1>,
               Parameters::Traits::Default<1>, Parameters::Traits::ValuesDenominator<1>,
               Parameters::Traits::Unit<"">>>
    {
        using Tag = Parameters::DynamicRangeParameterTag;

        static value_type minimum();
        static value_type maximum();

        static bool isValidValue(param_type value);
    }; // struct PeriodScaleParameterTraits

  public:
    /// \note The traits are qualified here rather than imported: this is
    /// namespace LE::Parameters itself, and only the namespaces that declare a
    /// lot of parameters -- LE::SW::Effects -- import them.
    ///                                       (31.07.2026.) (SW port)
    LE_DEFINE_PARAMETER(Enabled, Boolean);
    LE_DEFINE_PARAMETER(Phase, SymmetricFloat, Traits::MaximumOffset<50>,
                        Traits::ValuesDenominator<100>);
    LE_DEFINE_PARAMETER(LowerBound, LinearFloat, Traits::Minimum<minimumValue>,
                        Traits::Maximum<maximumValue>, Traits::Default<minimumValue>);
    LE_DEFINE_PARAMETER(UpperBound, LowerBound, Traits::Default<maximumValue>,
                        //...mrmlj...
                        Traits::ValuesDenominator<1>, Traits::Unit<"">);
    LE_DEFINE_PARAMETERS(Enabled, PeriodScale, Phase, LowerBound, UpperBound, SyncTypes, Waveform);

    Parameters &parameters() { return parameters_; }
    Parameters const &parameters() const { return const_cast<LFOImpl &>(*this).parameters(); }

    value_type periodScale() const;

    void setLowerBound(value_type newLowerBound);
    void setUpperBound(value_type newUpperBound);

    void setPeriodScale(value_type periodScale);

  public:
#ifdef LE_NO_LFOs
    struct Timer
    {
        struct TimingInformationChange;
        void reset() {}
    };
#else
    class Timer
    {
      public:
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4510) // Default constructor could not be generated.
#pragma warning(disable : 4512) // Assignment operator could not be generated.
#pragma warning(disable                                                                            \
                : 4610) // Class can never be instantiated - user-defined constructor required.
#endif                  // _MSC_VER
        struct TimingInformationChange
        {
            bool timingInfoChanged() const
            {
                return barDurationChanged() || measureNumeratorChanged();
            }

            bool barDurationChanged() const;
            bool measureNumeratorChanged() const { return measureNumeratorChanged_; }

            value_type const barDurationChangeRatio_;
            bool const measureNumeratorChanged_;
        }; // struct TimingInformationChange
#ifdef _MSC_VER
#pragma warning(pop)
#endif // _MSC_VER

      public:
        Timer();

        value_type const &currentTimeInBars() const { return currentTimeInBars_; }
        value_type const &previousTimeInBars() const { return previousTimeInBars_; }

        TimingInformationChange updatePositionAndTimingInformation(float positionInBars,
                                                                   float barDuration,
                                                                   std::uint8_t measureNumerator);
        TimingInformationChange
        updatePositionAndTimingInformation(unsigned int deltaNumberOfSamples, float sampleRate);

        void setPosition(unsigned int numberOfSamples, float sampleRate);
        void setPosition(float numberOfSeconds);

        void reset();

        ////////////////////////////////////////////////////////////////////////
        ///
        /// \brief The host's tempo, as every LFO in the process sees it.
        ///
        /// \note Still process-wide, and still wrong for two instances in two
        /// tracks at two tempi -- but no longer a data race. Each instance's
        /// audio thread writes these once a block and the message thread reads
        /// them to lay out the LFO panel, which as plain scalars is undefined
        /// behaviour rather than merely a stale answer. Relaxed atomics, because
        /// nothing else is published through them: what a reader needs is a
        /// tempo, not a happens-before edge.
        ///
        ///   Making them per-instance is the real fix and is not this redesign's:
        /// they are read by `snapPeriodScale()`, `clampFreePeriod()` and the two
        /// period-scale bounds, all *static* and all called from the parameter
        /// layer and the editor, so a per-instance timer means threading one
        /// through the LFO parameter interface. Recorded in tech_debt.md.
        ///                                   (02.08.2026.) (SW port)
        ///
        ////////////////////////////////////////////////////////////////////////
        static bool hasTempoInformation()
        {
            return hasTempoInformation_.load(std::memory_order_relaxed);
        }
        static value_type basePeriod() { return barDuration_.load(std::memory_order_relaxed); }
        static std::uint8_t measureNumerator()
        {
            return measureNumerator_.load(std::memory_order_relaxed);
        }
        static value_type measureNumeratorFloat();

      private:
        /// \brief What to report for incoming timing, given what was already
        /// known. See the definition: the first call after construction or
        /// reset() establishes the timing rather than changing it.
        TimingInformationChange establishedChange(value_type barDuration,
                                                  std::uint8_t measureNumerator);

      private:
        value_type currentTimeInBars_;
        value_type previousTimeInBars_;

        ////////////////////////////////////////////////////////////////////////
        ///
        /// \brief Whether this timer has ever been told the timing, as opposed to
        /// still holding the assumed 120 BPM 4/4.
        ///
        /// \note Per instance and *not* the static hasTempoInformation_ below,
        /// which cannot answer this: reset() deliberately leaves that flag alone
        /// -- a 2012 workaround for preset browsing in Live -- while it does put
        /// barDuration_ back to the assumption, so after a reset the flag says
        /// "known" about a value that is a placeholder again. It is also process
        /// wide, and this question is per engine.
        ///
        ///   What it is for: the first block after construction or reset()
        /// *establishes* the timing rather than changing it, and the difference
        /// matters because a Free LFO's period is rescaled by every bar-duration
        /// change (see updateForNewTimingInformation). Comparing a host's real
        /// tempo against an assumption we invented and calling the difference a
        /// change silently moved every LFO period in the plugin on the first
        /// block of any session not at 120 BPM.
        ///                                   (03.08.2026.) (SW port)
        ///
        ////////////////////////////////////////////////////////////////////////
        bool timingInformationEstablished_{false};

        static std::atomic<value_type> barDuration_;
        static std::atomic<std::uint8_t> measureNumerator_;
        static std::atomic<bool> hasTempoInformation_;
    }; // class Timer
#endif // LE_NO_LFOs
  public:
    LFOImpl();

    value_type getValue(Timer const &) const;

    /// \todo These synchronization type altering functions do not automatically
    /// cause period scale resnapping. Reconsider this.
    ///                                       (23.02.2011.) (Domagoj Saric)
    //void addSyncType   ( SyncType syncType );
    //void removeSyncType( SyncType syncType );

    void updateForNewTimingInformation(Timer::TimingInformationChange const &);

    static value_type currentPeriodScaleMinimum();
    static value_type currentPeriodScaleMaximum();

    static SnappedPeriod snapPeriodScale(value_type periodScale, std::uint8_t syncTypes);

    //...mrmlj...cleanup with a new 'logarithmic' parameter/control...
    static void snapPeriodScaleFromAutomation(PeriodScale &);

  public: // Preset saving/loading section
    template <typename T> typename T::value_type adjustValueForPreset(T const &value) const
    {
        return value;
    }

    template <typename T>
    typename T::value_type adjustValueFromPreset(typename T::value_type const value) const
    {
        return value;
    }

  public: // Parameters
          // Implementation note:
          //   To enable LFO automation, settings are stored as
          // LE::Parameters::Parameters<>.
          //                                        (18.02.2011.) (Domagoj Saric)

  public:
    static Plugins::AutomatedParameterValue
    internal2AutomatedValue(std::uint8_t parameterIndex, float internalValue, bool normalised);
    static float automated2InternalValue(std::uint8_t parameterIndex,
                                         Plugins::AutomatedParameterValue automatedValue,
                                         bool normalised);

    static Plugins::AutomatedParameterValue
    unlinearisePeriodScale(Plugins::AutomatedParameterValue linearisedNormalisedPeriodScale);
    static Plugins::AutomatedParameterValue
    linearisePeriodScale(Plugins::AutomatedParameterValue nonlinearNormalisedPeriodScale);

  private:
    friend class LFO;
    static value_type clampFreePeriod(value_type absolutePeriod);
    static SnappedPeriod snapSyncedPeriod(value_type periodScale, std::uint8_t syncTypes);

    value_type getWaveformAmplitudeForPosition(value_type position, bool newPeriodBegun) const;

    bool isValueInBounds(value_type) const;
    static bool isValueInRange(value_type);

  private:
    friend class LFO;
    Parameters parameters_;
    mutable value_type state_[2];
}; // class LFOImpl

template <>
LFOImpl::PeriodScale::value_type LFOImpl::adjustValueForPreset(PeriodScale const &) const;

template <>
LFOImpl::PeriodScale::value_type
    LFOImpl::adjustValueFromPreset<LFOImpl::PeriodScale>(LFOImpl::PeriodScale::value_type) const;

////////////////////////////////////////////////////////////////////////////////
//
// LFOImpl UIElements definitions.
//
////////////////////////////////////////////////////////////////////////////////

UI_NAME(LFOImpl::Enabled, "on")
UI_NAME(LFOImpl::PeriodScale, "T")
UI_NAME(LFOImpl::Phase, "ph")
UI_NAME(LFOImpl::LowerBound, "lbnd")
UI_NAME(LFOImpl::UpperBound, "ubnd")
UI_NAME(LFOImpl::SyncTypes, "sync")
UI_NAME(LFOImpl::Waveform, "wfrm")

//...mrmlj...this does not work yet because the Window enum is not a member
//...of the WindowFunction parameter class...fix this...
//ENUMERATED_PARAMETER_STRINGS
//(
//    LFOImpl, Waveform,
//    (( Sine           , "Sine"       ))
//    (( Triangle       , "Triangle"   ))
//    (( Sawtooth       , "Sawtooth"   ))
//    (( ReverseSawtooth, "htootwaS"   ))
//    (( Square         , "Square"     ))
//    (( Exponent       , "Exponent"   ))
//    (( RandomHold     , "Hrandom"    ))
//    (( RandomSlide    , "Grandom"    ))
//    (( Whacko         , "Whacko"     ))
//    (( Dirac          , "Dirac up"   ))
//    (( dIRAC          , "Dirac down" ))
//)

/// \note Written out rather than through ENUMERATED_PARAMETER_STRINGS for the
/// reason above: that macro checks each string against the enumerator it names,
/// and this parameter has no enumerators to name.
template <>
constexpr DiscreteValues<LFOImpl::Waveform>::Strings DiscreteValues<LFOImpl::Waveform>::strings{
    "Sine",    "Triangle", "Sawtooth", "htootwaS", "Square",    "Exponent",
    "Hrandom", "Grandom",  "Whacko",   "Dirac up", "Dirac down"};

//------------------------------------------------------------------------------
} // namespace Parameters
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // lfoImpl_hpp
