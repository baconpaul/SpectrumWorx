////////////////////////////////////////////////////////////////////////////////
///
/// \file spectrumWorxEditor.hpp
/// ----------------------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef spectrumWorxEditor_hpp__3D67D57C_4EAA_4263_8FA1_C8CA61C7A539
#define spectrumWorxEditor_hpp__3D67D57C_4EAA_4263_8FA1_C8CA61C7A539
//------------------------------------------------------------------------------
#include "core/host_interop/parameters.hpp"
#include "core/parameterID.hpp"
#include "gui/gui.hpp"
#include "gui/editor/auxiliaryComponents.hpp"
#include "gui/editor/moduleMenuHolder.hpp"
#include "gui/preset_browser/presetBrowser.hpp"

#include "le/parameters/lfoImpl.hpp" //...mrmlj...member typedefs...
#include "le/parameters/parametersUtilities.hpp"
#include "le/utility/criticalSection.hpp"
#include "le/utility/cstdint.hpp"
#include "le/utility/platformSpecifics.hpp"
#include "le/utility/objcfwdhelpers.hpp"

#include <juce_gui_basics/juce_gui_basics.h>
#include "le/utility/intrusivePtr.hpp"

#include <array>
#include <utility>
#include <optional>
//------------------------------------------------------------------------------
//...mrmlj...should be deduced based on protocol through metaprogramming...
#if defined(__APPLE__)
struct OpaqueWindowPtr;
typedef struct OpaqueWindowPtr *WindowPtr;
typedef WindowPtr WindowRef;
#elif defined(_WIN32)
struct HWND__;
typedef struct HWND__ *HWND;
#endif
namespace boost
{
template <class T> class intrusive_ptr;
}
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace SW
{
//------------------------------------------------------------------------------

class Module;
class ModuleGUI;
class SpectrumWorx;

class AutomatedModuleChain;

class Program;

//------------------------------------------------------------------------------
namespace GUI
{
//------------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
///
/// \class SpectrumWorxEditor
///
/// \brief Non-template/non-plugin platform dependent part of the
/// SpectrumWorxEditor.
///
////////////////////////////////////////////////////////////////////////////////

class SpectrumWorxEditor
    final :
      private ReferenceCountedGUIInitializationGuard,
      public WidgetBase<>,
      public juce::DragAndDropContainer,
      private juce::Button::Listener
{
  public:
    static unsigned short const estimatedWidth = 563;
    static unsigned short const estimatedHeight = 376;

  public: //...mrmlj...VST 2.4 editor dummy implementation...
    static bool setKnobMode(int) { return false; }

    static bool onKeyDown(char, int, int) { return false; }
    static bool onKeyUp(char, int, int) { return false; }

    static void idle() {}

  public:
    SpectrumWorxEditor();
    ~SpectrumWorxEditor();

#if defined(_WIN32)
    void attachToHostWindow(HWND parentWindow);
#elif defined(__APPLE__)
    void attachToHostWindow(ObjC::NSView *parentWindow);
#if !defined(__x86_64__)
    void attachToHostWindow(WindowRef parentWindow);
    //...mrmlj...seems to be needed after all...reinvestigate...
  private:
    ObjC::NSWindow *pCocoaHostWindow_;

  public:
#endif // !__x86_64__
#endif // platform

  public:
    static SpectrumWorxEditor &fromChild(juce::Component const &);
    static SpectrumWorxEditor &fromPresetBrowser(PresetBrowser &);

    Engine::Setup const &engineSetup() const;
    AutomatedModuleChain &moduleChain();
    AutomatedModuleChain const &moduleChain() const;

    bool loadPreset(juce::File const &, bool ignoreExternalSample, juce::String &comment,
                    juce::String const &presetName);
    void savePreset(juce::File const &, bool ignoreExternalSample,
                    juce::String const &comment) const;
    bool presetLoadingInProgress() const;
    char const *currentProgramName() const;

    ///...mrmlj...cleanup:
  public:
    SpectrumWorx &effect();
    SpectrumWorx const &effect() const;

  private:
    using Module = SW::Module;

    using Host = SpectrumWorx;
    //using Host = Plugin2HostInteropControler;
    Host &host();
    Host const &host() const;

    Program &program();
    Program const &program() const;

    SpectrumWorx &moduleChainOwner() { return effect(); }
    SpectrumWorx const &moduleChainOwner() const { return effect(); }

    SpectrumWorx &owner() { return effect(); }

    Utility::CriticalSectionLock getProcessingLock() const;


  private:
  public: //...mrmlj...FMOD...
    /// \note Workarounds for Clang to force lazy template instantiations so
    /// that this header does not require a full definition of the SpectrumWorx
    /// class (when Host and Effect are in fact SpectrumWorx).
    ///                                       (02.07.2014.) (Domagoj Saric)
    template <class Parameter, class Host>
    static void globalParameterChanged(Host &host, typename Parameter::value_type const value,
                                       bool const asDiscreteGesture)
    {
        host.template globalParameterChanged<Parameter>(Parameter(value), asDiscreteGesture);
    }
    template <class Parameter, class Effect>
    static bool setGlobalParameter(Effect &effect, typename Parameter::value_type const value)
    {
        return Effect::template setGlobalParameter<Parameter>(effect, value);
    }

  public: // for EditorKnob
    void mainKnobDragStarted(std::uint8_t parameterIndex) const;
    void mainKnobDragStopped(std::uint8_t parameterIndex) const;

    template <class Parameter>
    bool globalParameterChanged(typename Parameter::value_type const value,
                                bool const asDiscreteGesture)
    {
        if (!setGlobalParameter<Parameter>(value))
            return false;
        this->globalParameterChanged<Parameter>(host(), Parameter(value), asDiscreteGesture);
        return true;
    }

    template <class Parameter> bool setGlobalParameter(typename Parameter::value_type const value)
    {
        return this->setGlobalParameter<Parameter>(moduleChainOwner(), value);
    }

  private:
    template <class Parameter> void updateGlobalParameterWidget();

  public:
    void updateActiveControlValue();

#ifndef LE_SW_DISABLE_SIDE_CHANNEL
    void updateSampleName();
    void updateSampleNameAsync();
#endif // LE_SW_DISABLE_SIDE_CHANNEL

    void updateForGlobalParameterChange();

    void updateForEngineSetupChanges();

    void updateForNewTimingInfo();
    void updateLFO(ModuleUI const &, std::uint8_t parameterIndex, std::uint8_t lfoParameterIndex,
                   /*LFO::AutomatedParameterValue*/ float value);

    void moduleActivated();
    void moduleDeactivated();
    void moduleControlActivated(ModuleControlBase &, double minimum, double maximum,
                                double interval);
    void moduleControlDectivated(ModuleControlBase const &);

    ParameterID moduleControlID(ModuleControlBase const &) const;

    bool sharedModuleControlsActive() const { return sharedModuleControls_.has_value(); }
    bool sharedModuleControlsActiveAndFocused() const
    {
        return sharedModuleControlsActive() && sharedModuleControls_->hasFocus();
    }

    void updateModuleParameterAndNotifyHost(ModuleUI &, std::uint_fast8_t moduleParameterIndex,
                                            float parameterValue) const;

    void createChainGUIs(AutomatedModuleChain &);
    void destroyChainGUIs(AutomatedModuleChain &);

    void moduleRemoved() { setLastModulePosition(nextAvailableModuleSlot_ - 1); }
    void moduleAdded() { setLastModulePosition(nextAvailableModuleSlot_ + 1); }

  public: //...mrmlj...needed at end of preset loading...
    void setLastModulePosition(std::uint8_t slotIndex);

  private:
    void newSampleFileSelected(juce::File const &);

    void showSettings(unsigned int pageIndexToActivate);
    void updateSettings();

    void updateMainKnobs();

  private: // JUCE Component overrides.
    void mouseDown(juce::MouseEvent const &) override;
    void paint(juce::Graphics &) override;

    LE_IMPLEMENT_ASYNC_REPAINT

  private: // JUCE ButtonListener overrides.
    void buttonClicked(juce::Button *) override;

  private:
    void addUserAddedModule(std::uint8_t effectIndex);
    void moveModules(ModuleUI &targetSlotUI, std::uint8_t numberOfModules, std::int16_t offset);
    std::pair<LE::Utility::IntrusivePtr<Module>, std ::int8_t>
    setModuleInSlot(std::uint8_t slotIndex, std::int8_t effectIndex);

    void setActiveModuleName(juce::String const &newName);
    void setActiveControlName(juce::String const &newName);
    void setActiveControlValue(juce::String const &newValue);

#ifndef LE_SW_DISABLE_SIDE_CHANNEL
    void updateSampleName(juce::String const &);
    void setSampleLoadingStatus();
#endif // LE_SW_DISABLE_SIDE_CHANNEL
    void setDefaultFocusHandling();

    static void togglePresetBrowser(juce::Button const &);

  private:
    enum String
    {
        activeModuleName = 0,
        activeModuleDescription,
        activeControlName = activeModuleDescription,
        activeControlValue,
        currentSampleName,

        numberOfStrings
    };

    juce::String &string(String const stringID) { return strings_[stringID]; }

    void updateString(String, unsigned int stringVerticalOffset, unsigned int stringHeight,
                      juce::String const &);

  private:
    friend class ModuleUI;
    void moduleDrag(ModuleUI &, juce::MouseEvent const &);
    void moduleDragEnd(ModuleUI &, juce::MouseEvent const &);

    void removeModule(ModuleUI &);

    SharedModuleControls &sharedModuleControls() { return *sharedModuleControls_; }

  private:
    ////////////////////////////////////////////////////////////////////////////
    /// \internal
    /// \class ModuleMenuButton
    ////////////////////////////////////////////////////////////////////////////

    class ModuleMenuButton final : public BitmapButton
    {
      public:
        ModuleMenuButton(SpectrumWorxEditor &parent);
        void moveToSlot(std::uint8_t slotIndex);

      private: // JUCE component overrides.
        void clicked() override;
    }; // class ModuleMenuButton

    ////////////////////////////////////////////////////////////////////////////
    /// \internal
    /// \class Gradient
    ////////////////////////////////////////////////////////////////////////////

    /// \note Holds a ColourGradient rather than deriving from one: JUCE 8 marks
    /// it final.
    class Gradient final : public WidgetBase
    {
      public:
        Gradient(juce::Component &parent);
        ~Gradient() {}

      private: // JUCE component overrides.
        void paint(juce::Graphics &) override;

      private:
        juce::ColourGradient gradient_;
    }; // class Gradient

  public:
#ifndef LE_SW_DISABLE_SIDE_CHANNEL
    ////////////////////////////////////////////////////////////////////////////
    /// \internal
    /// \class SampleArea
    ////////////////////////////////////////////////////////////////////////////

    class SampleArea : public WidgetBase
    {
      public:
        SampleArea();

      private: // JUCE Component overrides.
        void mouseUp(juce::MouseEvent const &) override;

      private:
        SpectrumWorxEditor &editor();
    }; // class SampleArea
#endif // LE_SW_DISABLE_SIDE_CHANNEL

    ////////////////////////////////////////////////////////////////////////////
    /// \internal
    /// \class LFODisplay
    ////////////////////////////////////////////////////////////////////////////

    class LFODisplay : public WidgetBase,
                       private juce::Button::Listener,
                       private juce::Slider::Listener
    {
      public: //...mrmlj...
        //class LE_NOVTABLE AsyncSlider : public WidgetBase<juce::Slider> { LE_IMPLEMENT_ASYNC_REPAINT };
        using AsyncSlider = juce::Slider;
        using LFO = LE::Parameters::LFOImpl;

        class Period : public AsyncSlider
        {
          public:
            Period() : lastSyncType_(LFO::Free) {}

            double milliseconds() const;

            LFO::SyncType lastSyncType() const { return lastSyncType_; }

          private: // JUCE component overrides.
            friend class LFODisplay;
            /// \note JUCE 8 passes a DragMode where 2016 passed a bool. The
            /// implementation ignored it either way.
            double snapValue(double attemptedValue, DragMode) override;

          private:
            LFODisplay const &parent() const;

          private:
            LFO::SyncType lastSyncType_;
        }; // class Period

      public:
        LFODisplay();
        ~LFODisplay();

        void setupForControl(ModuleControlBase &, double minimum, double maximum, double interval);

        void updateForNewTimingInfo();
        void updateForChangedParameters(ModuleUI const &, std::uint8_t parameterIndex,
                                        std::uint8_t lfoParameterIndex,
                                        /*Plugins::AutomatedParameterValue*/ float);

        LFO const &lfo() const { return const_cast<LFODisplay &>(*this).lfo(); }
        ModuleControlBase const &control() const
        {
            return const_cast<LFODisplay &>(*this).control();
        }
        Period const &period() const { return period_; }

      private: // JUCE component overrides.
        void paint(juce::Graphics &) override;

        void buttonClicked(juce::Button *) override;
        void sliderValueChanged(juce::Slider *) noexcept override;

        LE_IMPLEMENT_ASYNC_REPAINT

      private:
        void updateAllControls();
        void updateAutomatableControls();
        void updatePeriodControl();
        void updateRangeControl();
        void updateSnapControls();
        void updateLFOAndHostFromPeriodControl();

        void automatedParameterChanged(std::uint8_t lfoParameterIndex, float parameterValue) const;
        template <class LFOParameter, typename T>
        void updateParameterAndNotifyHost(T const widgetValue)
        {
            using namespace LE::Parameters;
            using value_type = typename LFOParameter::value_type;
            auto const parameterValue(Math::convert<value_type>(widgetValue));
            lfo().parameters().set<LFOParameter>(parameterValue);
            auto const parameterIndex(IndexOf<LFO::Parameters, LFOParameter>::value);
            //...mrmlj...fmod/separated DSP-GUI...
            if (parameterIndex >= ParameterCounts::lfoExportedParameters)
                return;
            auto const internalValue(Math::convert<float>(parameterValue));
            automatedParameterChanged(parameterIndex, internalValue);
        }

        void verifyGUIAndLFOConsistency() const;

        std::uint8_t moduleIndex() const;

        SpectrumWorxEditor &editor();
        SpectrumWorxEditor const &editor() const;

        LE_NOINLINE ModuleControlBase::LFO &lfo() { return control().lfo(); }
        ModuleControlBase &control()
        {
            LE_ASSERT(pModuleControl_);
            return *pModuleControl_;
        }

      private:
        BitmapButton switch_;
        TextButton quarter_;
        TextButton triplet_;
        TextButton dotted_;
        BitmapButton typeArrow_;
        Period period_;
        AsyncSlider phase_;
        AsyncSlider range_;

        PopupMenuWithSelection type_;

        ModuleControlBase *pModuleControl_;

        static unsigned int const width = 116;

        typedef juce::Component LFODisplay::*ComponentPtr;
        static ComponentPtr const componentsToDisableKeyboardGrabingFor[];
    }; // class LFODisplay

    ////////////////////////////////////////////////////////////////////////////
    /// \internal
    /// \class Settings
    ////////////////////////////////////////////////////////////////////////////

    class Settings : public juce::TabbedComponent,
                     private juce::Slider::Listener,
                     private juce::Button::Listener,
                     public OwnedWindow<Settings>
    {
      public:
        Settings();
        ~Settings();

        void updateEnginePage();

        static void comboBoxValueChanged(ComboBox const &);

        SpectrumWorxEditor &editor();
        juce::Component &window()
        {
            return *this;
        } //...mrmlj...required because of the focus chemistry in PresetBrowser (which disables proper implicit cast to juce::Component)...

      private:
        void refillFrameSize(Engine::Setup const &);
        void updateLoadLastSessionOnStartup();

      private: // JUCE component overrides.
        void paint(juce::Graphics &) override {}
        juce::TabBarButton *createTabButton(juce::String const &tabName, int tabIndex) override;

      private: // JUCE ButtonListener overrides.
        void buttonClicked(juce::Button *) override;

      private: // JUCE SliderListener overrides.
        void sliderValueChanged(juce::Slider *) noexcept override;

      private:
        class EnginePage : public BackgroundImage
        {
          public:
            EnginePage();

            void setNewQualityFactor(float const &qualityFactor);

          private: // JUCE component overrides.
            void paint(juce::Graphics &) override;

          private:
            juce::String engineQuality_;
        }; // class EnginePage

        class InterfacePage : public BackgroundImage
        {
          public:
            InterfacePage();

            juce::Slider const &opacitySlider() const { return globalOpacity_; }
            TitledComboBox const &mouseOverComboBox() const { return moduleUIMouseOverReaction_; }
            TitledComboBox const &lfoUpdateComboBox() const { return lfoUpdateBehaviour_; }

          private: // JUCE component overrides.
            void paint(juce::Graphics &) override;

          private:
            friend class Settings;
            static unsigned int const opacityWidth = 136;

            DrawableText opacityTitle_;

            juce::Slider globalOpacity_;
            TitledComboBox moduleUIMouseOverReaction_;
            TitledComboBox lfoUpdateBehaviour_;
            LEDTextButton loadLastSessionOnStartup_;
            LEDTextButton hideCursorOnKnobDrag_;
        }; // class InterfacePage

        class AboutPage : public BackgroundImage
        {
          public:
            AboutPage();

          private: // JUCE component overrides.
            void paint(juce::Graphics &) override;

          private:
            friend class Settings;
            DrawableText const versionText_;
            BitmapButton showUsersGuide_;
        }; // class AboutPage

        EnginePage enginePage_;
        InterfacePage interfacePage_;
        AboutPage aboutPage_;

        DiscreteParameterComboBox fftSize_;
        DiscreteParameterComboBox overlapFactor_;
        DiscreteParameterComboBox windowFunction_;
#if LE_SW_ENGINE_WINDOW_PRESUM
        DiscreteParameterComboBox windowSizeFactor_;
#endif // LE_SW_ENGINE_WINDOW_PRESUM
#if LE_SW_ENGINE_INPUT_MODE >= 1
        DiscreteParameterComboBox inputMode_;
#endif // LE_SW_ENGINE_INPUT_MODE

      public:
        static std::uint8_t const xMargin = 20;
        static std::uint8_t const yMargin = 20;
        static std::uint8_t const yStep = 45;
    }; // class Settings

  private:
    std::uint8_t nextAvailableModuleSlot_;

    EditorKnob in_, out_, mix_;

    ModuleMenuHolder const moduleMenu_;
    ModuleMenuButton moduleMenuButton_;
    Gradient gradient_;

#ifndef LE_SW_DISABLE_SIDE_CHANNEL
  public: //...mrmlj...SpectrumWorx::sampleLoadingLoop()
    SampleArea sampleArea_;

  private:
#endif // LE_SW_DISABLE_SIDE_CHANNEL

    BitmapButton preset_;
    BitmapButton settingsButton_;

    // Optional/auxiliary components
    friend class OwnedWindowBase;
    friend class SharedModuleControls;
    std::optional<SharedModuleControls> sharedModuleControls_;
    std::optional<LFODisplay> lfoDisplay_;
    std::optional<PresetBrowser> presetBrowser_;
    std::optional<Settings> settings_;

    mutable bool holdSharedModuleControls_;
    mutable bool holdLFODisplay_;

    std::array<juce::String, numberOfStrings> strings_;

}; // class SpectrumWorxEditor

//------------------------------------------------------------------------------
} // namespace GUI
//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // spectrumWorxEditor_hpp
