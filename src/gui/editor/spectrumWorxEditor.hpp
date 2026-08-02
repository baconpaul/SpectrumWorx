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
#ifndef LE_NO_PRESETS
#include "gui/preset_browser/presetBrowser.hpp"
#endif // !LE_NO_PRESETS

#include "le/parameters/lfoImpl.hpp" //...mrmlj...member typedefs...
#include "le/parameters/parametersUtilities.hpp"
#include "le/utility/criticalSection.hpp"
#include "le/utility/cstdint.hpp"
#include "le/utility/platformSpecifics.hpp"

#include <juce_gui_basics/juce_gui_basics.h>
#include "le/utility/intrusivePtr.hpp"

#include <array>
#include <memory>
#include <utility>
#include <optional>
//------------------------------------------------------------------------------
/// \note A global-namespace forward declaration of Carbon's `WindowRef` and of
/// Win32's `HWND` stood here, for the three `attachToHostWindow` overloads
/// stage 6.4 deleted. Nothing else in the tree named either.
///                                           (01.08.2026.) (SW port)
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
class SpectrumWorxCore;
class Plugin2HostInteropControler;

class AutomatedModuleChain;

class Program;

//------------------------------------------------------------------------------
namespace GUI
{
//------------------------------------------------------------------------------

class EditorHost;

////////////////////////////////////////////////////////////////////////////////
///
/// \class SpectrumWorxEditor
///
/// \brief Non-template/non-plugin platform dependent part of the
/// SpectrumWorxEditor.
///
////////////////////////////////////////////////////////////////////////////////

class SpectrumWorxEditor final : private SkinLifetime,
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
    explicit SpectrumWorxEditor(EditorHost &);
    ~SpectrumWorxEditor();

  public:
    static SpectrumWorxEditor &fromChild(juce::Component const &);
#ifndef LE_NO_PRESETS
    static SpectrumWorxEditor &fromPresetBrowser(PresetBrowser &);
#endif // !LE_NO_PRESETS

    Engine::Setup const &engineSetup() const;
    AutomatedModuleChain &moduleChain();
    AutomatedModuleChain const &moduleChain() const;

#ifndef LE_NO_PRESETS
    bool loadPreset(juce::File const &, bool ignoreExternalSample, juce::String &comment,
                    juce::String const &presetName);
    /// \note A factory preset has no file; it comes out of the binary.
    bool loadPreset(char *inMemoryPreset, bool ignoreExternalSample, juce::String &comment,
                    juce::String const &presetName);
    void savePreset(juce::File const &, bool ignoreExternalSample,
                    juce::String const &comment) const;
    char const *currentProgramName() const;
#endif // !LE_NO_PRESETS

    bool presetLoadingInProgress() const;

  public:
    /// \note Was the SpectrumWorx VST2/AU class, recovered from this editor's
    /// own address. It is the engine now: every one of these calls asked the
    /// effect for something the engine owns.
    SpectrumWorxCore &effect();
    SpectrumWorxCore const &effect() const;

    /// The rest -- sample, presets, settings -- which only a plugin can answer.
    EditorHost &editorHost() const { return editorHost_; }

  private:
    using Module = SW::Module;

    /// \note The 2016 header carried this as a commented-out alternative to
    /// `using Host = SpectrumWorx`. It is the whole of what the editor ever
    /// wanted from the plugin in this direction, so it is the declaration now.
    using Host = Plugin2HostInteropControler;
    Host &host();
    Host const &host() const;

    Program &program();
    Program const &program() const;

    SpectrumWorxCore &moduleChainOwner() { return effect(); }
    SpectrumWorxCore const &moduleChainOwner() const { return effect(); }

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

    void updateSampleName();
    void updateSampleNameAsync();

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

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \brief Which module strip is selected, and which of its controls the mouse
    /// or the keyboard is on. One of each, per editor.
    ///
    /// \note Both were file-scope statics -- `ModuleUI::pSelectedModule_` and
    /// `ModuleControlBase::pActiveControl` -- which every instance of the plugin
    /// in a host shared. Selecting a module in one window deselected the other's,
    /// and closing a window left the survivor holding a pointer into freed
    /// storage. The 2011 note on the second one argued a static was safe because
    /// only one window can have focus; that is true and is not the question.
    ///                                       (02.08.2026.) (SW port)
    ///
    ////////////////////////////////////////////////////////////////////////////

    ModuleUI *selectedModule() const { return pSelectedModule_; }
    ModuleControlBase *activeControl() const { return pActiveControl_; }

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

    /// \brief Where the preset browser and the settings panel go.
    ///
    /// \note Both are 191 x 363 and there is exactly one place in a 563 x 376
    /// editor that will take one: over the module strips, right edge flush with
    /// theirs. The left column is 213 px wide and every pixel of it is spoken
    /// for -- the in/out/mix knobs, the module-info and LFO column, and the two
    /// buttons that open these panels, which an overlay must not cover or there
    /// is no way to shut it again. So the panels share one rectangle and only
    /// one of them is ever open. Stage 6.4; the alternative was to grow the
    /// editor while a panel is up, which needs a host that honours a resize
    /// request and 6.6 ships non-resizable.
    ///                                       (01.08.2026.) (SW port)
    /// \note overlayX is the module strips' right edge less overlayWidth, and
    /// the .cpp static_asserts it against ModuleUI's own constants rather than
    /// this header taking a dependency on moduleUI.hpp for three numbers.
    static unsigned short const overlayWidth = 191;
    static unsigned short const overlayHeight = 363;
    static unsigned short const overlayX = 362;
    static unsigned short const overlayY = (estimatedHeight - overlayHeight) / 2;

  private:
    void newSampleFileSelected(juce::File const &);

    /// \brief Parents \p panel to the editor at the overlay rectangle, on top.
    void openOverlay(juce::Component &panel);

    void updateSettings();

    void updateMainKnobs();

  private: // JUCE Component overrides.
    void mouseDown(juce::MouseEvent const &) override;
    void paint(juce::Graphics &) override;
    void parentHierarchyChanged() override;

    LE_IMPLEMENT_ASYNC_REPAINT

  private: // JUCE ButtonListener overrides.
    void buttonClicked(juce::Button *) override;

  public:
    /// \brief What the add-module menu calls when an entry is chosen.
    ///
    /// \note Public so that sw-show-ui can drive it. Adding a module is not one
    /// step but five -- create, build the region, take focus, select, notify --
    /// and only the first two are reachable through the module chain. Everything
    /// that has gone wrong here has gone wrong in the other three, so a harness
    /// that stops short of them is not testing the thing that breaks.
    ///                                       (29.07.2026.) (SW port)
    void addUserAddedModule(std::uint8_t effectIndex);

    /// \brief togglePresetBrowser() with the button taken out of it.
    ///
    /// \note Public for the same reason as addUserAddedModule() above: the
    /// presets button is private and its handler recovers the editor *from* the
    /// button, neither of which a headless render has. `tools/show-ui`'s
    /// "editor-presets" page is the caller, and it exists because this button
    /// asserted on its first real press with nothing headless covering it.
    ///                                       (31.07.2026.) (SW port)
    void showPresetBrowser(bool show);

    /// \brief Opens the browser on a factory bank, as double-clicking into one
    /// does. `tools/show-ui` only; see showPresetBrowser().
    void showFactoryBank(juce::String const &bank);

    /// \brief Opens the settings panel on \p pageIndexToActivate, as the
    /// settings button does. Public for the same reason showPresetBrowser() is:
    /// the button is private and its handler recovers the editor from it.
    void showSettings(unsigned int pageIndexToActivate);

  private:
    void moveModules(ModuleUI &targetSlotUI, std::uint8_t numberOfModules, std::int16_t offset);
    std::pair<LE::Utility::IntrusivePtr<Module>, std ::int8_t>
    setModuleInSlot(std::uint8_t slotIndex, std::int8_t effectIndex);

    void setActiveModuleName(juce::String const &newName);
    void setActiveControlName(juce::String const &newName);
    void setActiveControlValue(juce::String const &newValue);

    void updateSampleName(juce::String const &);
    void setSampleLoadingStatus();

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
    ////////////////////////////////////////////////////////////////////////////
    /// \internal
    /// \class SampleArea
    ///
    /// \brief The "External audio" strip: what the side channel is being fed
    /// from, and the click that changes it.
    ////////////////////////////////////////////////////////////////////////////

    class SampleArea : public WidgetBase
    {
      public:
        SampleArea();

      private: // JUCE Component overrides.
        void mouseUp(juce::MouseEvent const &) override;

      private:
        SpectrumWorxEditor &editor();

        void browseForFile();

        /// \note Outlives the async file dialog it launches.
        std::unique_ptr<juce::FileChooser> fileChooser_;

        /// \note Likewise the async menu: showCenteredBelow() returns before the
        /// user has chosen, and the items have to still be there when they do.
        PopupMenu menu_;
    }; // class SampleArea

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
                     private juce::Button::Listener
    {
      public:
        Settings();
        ~Settings();

        void updateEnginePage();

        static void comboBoxValueChanged(ComboBox const &);

        SpectrumWorxEditor &editor();

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

    /// Tab indices into Settings, in addTab() order.
    enum SettingsPage : unsigned int
    {
        enginePageIndex = 0,
        interfacePageIndex,
        aboutPageIndex,
        numberOfSettingsPages
    };

  private:
    friend class ModuleControlBase;
    /// \note Written by ModuleUI::activate()/deactivate() and by
    /// ModuleControlBase::report{Active,Inactive}Control(), which are the four
    /// places that used to write the statics these replace.
    ModuleUI *pSelectedModule_{nullptr};
    ModuleControlBase *pActiveControl_{nullptr};

  private:
    /// \note First member, and a reference: everything below is built in the
    /// constructor body and reaches through it.
    EditorHost &editorHost_;

    std::uint8_t nextAvailableModuleSlot_;

    EditorKnob in_, out_, mix_;

    ModuleMenuHolder const moduleMenu_;
    ModuleMenuButton moduleMenuButton_;
    Gradient gradient_;

    /// \note Was public, for the 2016 loader thread's completion callback to
    /// reach. There is no loader thread now, so it is nobody's but the editor's
    /// and its own nested class's.
    SampleArea sampleArea_;

    BitmapButton preset_;
    BitmapButton settingsButton_;

    // Optional/auxiliary components
    friend class SharedModuleControls;
    std::optional<SharedModuleControls> sharedModuleControls_;
    std::optional<LFODisplay> lfoDisplay_;
#ifndef LE_NO_PRESETS
    std::optional<PresetBrowser> presetBrowser_;
#endif // !LE_NO_PRESETS
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
