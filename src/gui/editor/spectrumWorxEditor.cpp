////////////////////////////////////////////////////////////////////////////////
///
/// spectrumWorxEditor.cpp
/// ----------------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "spectrumWorxEditor.hpp"

#include "external_audio/sample.hpp"

#include "core/automatedModuleChain.hpp"
#include "core/host_interop/plugin2Host.hpp"
#include "core/modules/moduleDSPAndGUI.hpp"
#include "core/spectrumWorxCore.hpp"
#include "gui/editor/editorHost.hpp"
#include "gui/editor/editorModuleInitialiser.hpp"
#include "gui/editor/presetLoading.hpp"

#include "le/parameters/lfo.hpp"
#include "le/parameters/printer.hpp"
#include "le/parameters/uiElements.hpp"
#include "le/spectrumworx/presetFile.hpp"
#include "le/spectrumworx/presets.hpp"
#include "le/utility/countof.hpp"
#include "le/utility/parentFromMember.hpp"

#include <juce_gui_basics/juce_gui_basics.h>

#include "le/utility/assert.hpp"
#include "le/utility/polymorphicDowncast.hpp"
#include "le/utility/intrusivePtr.hpp"
#include "le/utility/ignoreUnused.hpp"

#include <array>
#include <optional>
#include <string_view>
#include "le/utility/span.hpp"
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace SW
{
//------------------------------------------------------------------------------
namespace GUI
{
//------------------------------------------------------------------------------

//...mrmlj...the specialized/optimized fillComboBoxForParameter<>() helpers no
//...longer go through the generic Parameters::print<>() function so we also
//...have to provide a specialization of the fillComboBoxForParameter()
//...function template to get the overlap factor in percentages in the settings
//...window...
//...clean this up...
template <> void fillComboBoxForParameter<Engine::OverlapFactor>(ComboBox &comboBox)
{
    //...mrmlj...
#if defined(__clang__) && defined(_DEBUG)
    Engine::Setup const *pEngineSetup(nullptr);
    ++pEngineSetup; //...mrmlj...workaround for clang's -fcatch-undefined-behavior...
#else
    static Engine::Setup const *const pEngineSetup(nullptr);
#endif // _DEBUG

    using Parameter = Engine::OverlapFactor;
    std::array<char, 20> buffer;
    Parameter::value_type value(Parameter::minimum());
    while (value <= Parameter::maximum())
    {
        using LE::Parameters::DisplayValueTransformer;
        using LE::Parameters::print;
        print<Parameter>(value, const_cast<Engine::Setup const &>(*pEngineSetup),
                         LE::Utility::makeSpan(&buffer[0], buffer.size()));
        std::strcat(&buffer[0], DisplayValueTransformer<Engine::OverlapFactor>::Suffix::c_str());
        comboBox.addItem(value, &buffer[0]);
        value *= 2;
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// Constants private to this module.
// ---------------------------------
//
////////////////////////////////////////////////////////////////////////////////

namespace Constants
{
namespace Layout
{
unsigned int const textBoxHorizontalOffset = 76;
unsigned int const textBoxHeight = 22;
unsigned int const textBoxWidth = 113;

unsigned int const moduleNameVerticalOffset = 13;
unsigned int const controlNameVerticalOffset = 42;
unsigned int const controlValueVerticalOffset = 53;
unsigned int const sampleNameVerticalOffset = 306;
} // namespace Layout
} //namespace Constants

#pragma warning(push)
#pragma warning(disable : 4355) // 'this' used in base member initializer list.

SpectrumWorxEditor::SpectrumWorxEditor(EditorHost &editorHost)
    : editorHost_(editorHost), nextAvailableModuleSlot_(0),

      in_(*this, 18, 37), out_(*this, 18, 110), mix_(*this, 18, 185),

      moduleMenuButton_(*this), gradient_(*this),

      /// \todo Reimplement these Alex's widgets.
      ///                                       (22.09.2009.) (Domagoj Saric)
      //cSamplerDisplay ( CRect( 0, 0, 235, 129 ).offset( 222, 17 ), this, kBankSelect, 0, &resourceBitmap<kbLoad>(), &resourceBitmap<kbDel>(), &resourceBitmap<kbLock>() ),
      //cSpectrumDisplay( CRect( 0, 0, 235, 129 ).offset( 222, 17 ), this, SpectrumDisplay, 0, capture ),

      // buttons...
      preset_(*this, resourceBitmap<PresetOn>(), resourceBitmap<PresetOff>()),
      settingsButton_(*this, resourceBitmap<SettingsOn>(), resourceBitmap<SettingsOff>()),

      holdSharedModuleControls_(false), holdLFODisplay_(false)
{
    using LE::Parameters::IndexOf;
    using namespace GlobalParameters;
    using GlobalParameters::Parameters;
    in_.setupForParameter(IndexOf<Parameters, InputGain>::value, InputGain ::minimum(),
                          InputGain ::maximum(), InputGain ::default_());
    out_.setupForParameter(IndexOf<Parameters, OutputGain>::value, OutputGain ::minimum(),
                           OutputGain ::maximum(), OutputGain ::default_());
    mix_.setupForParameter(IndexOf<Parameters, MixPercentage>::value, MixPercentage::minimum(),
                           MixPercentage::maximum(), MixPercentage::default_());

    updateMainKnobs();
    LE_ASSERT(!settings_);

    // Implementation note:
    //   A sample may have already been loaded, either at startup using the last
    // session "preset" or through the GUI that was then destroyed and is now
    // being recreated.
    //                                        (10.06.2010.) (Domagoj Saric)
    updateSampleNameAsync();

    /// \note The focus grab that was here moved to parentHierarchyChanged().
    /// A component can only take focus once it is on screen, and in 2016 the
    /// editor was constructed by a plugin that had already parented it. The
    /// CLAP shim builds it first and parents it after, so grabbing here asserts
    /// in JUCE and does nothing.
    ///                                       (29.07.2026.) (SW port)
    setDefaultFocusHandling();

    // Resizable VST GUI discussions:
    // http://www.kvraudio.com/forum/viewtopic.php?t=141313
    // http://lists.steinberg.net:8100/Lists/vst-plugins/Message/17785.html
    // http://www.u-he.com/vstsource
    setSizeFromImage(*this, resourceBitmap<EditorBackground>());

    gradient_.setInvisible();
    gradient_.setSize(ModuleUI::width, ModuleUI::height);
    moduleMenuButton_.moveToSlot(0);

    sampleArea_.setBounds(75, 307, 115, 20);

    preset_.setTopLeftPosition(74, 338);
    settingsButton_.setTopLeftPosition(134, 338);

    preset_.addListener(this);
    settingsButton_.addListener(this);

#ifdef LE_NO_PRESETS
    /// \note There is no preset browser to open, so the button says so rather
    /// than swallowing the click. Goes with the flag.
    preset_.setEnabled(false);
#endif // LE_NO_PRESETS

    createChainGUIs(moduleChain());

    setOpaque(true);
    setVisible();

    // Last: nothing may reach a half-built editor.
    editorHost_.editorOpened(*this);
}

#pragma warning(pop)

SpectrumWorxEditor::~SpectrumWorxEditor()
{
    LE_ASSERT(GUI::isThisTheGUIThread());

    // First: nothing may reach a dying editor.
    editorHost_.editorClosed();

    editorHost_.deregisterSampleLoadedListener(*this);

    while (static_cast<bool const volatile &>(holdLFODisplay_))
    {
    }
    while (static_cast<bool const volatile &>(holdSharedModuleControls_))
    {
    }

    /// \note
    ///   Take the focus beforehand to workaround JUCE's problematic focus
    /// handling (while taking the focus it will refocus the last focused
    /// component if it was a child of the component that is taking focus, IOW
    /// it will refocus the ModuleUI being destroyed, just what we are trying
    /// to avoid).
    ///                                       (13.01.2012.) (Domagoj Saric)
    LE_ASSERT(getWantsKeyboardFocus());
    LE_ASSERT(getMouseClickGrabsKeyboardFocus());
    // Only meaningful while on screen, and JUCE asserts otherwise.
    if (isShowing() || isOnDesktop())
        grabKeyboardFocus();
    destroyChainGUIs(moduleChain());

    /// \note
    ///   Required now that std::optional does not mark itself as
    /// uninitialised in its destructor so the PresetBrowser would think that
    /// the Settings window still exists (and vice verse) in its destructor.
    ///                                       (12.01.2012.) (Domagoj Saric)
    //...mrmlj...think of a cleaner solution...
    settings_ = std::nullopt;
#ifndef LE_NO_PRESETS
    presetBrowser_ = std::nullopt;
#endif // !LE_NO_PRESETS
}

/// \note `attachToHostWindow` had three overloads here -- a Win32 SetParent, a
/// Cocoa NSView one and a 32 bit Carbon HIView one -- and no callers on any
/// platform: the CLAP shim parents the editor. Deleted with the rest of the
/// owned-window machinery in stage 6.4, and they took `-framework Carbon` with
/// them.
///                                           (01.08.2026.) (SW port)

/// \note Walks up to the nearest enclosing editor rather than to the top-level
/// component. Those were the same thing in 2016: VST 2.4 and AU parented the
/// editor straight into the host's window, so "the top" *was* the editor.
/// clap-wrapper's JUCE shim nests it two deep instead --
/// implDesktop -> implHolder -> editor (clap_juce_shim_impl.cpp) -- so the old
/// walk landed on implDesktop, and the downcast asserted in a checked build and
/// silently produced a bad pointer in a release one. Every widget that asks its
/// editor for the engine setup went through here, so this failed as soon as a
/// module's widgets were built.
///                                           (29.07.2026.) (SW port)
SpectrumWorxEditor &SpectrumWorxEditor::fromChild(juce::Component const &widget)
{
    LE_ASSERT(widget.getParentComponent());
    for (auto *pParent(widget.getParentComponent()); pParent;
         pParent = pParent->getParentComponent())
    {
        if (auto *const pEditor = dynamic_cast<SpectrumWorxEditor *>(pParent))
            return *pEditor;
    }
    LE_UNREACHABLE_CODE();
}

#ifndef LE_NO_PRESETS
SpectrumWorxEditor &SpectrumWorxEditor::fromPresetBrowser(PresetBrowser &presetBrowser)
{
    return Utility::ParentFromOptionalMember<SpectrumWorxEditor, PresetBrowser,
                                             &SpectrumWorxEditor::presetBrowser_, false>()(
        presetBrowser);
}
#endif // !LE_NO_PRESETS

Engine::Setup const &SpectrumWorxEditor::engineSetup() const
{
    return effect().uncheckedEngineSetup();
}

AutomatedModuleChain &SpectrumWorxEditor::moduleChain() { return effect().moduleChain(); }
AutomatedModuleChain const &SpectrumWorxEditor::moduleChain() const
{
    return effect().moduleChain();
}

SpectrumWorxCore &SpectrumWorxEditor::effect() { return editorHost_.core(); }
SpectrumWorxCore const &SpectrumWorxEditor::effect() const
{
    return const_cast<SpectrumWorxEditor &>(*this).effect();
}

SpectrumWorxEditor::Host &SpectrumWorxEditor::host() { return editorHost_.automation(); }
SpectrumWorxEditor::Host const &SpectrumWorxEditor::host() const
{
    return const_cast<SpectrumWorxEditor &>(*this).host();
}

Program &SpectrumWorxEditor::program() { return effect().program(); }
Program const &SpectrumWorxEditor::program() const { return effect().program(); }

Utility::CriticalSectionLock SpectrumWorxEditor::getProcessingLock() const
{
    return effect().getProcessingLock();
}

void SpectrumWorxEditor::togglePresetBrowser(juce::Button const &button)
{
    auto &editor(SpectrumWorxEditor::fromChild(button));
    LE_ASSERT(editor.getPeer());
    editor.showPresetBrowser(button.getToggleState());
}

////////////////////////////////////////////////////////////////////////////////
//
// SpectrumWorxEditor::openOverlay()
// ---------------------------------
//
////////////////////////////////////////////////////////////////////////////////
///
/// \note The whole of what stage 6.4 replaced ~500 lines of OwnedWindowBase
/// with. The panel is an ordinary child; the only thing that needed saying is
/// where it goes and that it goes on top -- gradient_ raises itself to always-
/// on-top for a module drag, and a stale one would otherwise paint through this.
///                                           (01.08.2026.) (SW port)
///
////////////////////////////////////////////////////////////////////////////////

void SpectrumWorxEditor::openOverlay(juce::Component &panel)
{
    static_assert(overlayX == ModuleUI::horizontalOffset +
                                  SW::Constants::maxNumberOfModules *
                                      (ModuleUI::width + ModuleUI::distance) -
                                  overlayWidth,
                  "the overlay's right edge is the module strips' right edge");

    LE_ASSERT(panel.getWidth() == overlayWidth);
    LE_ASSERT(panel.getHeight() == overlayHeight);

    /// \note The whole of the "one rectangle, one panel" rule, in the one place
    /// both callers pass through. Both toggle buttons feed this, and a host or a
    /// harness can reach showSettings()/showPresetBrowser() without touching
    /// either button, so the invariant belongs here rather than in the handlers.
#ifndef LE_NO_PRESETS
    LE_ASSERT_MSG(!(settings_.has_value() && presetBrowser_.has_value()),
                  "the settings panel and the preset browser share one rectangle");
#endif // !LE_NO_PRESETS
    LE_ASSERT(!panel.getParentComponent());

    panel.setTopLeftPosition(overlayX, overlayY);
    addAndMakeVisible(panel);
    panel.toFront(false);
}

/// \note The two panels share one rectangle, so opening either shuts the other
/// and un-toggles its button.
void SpectrumWorxEditor::showPresetBrowser(bool const show)
{
    if (show)
    {
        settings_ = std::nullopt;
        settingsButton_.setToggleState(false, juce::dontSendNotification);
        presetBrowser_.emplace();
        openOverlay(*presetBrowser_);
    }
    else
    {
        presetBrowser_ = std::nullopt;
    }
}

void SpectrumWorxEditor::showFactoryBank(juce::String const &bank)
{
    showPresetBrowser(true);
    presetBrowser_->setFactoryBank(bank);
}

void SpectrumWorxEditor::setDefaultFocusHandling()
{
    setWantsKeyboardFocus(true);
    setMouseClickGrabsKeyboardFocus(true);
}

/// \note Where the constructor's focus grab went. Fires when the host's window
/// takes the editor, which is the first moment it can hold focus.
void SpectrumWorxEditor::parentHierarchyChanged()
{
    if (isShowing() || isOnDesktop())
        grabKeyboardFocus();
}

void SpectrumWorxEditor::moduleDrag(ModuleUI &moduleUI, juce::MouseEvent const &event)
{
    if (isDragAndDropActive())
    {
        unsigned int const halfModuleWidth(ModuleUI::width / 2);

        juce::Rectangle<int> const &sourceRect(moduleUI.getBounds());

        juce::Rectangle<int> const leftRect(ModuleUI::horizontalOffset, ModuleUI::verticalOffset,
                                            sourceRect.getX() -
                                                (halfModuleWidth + ModuleUI::distance) - 1 -
                                                ModuleUI::horizontalOffset,
                                            ModuleUI::height);

        unsigned int const emptyModuleSpaceBegin(moduleMenuButton_.getX() - 4);
        unsigned int const rightRectStart(sourceRect.getRight() + ModuleUI::distance +
                                          halfModuleWidth + 1);
        unsigned int const rightRectEnd(emptyModuleSpaceBegin + halfModuleWidth);
        juce::Rectangle<int> const rightRect(rightRectStart, ModuleUI::verticalOffset,
                                             rightRectEnd - rightRectStart, ModuleUI::height);

        juce::Point<int> const mousePosition(
            this->getLocalPoint(nullptr, event.getScreenPosition()));

        bool const showGradient(leftRect.contains(mousePosition) ||
                                rightRect.contains(mousePosition));

        if (showGradient)
        {
            unsigned int const firstGradientOffset(ModuleUI::horizontalOffset -
                                                   (halfModuleWidth + (ModuleUI::distance / 2)));

            unsigned int const gradientIndex((mousePosition.getX() - firstGradientOffset) /
                                             (ModuleUI::width + ModuleUI::distance));
            LE_ASSERT(gradient_.getHeight() == ModuleUI::height);
            LE_ASSERT(gradient_.getWidth() == ModuleUI::width);

            unsigned int const gradientOffset(
                firstGradientOffset + (gradientIndex * (ModuleUI::width + ModuleUI::distance)));
            gradient_.setTopLeftPosition(gradientOffset, ModuleUI::verticalOffset);
            LE_ASSERT(!gradient_.getBounds().intersects(sourceRect));
        }
        gradient_.setIsVisible(showGradient);
    }
    else
    {
        gradient_.toFront(true);
        gradient_.setAlwaysOnTop(true);

        startDragging(juce::var(), &moduleUI);
    }
}

void SpectrumWorxEditor::moduleDragEnd(ModuleUI &moduleUI, juce::MouseEvent const &event)
{
    juce::Point<int> const mousePosition(this->getLocalPoint(nullptr, event.getScreenPosition()));

    bool const dragAborted(!gradient_.isVisible() ||
                           !gradient_.getBounds().contains(mousePosition));
    gradient_.setInvisible();
    /// \note moduleDrag() raises this to always-on-top and nothing lowered it,
    /// so a panel opened after any drag painted underneath it. Only matters now
    /// the panels are children rather than desktop windows.
    ///                                       (01.08.2026.) (SW port)
    gradient_.setAlwaysOnTop(false);
    if (dragAborted)
        return;

    /// \note We have to block automation here because of FMOD's MVC
    /// implementation in which it responds to
    /// EDITOR_TO_HOST_SET_PARAMETER_VALUE calls (part of the below
    /// host().modulesChanged() calls) by immediately calling
    /// HOST_TO_EDITOR_UPDATE_PARAMETER_VALUE which in turn, coupled with the
    /// "dependent parameter caching hack-mechanism", breaks the module chain
    /// contents while it is being traversed.
    ///                                       (20.10.2014.) (Domagoj Saric)
    Host2PluginInteropControler::AutomationBlocker const automationBlocker(
        /*host*/ moduleChainOwner /*mrmlj*/ ());

    LE_ASSERT(!gradient_.getBounds().intersects(moduleUI.getBounds()));
    unsigned int const slotWidth(ModuleUI::width + ModuleUI::distance);
    unsigned int const sourceX(moduleUI.getX());
    unsigned int targetX(gradient_.getX());
    bool const moveLeft(static_cast<unsigned int>(gradient_.getRight()) < sourceX);
    int const gradientToTargetOffset((ModuleUI::width + ModuleUI::distance) / 2);
    targetX -= moveLeft ? -gradientToTargetOffset : +gradientToTargetOffset;
    LE_ASSERT((signed(targetX - sourceX) % signed(slotWidth)) == 0);
    moduleUI.setTopLeftPosition(targetX, ModuleUI::verticalOffset);

    int const offset(moveLeft ? +static_cast<int>(slotWidth) : -static_cast<int>(slotWidth));

    std::uint8_t const sourceIndex((sourceX - ModuleUI::horizontalOffset) / slotWidth);
    std::uint8_t const targetIndex((targetX - ModuleUI::horizontalOffset) / slotWidth);
    //...mrmlj...
    //if ( sourceIndex > targetIndex )
    //    std::swap( sourceIndex, targetIndex );

    moveModules(moduleUI, Math::abs(targetIndex - sourceIndex), offset);
    auto &moduleChain(this->moduleChain());
    moduleChain.moveModule(sourceIndex, targetIndex);
    host().gestureBegin("Drag module");
    host().modulesChanged(moduleChain, sourceIndex, targetIndex);
    host().gestureEnd();
}

void SpectrumWorxEditor::setLastModulePosition(std::uint_fast8_t const slotIndex)
{
    LE_ASSERT(slotIndex <= SW::Constants::maxNumberOfModules);
    nextAvailableModuleSlot_ = slotIndex;
    moduleMenuButton_.moveToSlot(slotIndex);
}

namespace
{
#pragma warning(push)
#pragma warning(disable : 4510) // Default constructor could not be generated.
#pragma warning(disable                                                                            \
                : 4610) // Class can never be instantiated - user-defined constructor required.

struct EditorMainAreaText
{
    juce::String const *pText;
    juce::Font const *pFont;
    juce::Colour const colour;
    unsigned int const verticalOffset;
    juce::Justification const justification;
    unsigned int const textLinesToUse;
}; // struct EditorMainAreaText

#pragma warning(pop)

// Implementation note:
//   To prevent the "static initialisation order fiasco" (occurring with
// Clang 2.8 on OS X) we do not use the JUCE static Colours::white object
// but construct our own white juce::Colour here.
//                                        (25.01.2011.) (Domagoj Saric)
EditorMainAreaText mainAreaTexts[] = {
    {0, 0, Theme::blueColour(), Constants::Layout::moduleNameVerticalOffset,
     juce::Justification::centred, 1}, // active module name
    {0, 0, juce::Colour(0xFFFFFFFF), Constants::Layout::controlNameVerticalOffset,
     juce::Justification::top | juce::Justification::horizontallyCentred, 2}, // control name
    {0, 0, juce::Colour(0xFFFFFFFF), Constants::Layout::controlValueVerticalOffset,
     juce::Justification::centred, 1}, // control value
    {0, 0, Theme::blueColour(), Constants::Layout::sampleNameVerticalOffset,
     juce::Justification::centred, 1}, // sample name
};

void drawMainAreaText(juce::Graphics &graphics, EditorMainAreaText const &text)
{
    using namespace Constants::Layout;

    graphics.setColour(text.colour);
    graphics.setFont(*text.pFont);
    graphics.drawFittedText(*text.pText, textBoxHorizontalOffset, text.verticalOffset, textBoxWidth,
                            textBoxHeight * text.textLinesToUse, text.justification,
                            text.textLinesToUse);
}
} //anonymous namespace

void SpectrumWorxEditor::paint(juce::Graphics &graphics)
{
    GUI::paintImage(graphics, resourceBitmap<EditorBackground>());

    juce::Font const &moduleNameFont(Theme::singleton().blueFont());
    juce::Font const &sampleNameFont(DrawableText::defaultFont());
    juce::Font const &controlTextFont(Theme::singleton().whiteFont());

    mainAreaTexts[0].pText = &string(activeModuleName);
    mainAreaTexts[0].pFont = &moduleNameFont;
    mainAreaTexts[1].pText = &string(activeControlName);
    mainAreaTexts[1].pFont = &controlTextFont;
    mainAreaTexts[2].pText = &string(activeControlValue);
    mainAreaTexts[2].pFont = &controlTextFont;
    mainAreaTexts[3].pText = &string(currentSampleName);
    mainAreaTexts[3].pFont = &sampleNameFont;

    for (auto const &text : mainAreaTexts)
        drawMainAreaText(graphics, text);
}

void SpectrumWorxEditor::buttonClicked(juce::Button *const pButton)
{
    if (pButton == &settingsButton_)
    {
        if (settingsButton_.getToggleState())
        {
            showSettings(0);
        }
        else
        {
            LE_ASSERT(settings_);
            settings_ = std::nullopt;
        }
    }
#ifndef LE_NO_PRESETS
    else
    {
        LE_ASSERT(pButton == &preset_);
        togglePresetBrowser(*pButton);
    }
#endif // !LE_NO_PRESETS
}

void LE_NOINLINE SpectrumWorxEditor::updateString(String const stringID,
                                                  unsigned int const stringVerticalOffset,
                                                  unsigned int const stringHeight,
                                                  juce::String const &updatedString)
{
    string(stringID) = updatedString;

    using namespace Constants::Layout;
    repaint(textBoxHorizontalOffset, stringVerticalOffset, textBoxWidth, stringHeight);
}

void SpectrumWorxEditor::setActiveModuleName(juce::String const &newName)
{
    using namespace Constants::Layout;
    updateString(activeModuleName, moduleNameVerticalOffset, textBoxHeight, newName);
}

void SpectrumWorxEditor::setActiveControlName(juce::String const &newName)
{
    using namespace Constants::Layout;
    updateString(activeControlName, controlNameVerticalOffset, textBoxHeight * 4, newName);
}

void SpectrumWorxEditor::setActiveControlValue(juce::String const &newValue)
{
    using namespace Constants::Layout;
    updateString(activeControlValue, controlValueVerticalOffset, textBoxHeight, newValue);
}

void SpectrumWorxEditor::updateActiveControlValue()
{
    try
    {
        LE_ASSERT(lfoDisplay_);
        LFODisplay const &lfoDisplay(/*static_cast<LFODisplay const &>*/ (*lfoDisplay_));
        if (lfoDisplay.lfo().enabled())
            setActiveControlValue("[LFO]");
        else
            setActiveControlValue(lfoDisplay.control().getValueText());
    }
    catch (...)
    {
    }
}

void SpectrumWorxEditor::updateSampleName(juce::String const &newSampleName)
{
    using namespace Constants::Layout;
    updateString(currentSampleName, sampleNameVerticalOffset, textBoxHeight, newSampleName);
}

void SpectrumWorxEditor::updateSampleName()
{
    updateSampleName(editorHost_.currentSampleFile().getFileNameWithoutExtension());
}

/// \note "Async" is 2016's, and the branch it names is currently unreachable:
/// loading happens on this thread inside setNewSample(), so the host is never
/// mid-load when it is asked. Kept whole rather than collapsed to
/// updateSampleName(), because whether the loader gets a thread again is the
/// threading redesign's decision and this is the shape it would come back in.
///                                           (01.08.2026.) (SW port)
void SpectrumWorxEditor::updateSampleNameAsync()
{
    if (editorHost_.isSampleLoadInProgress())
    {
        editorHost_.registerSampleLoadedListener(*this);
        setSampleLoadingStatus();
    }
    else
    {
        sampleArea_.setVisible();
        updateSampleName();
    }
}

void SpectrumWorxEditor::setSampleLoadingStatus()
{
    sampleArea_.setInvisible();
    updateSampleName("Loading...");
}

void SpectrumWorxEditor::newSampleFileSelected(juce::File const &file)
{
    editorHost_.setNewSample(file);
    updateSampleNameAsync();
}

void SpectrumWorxEditor::removeModule(ModuleUI &moduleUI)
{
    static_assert(ModuleUI::width % 2 == 0, "Only even width modules supported.");
    static_assert(ModuleUI::distance % 2 == 0, "Only even width modules supported.");

    /// \note See the note for the equivalent statement in the moduleDragEnd()
    /// member function.
    ///                                       (20.10.2014.) (Domagoj Saric)
    Host2PluginInteropControler::AutomationBlocker const automationBlocker(
        /*host*/ moduleChainOwner /*mrmlj*/ ());

    LE_ASSUME(nextAvailableModuleSlot_ != 0);
    auto const slotWidth(ModuleUI::width + ModuleUI::distance);
    auto const offset(-signed(slotWidth));
    auto const slot((moduleUI.getX() - ModuleUI::horizontalOffset) / slotWidth);
    auto const firstModuleIndex(slot);
    auto const lastModuleIndex(nextAvailableModuleSlot_ - 1);
    moveModules(moduleUI, lastModuleIndex - firstModuleIndex, offset);
    moduleRemoved();
    LE_VERIFY(setModuleInSlot(slot, AutomatedModuleChain::noModule).first == nullptr);
    host().gestureBegin("Remove module");
    host().modulesChanged(moduleChain(), firstModuleIndex, lastModuleIndex);
    host().gestureEnd();
}

void SpectrumWorxEditor::moveModules(ModuleUI &targetSlotUI, std::uint8_t numberOfModules,
                                     std::int16_t const offset)
{
    //...mrmlj...
    typedef Engine::ModuleNode ModuleNode;
    typedef std::remove_reference<decltype(targetSlotUI.module())>::type Module;
    ModuleNode::NodePtr ModuleNode::*const pNextPtr((offset < 0) ? &ModuleNode::next_
                                                                 : &ModuleNode::previous_);
    //...mrmlj...internal module chain knowledge...
    auto *LE_RESTRICT pMovedModule(&targetSlotUI.module());
    while (numberOfModules--)
    {
        pMovedModule = &Engine::actualModule<Module>(*(Engine::node(*pMovedModule).*pNextPtr));
        LE_ASSUME(pMovedModule); //...msvc...
        auto &gui(*pMovedModule->gui());
        gui.setTopLeftPosition(gui.getX() + offset, ModuleUI::verticalOffset);
    }
}

std::pair<LE::Utility::IntrusivePtr<SpectrumWorxEditor::Module>, std::int8_t>
SpectrumWorxEditor::setModuleInSlot(std::uint8_t const slotIndex, std::int8_t const effectIndex)
{
    /// \note The editor's own initialiser, not the core's: filling a slot from
    /// here has to build the module's UI region as well as its buffers, and the
    /// core half cannot reach the GUI. addUserAddedModule() depends on it having
    /// happened by the time this returns.
    EditorModuleInitialiser const initialise{moduleChainOwner().moduleInitialiser(), this};
    return moduleChainOwner().moduleChain().setParameter(slotIndex, effectIndex, initialise);
}

void SpectrumWorxEditor::addUserAddedModule(std::uint8_t const effectIndex)
{
    // Implementation note:
    //   This is certainly executed from the GUI thread so this function expects
    // the module creation to be done synchronously, in order for the focus
    // grabbing to be safe.
    //                                        (06.07.2011.) (Domagoj Saric)
    LE_ASSERT(isThisTheGUIThread());
    // Implementation note:
    //   We want any user-added module (using the add module menu) to
    // automatically gain focus.
    //                                        (09.02.2010.) (Domagoj Saric)
#ifdef _WIN32
    /// \note There was a runDispatchLoopUntil( 2 ) here, pumping the message
    /// queue before taking focus. It was a 2013 quick-fix for crashes in
    /// SoundForge 10, whose own diagnosis -- recorded at the time -- was that
    /// SetFocus() let Windows deliver a queued message that called
    /// loadProgramState() in the middle of adding a module. That is a re-entrancy
    /// hazard being treated by draining the queue *first*, which is the same
    /// hazard a moment earlier.
    ///
    ///   It cannot survive the port in any case: runDispatchLoopUntil() only
    /// exists when JUCE_MODAL_LOOPS_PERMITTED is 1, and this build sets it to 0 --
    /// the premise of the stage 6 rewrite that made the menus and dialogs
    /// asynchronous. A nested dispatch loop in the middle of a slot change is
    /// precisely what that setting exists to forbid.
    ///
    ///   If SoundForge 10 ever matters again, the fix is to make the add-module
    /// path re-entrancy-safe rather than to pick a quieter moment for it.
    ///                                       (30.07.2026.) (SW port)
    LE_ASSERT(getWantsKeyboardFocus());
    LE_ASSERT(getMouseClickGrabsKeyboardFocus());
    this->grabKeyboardFocus();
#endif // _WIN32

    auto const result(setModuleInSlot(nextAvailableModuleSlot_, effectIndex));
    std::int8_t const actualEffectIndex(result.second);
    if (actualEffectIndex == effectIndex) //...mrmlj...
    {
        LE_ASSERT(result.first);
        std::uint8_t const changedSlot(nextAvailableModuleSlot_);
        moduleAdded();
        /// \note Checked rather than assumed. setModuleInSlot() builds the region
        /// synchronously and this is the GUI thread, so it is there -- unless
        /// createGUI() threw, which it swallows in a release build. This used to
        /// be an unconditional `gui()->`, which on an empty optional is undefined
        /// behaviour rather than a missing knob.
        LE_ASSERT(result.first->gui());
        if (result.first->gui())
            result.first->gui()->grabKeyboardFocus();
        host().gestureBegin("Add module");
        host().moduleChangedByUser(changedSlot, result.first.get());
        host().gestureEnd();
    }
    else
    { // failed module creation
        LE_ASSERT(result.first == nullptr);
        LE_ASSERT(result.second == noModule);
    }
}

#ifndef LE_NO_PRESETS
bool SpectrumWorxEditor::loadPreset(juce::File const &presetFile, bool const ignoreExternalSample,
                                    juce::String &comment, juce::String const &presetName)
{
    auto const pPresetName(presetName.getCharPointer().getAddress());
    return GUI::loadPreset(editorHost_, this, presetFile, ignoreExternalSample, &comment,
                           pPresetName);
}

bool SpectrumWorxEditor::loadPreset(char *const inMemoryPreset, bool const ignoreExternalSample,
                                    juce::String &comment, juce::String const &presetName)
{
    auto const pPresetName(presetName.getCharPointer().getAddress());
    return GUI::loadPreset(editorHost_, this, inMemoryPreset, ignoreExternalSample, &comment,
                           pPresetName);
}

void SpectrumWorxEditor::savePreset(juce::File const &presetFile, bool const ignoreExternalSample,
                                    juce::String const &comment) const
{
    juce::File const externalSample(ignoreExternalSample ? juce::File()
                                                         : editorHost_.currentSampleFile());
    SW::savePreset(presetFile, externalSample, comment, program());
}

char const *SpectrumWorxEditor::currentProgramName() const { return program().name().data(); }
#endif // !LE_NO_PRESETS

/// \note Not preset machinery despite the name: the flag lives on the engine
/// side and guards automation while a whole program is being swapped in.
bool SpectrumWorxEditor::presetLoadingInProgress() const
{
    return static_cast<Host2PluginInteropControler const &>(
               moduleChainOwner()) /*...mrmlj...*/.presetLoadingInProgress();
}

void SpectrumWorxEditor::moduleActivated()
{
    LE_ASSERT(isThisTheGUIThread());
    LE_ASSERT(selectedModule());
    ModuleUI const &module(*selectedModule());
    setActiveModuleName(module.getName());
    if (!activeControl())
    {
        setActiveControlName(module.description());
        setActiveControlValue(juce::String());
    }

    /// \note
    ///   See the note in the moduleDeactivated() member function for an
    /// explanation as to why we expect the SharedModuleControls instance to
    /// possibly be already created.
    ///                                       (17.01.2012.) (Domagoj Saric)
    if (!sharedModuleControls_)
        sharedModuleControls_.emplace();
    else
        sharedModuleControls_->setEnabled(true);
    sharedModuleControls_->updateForActiveModule();
}

void SpectrumWorxEditor::moduleDeactivated()
{
    LE_ASSERT(selectedModule());

    // Implementation note:
    //   We need to prevent JUCE from transferring focus to other module UIs
    // when it destroys the currently active ModuleUI and/or the
    // SharedModuleControls instance as that would cause a call to
    // moduleActivated() while we are still in the SharedModuleControls
    // destructor which in turn would cause another (reentrant) call to the
    // SharedModuleControls destructor. This is accomplished by first
    // transferring focus to the editor window if the module being deactivated
    // (and possibly destroyed) is currently focused.
    //                                        (03.01.2012.) (Domagoj Saric)
    LE_ASSERT(this->getWantsKeyboardFocus());
    if (selectedModule()->juce::Component::isParentOf(getCurrentlyFocusedComponent()))
    {
        this->grabKeyboardFocus();
        LE_ASSERT(hasDirectFocus());
    }

    LE_ASSERT_MSG(!lfoDisplay_ || !lfoDisplay_->isEnabled(), "Module controls not deactivated.");

    setActiveModuleName(juce::String());
    setActiveControlName(juce::String());
    setActiveControlValue(juce::String());

    if (sharedModuleControls_)
    {
        /// \note We defer the destruction of the SharedModuleControls instance
        /// so that we can avoid the destruction+recreation in case the user is
        /// actually only activating a different module.
        ///                                   (17.01.2012.) (Domagoj Saric)
        sharedModuleControls_->setEnabled(false);
        GUI::postMessageToComponent(*this, [](GUI::SpectrumWorxEditor &editor) {
            auto &sharedModuleControls(editor.sharedModuleControls_);
            if (sharedModuleControls && !sharedModuleControls->isEnabled())
            {
                if (static_cast<bool const volatile &>(editor.holdSharedModuleControls_))
                    return false;
                else
                    sharedModuleControls = std::nullopt;
            }
            return true;
        });
    }
}

ParameterID SpectrumWorxEditor::moduleControlID(ModuleControlBase const &control) const
{
    ParameterID parameterID;
    parameterID.value.type = ParameterID::ModuleParameter;
    parameterID.value._.module.moduleIndex = moduleChain().getIndexForModule(control.module());
    parameterID.value._.module.moduleParameterIndex = control.moduleParameterIndex();
    return parameterID;
}

void SpectrumWorxEditor::moduleControlActivated(ModuleControlBase &control, double const minimum,
                                                double const maximum, double const interval)
{
    /// \note
    ///   In addition to the reason given for the SharedModuleControls instance
    /// in the moduleActivated()/moduleDeactivated() member functions, the
    /// LFODisplay instance can also be expected to be already created here
    /// because of the SharedModuleControls::FrequencyRange control (because
    /// one of its thumbs can be activated w/o first deactivating the other).
    ///                                       (17.01.2012.) (Domagoj Saric)
    if (!lfoDisplay_)
        lfoDisplay_.emplace();
    else
        lfoDisplay_->setEnabled(true);

    lfoDisplay_->setupForControl(control, minimum, maximum, interval);

    setActiveModuleName(control.moduleUI().getName());
    setActiveControlName(control.widget().getName());
    updateActiveControlValue();

    host().automatedParameterBeginEdit(moduleControlID(control));
}

void SpectrumWorxEditor::moduleControlDectivated(ModuleControlBase const &control)
{
    LE_ASSERT(lfoDisplay_);
    LE_ASSERT_MSG((&static_cast<LFODisplay const &>(*lfoDisplay_).control() == &control),
                  "Deactivating active module control through a wrong control.");
    LE::Utility::ignoreUnused(control);

    setActiveControlName(selectedModule() ? selectedModule()->description() : juce::String());
    setActiveControlValue(juce::String());

    if (lfoDisplay_)
    {
        /// \note See the note in the moduleDeactivated() member function.
        ///                                   (17.01.2012.) (Domagoj Saric)
        setDefaultFocusHandling();
        lfoDisplay_->setEnabled(false);
        /// \note We defer LFODisplay destruction so that we can avoid the
        /// destruction+recreation in case the user is actually only switching
        /// between controls.
        ///                                   (02.09.2013.) (Domagoj Saric)
        postMessageToComponent(*this, [](GUI::SpectrumWorxEditor &editor) {
            auto &lfoDisplay(editor.lfoDisplay_);
            if (lfoDisplay && !lfoDisplay->isEnabled())
            {
                if (static_cast<bool const volatile &>(editor.holdLFODisplay_))
                    return false;
                else
                    lfoDisplay = std::nullopt;
                LE_ASSERT(editor.getWantsKeyboardFocus());
                LE_ASSERT(editor.getMouseClickGrabsKeyboardFocus());
            }
            return true;
        });
    }

    host().automatedParameterEndEdit(moduleControlID(control));
}

void SpectrumWorxEditor::mainKnobDragStarted(std::uint8_t const index) const
{
    ParameterID parameterID;
    parameterID.value.type = ParameterID::GlobalParameter;
    parameterID.value._.global.index = index;
    host().automatedParameterBeginEdit(parameterID);
}

void SpectrumWorxEditor::mainKnobDragStopped(std::uint8_t const index) const
{
    ParameterID parameterID;
    parameterID.value.type = ParameterID::GlobalParameter;
    parameterID.value._.global.index = index;
    host().automatedParameterEndEdit(parameterID);
}

/// \note Moved here from gui.cpp. It instantiates globalParameterChanged<>,
/// which reaches host() and so needs the complete SpectrumWorx; leaving it in
/// the widget layer meant every widget translation unit pulled in the 2016 VST2
/// plugin class and the deleted VST 2.4 SDK behind it.
///                                       (28.07.2026.) (SW port)
void EditorKnob::valueChanged() noexcept
{
    using LE::Parameters::IndexOf;
    using namespace GlobalParameters;
    typedef GlobalParameters::Parameters GlobalParams;
    auto &editor(this->editor());
    auto const &value(this->getValue());
    switch (parameterIndex_)
    {
    case IndexOf<GlobalParams, InputGain>::value:
        LE_VERIFY(editor.globalParameterChanged<InputGain>(value, false));
        break;
    case IndexOf<GlobalParams, OutputGain>::value:
        LE_VERIFY(editor.globalParameterChanged<OutputGain>(value, false));
        break;
    case IndexOf<GlobalParams, MixPercentage>::value:
        LE_VERIFY(editor.globalParameterChanged<MixPercentage>(value, false));
        break;
        LE_DEFAULT_CASE_UNREACHABLE();
    }
}

void SpectrumWorxEditor::createChainGUIs(AutomatedModuleChain &chain)
{
    // http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2012/n3424.pdf
    std::uint8_t moduleIndex(0);
    chain.forEach<Module>([&](Module &module) mutable { module.createGUI(*this, moduleIndex++); });
    setLastModulePosition(moduleIndex);
}

void SpectrumWorxEditor::destroyChainGUIs(AutomatedModuleChain &chain)
{
    chain.forEach<Module>([](Module &module) { module.destroyGUI(); });
    setLastModulePosition(0);
}

void SpectrumWorxEditor::mouseDown(juce::MouseEvent const &event)
{
    juce::Rectangle<int> const logoArea(12, 290, 51, 63);
    if (logoArea.contains(event.x, event.y))
    {
        /// \note Was 3, and there are three tabs. JUCE clamps an out of range
        /// index to -1, so clicking the logo raised the panel with *no* page
        /// selected -- an empty transparent window over the desktop, which is
        /// why nobody saw it, and an empty panel over the editor now. The About
        /// page this means is index 2.
        ///                                   (01.08.2026.) (SW port)
        showSettings(aboutPageIndex);
    }
}

void SpectrumWorxEditor::showSettings(unsigned int const pageIndexToActivate)
{
    if (!settings_.has_value())
    {
#ifndef LE_NO_PRESETS
        presetBrowser_ = std::nullopt;
        preset_.setToggleState(false, juce::dontSendNotification);
#endif // !LE_NO_PRESETS
        settings_.emplace();
        openOverlay(*settings_);
    }
    settings_->setCurrentTabIndex(pageIndexToActivate, false);
    settingsButton_.setToggleState(true, juce::dontSendNotification);
}

void SpectrumWorxEditor::updateSettings()
{
    if (settings_.has_value())
        settings_->updateEnginePage();
}

void SpectrumWorxEditor::updateMainKnobs()
{
    auto const &parameters(effect().parameters());
    using namespace GlobalParameters;
    in_.setValue(parameters.get<InputGain>());
    out_.setValue(parameters.get<OutputGain>());
    mix_.setValue(parameters.get<MixPercentage>());
}

void SpectrumWorxEditor::updateForGlobalParameterChange()
{
    updateMainKnobs();
    updateSettings();
}

using namespace GlobalParameters;
template <> void SpectrumWorxEditor::updateGlobalParameterWidget<FFTSize>()
{
    updateSettings();
    updateForEngineSetupChanges();
}
template <> void SpectrumWorxEditor::updateGlobalParameterWidget<OverlapFactor>()
{
    updateSettings();
    updateForEngineSetupChanges();
}
template <> void SpectrumWorxEditor::updateGlobalParameterWidget<WindowFunction>()
{
    updateSettings();
    updateForEngineSetupChanges();
}
#if LE_SW_ENGINE_WINDOW_PRESUM
template <> void SpectrumWorxEditor::updateGlobalParameterWidget<WindowSizeFactor>()
{
    updateSettings();
    updateForEngineSetupChanges();
}
#endif // LE_SW_ENGINE_WINDOW_PRESUM
#if LE_SW_ENGINE_INPUT_MODE >= 2
template <> void SpectrumWorxEditor::updateGlobalParameterWidget<InputMode>() { updateSettings(); }
#endif // LE_SW_ENGINE_INPUT_MODE >= 2
template <> void SpectrumWorxEditor::updateGlobalParameterWidget<InputGain>() { updateMainKnobs(); }
template <> void SpectrumWorxEditor::updateGlobalParameterWidget<OutputGain>()
{
    updateMainKnobs();
}
template <> void SpectrumWorxEditor::updateGlobalParameterWidget<MixPercentage>()
{
    updateMainKnobs();
}

void SpectrumWorxEditor::updateForEngineSetupChanges()
{
    Engine::Setup const &engineSetup(this->engineSetup());
    holdSharedModuleControls_ = true;
    if (sharedModuleControlsActive())
        sharedModuleControls().updateForEngineSetupChanges(engineSetup);
    holdSharedModuleControls_ = false;
    moduleChain().forEach<Module>([&](Module &module) {
#ifndef LE_SW_FMOD
        //...mrmlj...when switching programs...
        LE_ASSERT(module.gui());
        if (module.gui())
#endif // LE_SW_FMOD
            module.gui()->updateForEngineSetupChanges(engineSetup);
    });
}

void SpectrumWorxEditor::updateForNewTimingInfo()
{
    // This gets called from a non GUI thread.
    LE_ASSERT(!holdLFODisplay_);
    holdLFODisplay_ = true;
    if (lfoDisplay_ && lfoDisplay_->isEnabled())
        lfoDisplay_->updateForNewTimingInfo();
    holdLFODisplay_ = false;
}

void SpectrumWorxEditor::updateLFO(ModuleUI const &moduleUI, std::uint8_t const parameterIndex,
                                   std::uint8_t const lfoParameterIndex,
                                   Plugins::AutomatedParameterValue const value)
{
    // This gets called from a non GUI thread.
    LE_ASSERT(!holdLFODisplay_);
    holdLFODisplay_ = true;
    if (lfoDisplay_ && lfoDisplay_->isEnabled())
        lfoDisplay_->updateForChangedParameters(moduleUI, parameterIndex, lfoParameterIndex, value);
    holdLFODisplay_ = false;
}

void SpectrumWorxEditor::updateModuleParameterAndNotifyHost(ModuleUI &moduleUI,
                                                            std::uint8_t const moduleParameterIndex,
                                                            float parameterValue) const
{
    auto &module(moduleUI.module());
    std::uint8_t const moduleIndex(moduleChain().getIndexForModule(module));
    auto const snappedParameterValue(
        module.setParameterValueFromUI(moduleParameterIndex, parameterValue));
    parameterValue = snappedParameterValue;
    host().automatedParameterChanged(module, moduleIndex, moduleParameterIndex, parameterValue);
}

SpectrumWorxEditor::ModuleMenuButton::ModuleMenuButton(SpectrumWorxEditor &parent)
    : BitmapButton(parent, resourceBitmap<AddModule>(), resourceBitmap<AddModule>(),
                   Theme::singleton().blueColour())
{
}

void SpectrumWorxEditor::ModuleMenuButton::moveToSlot(std::uint8_t const slotIndex)
{
    //...mrmlj..."magic number" adjustments...
    setTopLeftPosition(4 + ModuleUI::horizontalOffset +
                           ((ModuleUI::width + ModuleUI::distance) * slotIndex),
                       (ModuleUI::verticalOffset - 4) + (ModuleUI::height / 2) - (getHeight() / 2));
    setIsVisible(slotIndex < SW::Constants::maxNumberOfModules);
}

void SpectrumWorxEditor::ModuleMenuButton::clicked()
{
    SpectrumWorxEditor &editor(
        *LE::Utility::polymorphicDowncast<SpectrumWorxEditor *>(this->getParentComponent()));
    LE_ASSERT(editor.nextAvailableModuleSlot_ < SW::Constants::maxNumberOfModules);
    /// \note The menu no longer blocks, so the editor can be torn down while it
    /// is open -- a host closing the window under it. A SafePointer to it, and
    /// the reference is only taken once the callback has proved it is alive.
    juce::Component::SafePointer<SpectrumWorxEditor> pEditor(&editor);
    editor.moduleMenu_.topMenu().showCenteredAtRight(
        *this, [pEditor](PopupMenu::OptionalID const chosenMenuEntryID) {
            if (!pEditor || !chosenMenuEntryID.has_value())
                return;
            auto &editor(*pEditor);
            LE_ASSERT(editor.moduleMenu_.isOwnerOfEntry(*chosenMenuEntryID));
            editor.addUserAddedModule(editor.moduleMenu_.effectIndexForEntry(*chosenMenuEntryID));
        });
}

SpectrumWorxEditor::Gradient::Gradient(juce::Component &parent)
    : gradient_(juce::Colours::transparentWhite, 0, 0, juce::Colours::transparentWhite,
                static_cast<float>(ModuleUI::width), 0, false)
{
    gradient_.addColour(0.5, juce::Colours::darkgrey);
    addToParentAndShow(parent, *this);
}

void SpectrumWorxEditor::Gradient::paint(juce::Graphics &graphics)
{
    graphics.setGradientFill(gradient_);
    graphics.fillAll();
}

namespace
{
using LFO = LE::Parameters::LFOImpl;

LE_NOINLINE LFO::value_type rangeSliderValueToLFOValue(juce::Slider const &slider,
                                                       double const value)
{
    return Math::convertLinearRange<LFO::value_type, LFO::minimumValue,
                                    LFO::maximumValue - LFO::minimumValue, 1, double>(
        value, slider.getMinimum(), slider.getMaximum());
}

LE_NOINLINE double lfoValueToRangeSliderValue(juce::Slider const &slider,
                                              LFO::value_type const &value)
{
    return Math::convertLinearRange<double, LFO::value_type, LFO::minimumValue,
                                    LFO::maximumValue - LFO::minimumValue, 1>(
        value, slider.getMinimum(), slider.getMaximum());
}

void fillLFOWaveformsMenu(PopupMenu &menu)
{
    static juce::Image const *LE_RESTRICT const icons[] = {
        &resourceBitmap<LFOSine>(),         &resourceBitmap<LFOTriangle>(),
        &resourceBitmap<LFOSawtooth>(),     &resourceBitmap<LFOReverseSaw>(),
        &resourceBitmap<LFOSquare>(),       &resourceBitmap<LFOExponent>(),
        &resourceBitmap<LFORandomHold>(),   &resourceBitmap<LFORandomSlide>(),
        &resourceBitmap<LFORandomWhacko>(), &resourceBitmap<LFODirac>(),
        &resourceBitmap<LFOdIRAC>()};

    //LE_ASSERT( menu.getNumItems() == 0 );...mrmlj...add size information to the new ComboBox class...
    unsigned int itemId(0);
    juce::Image const *LE_RESTRICT const *ppIcon = icons;
    for (auto const waveFormName : LE::Parameters::DiscreteValues<LFO::Waveform>::strings)
        menu.addItem(itemId++, waveFormName, **ppIcon++);
}
} // namespace

#define LE_COMP_PTR(member) reinterpret_cast<ComponentPtr>(&SpectrumWorxEditor::LFODisplay::member)
SpectrumWorxEditor::LFODisplay::ComponentPtr const
    SpectrumWorxEditor::LFODisplay::componentsToDisableKeyboardGrabingFor[] = {
        LE_COMP_PTR(switch_),  LE_COMP_PTR(phase_),   LE_COMP_PTR(range_), LE_COMP_PTR(period_),
        LE_COMP_PTR(quarter_), LE_COMP_PTR(triplet_), LE_COMP_PTR(dotted_)
        /*, this*/
};
#undef LE_COMP_PTR

#pragma warning(push)
#pragma warning(disable : 4355) // 'this' used in base member initializer list.

SpectrumWorxEditor::LFODisplay::LFODisplay()
    : switch_(*this, resourceBitmap<LEDOn>(), resourceBitmap<LEDOff>()),
      quarter_(*this, 62, 5, " N "), triplet_(*this, 62 + 18 * 1, 5, " T "),
      dotted_(*this, 62 + 18 * 2 - 2, 5, " D "),
      typeArrow_(*this, resourceBitmap<ChangeWaveform>(), resourceBitmap<ChangeWaveform>(),
                 juce::Colours::white.withAlpha(0.5f)),
      pModuleControl_(nullptr)
{
    for (auto const pComponent : componentsToDisableKeyboardGrabingFor)
    {
        juce::Component &component(this->*pComponent);
        component.setWantsKeyboardFocus(false);
        component.setMouseClickGrabsKeyboardFocus(false);
    }
    this->setWantsKeyboardFocus(false);
    this->setMouseClickGrabsKeyboardFocus(false);

    fillLFOWaveformsMenu(type_);

    switch_.setTopLeftPosition(25, 3);

    period_.setBounds(7, 32, 108, 18);
    period_.setSliderStyle(juce::Slider::LinearHorizontal);
    period_.setTextBoxStyle(juce::Slider::NoTextBox, true, 10, 12);
    //period_.setVelocityBasedMode( true );
    addToParentAndShow(*this, period_);

    phase_.setBounds(39, 118, 42, 12);
    phase_.setSliderStyle(juce::Slider::LinearHorizontal);
    phase_.setTextBoxStyle(juce::Slider::NoTextBox, true, 10, 12);
    phase_.setRange(-0.5, +0.5);
    phase_.setDoubleClickReturnValue(true, 0);
    addToParentAndShow(*this, phase_);

    typeArrow_.setTopLeftPosition(109, 99);
    typeArrow_.addListener(this);

    range_.setBounds(7, 73, width - 7, 10);
    range_.setSliderStyle(juce::Slider::TwoValueHorizontal);
    range_.setTextBoxStyle(juce::Slider::NoTextBox, true, 90, 20);
    addToParentAndShow(*this, range_);

    this->setBounds(71, 156, width, 128);

    switch_.addListener(this);
    quarter_.addListener(this);
    triplet_.addListener(this);
    dotted_.addListener(this);
    range_.addListener(this);
    period_.addListener(this);
    phase_.addListener(this);
}

#pragma warning(pop)

SpectrumWorxEditor::LFODisplay::~LFODisplay() { editor().setDefaultFocusHandling(); }

void SpectrumWorxEditor::LFODisplay::setupForControl(ModuleControlBase &control,
                                                     double const minimum, double const maximum,
                                                     double const interval)
{
    pModuleControl_ = &control;

    range_.setRange(minimum, maximum, interval);

    // Implementation note:
    //   A two-valued juce::Slider does not allow to set a max periodScale that
    // is lower than the current min periodScale and vice verse. As a workaround
    // we first set both values to their respective extremes so that the
    // juce::Slider::set(Max/Min)Value() setter function would not alter our
    // values.
    //                                        (24.03.2010.) (Domagoj Saric)
    range_.setMaxValue(range_.getMaximum(), juce::dontSendNotification, false);
    range_.setMinValue(range_.getMinimum(), juce::dontSendNotification, false);

    //range_.setSkewFactor( control.getSkewFactor() );

    updateAllControls();

    addToParentAndShow(editor(), *this);

    repaint();
}

namespace
{
bool skipPeriodRatio(SpectrumWorxEditor::LFODisplay::Period const &period)
{
    return (period.lastSyncType() == LFO::Free) || !LFO::Timer::hasTempoInformation();
}

juce::String periodRatioString(SpectrumWorxEditor::LFODisplay const &parent,
                               double const &periodScale)
{
    std::array<char, 16> buffer;

    double numerator;
    double denominator;
    char const *suffix;
    switch (parent.period().lastSyncType())
    {
    case LFO::Quarter:
        if (periodScale < 1)
        {
            numerator = 1;
            denominator = 1 / periodScale;
        }
        else
        {
            denominator = 1;
            numerator = periodScale;
        }
        suffix = "";
        break;

    case LFO::Triplet:
        if (periodScale < 1)
        {
            numerator = 1;
            denominator = 1 / (periodScale * 3 / 2);
        }
        else
        {
            denominator = 1;
            numerator = (periodScale * 3 / 2);
        }
        suffix = "T";
        break;

    case LFO::Dotted:
        if (periodScale < 1)
        {
            numerator = 1;
            denominator = 1 / (periodScale * 2 / 3);
        }
        else
        {
            denominator = 1;
            numerator = (periodScale * 2 / 3);
        }
        suffix = "D";
        break;

        LE_DEFAULT_CASE_UNREACHABLE();
    }

    unsigned int const charactersWritten(
        LE_INT_SPRINTFA(&buffer[0], "%u/%u%s bars", Math::convert<unsigned int>(numerator),
                        Math::convert<unsigned int>(denominator), suffix));
    LE_ASSERT(charactersWritten < buffer.size());
    return juce::String(&buffer[0], charactersWritten);
}

juce::String periodMillisecondsString(SpectrumWorxEditor::LFODisplay const &parent,
                                      double const &periodScale)
{
    LE_ASSUME(parent.period().milliseconds() == periodScale);

    bool const skipRatio(skipPeriodRatio(parent.period()));
    unsigned char const precision(!skipRatio * 2);

    std::array<char, 32> buffer;
    auto const charactersWritten(Utility::lexical_cast(periodScale, precision, &buffer[0]));
    std::strcpy(&buffer[charactersWritten], " ms");

    return juce::String(&buffer[0]);
}

juce::String phaseString(SpectrumWorxEditor::LFODisplay const & /*parent*/,
                         double const &periodScale)
{
    std::array<char, 32> buffer;
    auto const numberOfCharactersWritten(Utility::lexical_cast(periodScale * 100, 1, &buffer[0]));
    std::strcpy(&buffer[numberOfCharactersWritten], "%");
    return juce::String(&buffer[0]);
}

juce::String rangeValueString(SpectrumWorxEditor::LFODisplay const &parent,
                              double const &periodScale)
{
    /// \note `LE_ASSERT( activeControl() )` stood above this, over the file-scope
    /// pointer. It said "something is active"; the line below says "and it is this
    /// control", which subsumes it.
    LE_ASSERT(parent.control().isActive());
    return parent.control().getTextFromValue(static_cast<float>(periodScale));
}

#pragma warning(push)
#pragma warning(disable : 4510) // Default constructor could not be generated.
#pragma warning(disable                                                                            \
                : 4610) // Class can never be instantiated - user-defined constructor required.

struct LFOTextData
{
    typedef juce::String(StringGetter)(SpectrumWorxEditor::LFODisplay const &, double const &);

    double value;
    StringGetter *const getString;
    unsigned int const x;
    unsigned int const y;
    unsigned int const width;
    unsigned int const height;
    juce::Justification const justification;
};

#pragma warning(pop)

std::size_t const lfoWidth = 116;

LFOTextData sliderTexts[] = {
    {0, &periodRatioString, 9, 24, 105, 12, juce::Justification::right},        // period ratio
    {0, &periodMillisecondsString, 9, 47, 105, 12, juce::Justification::right}, // period ms
    {0, &rangeValueString, 9, 62, lfoWidth - 10 - 2, 12, juce::Justification::right}, // range max
    {0, &rangeValueString, 10, 84, lfoWidth - 10, 12, juce::Justification::left},     // range min
    {0, &phaseString, 9, 118, 105, 12, juce::Justification::right},                   // period ms
};

#pragma warning(push)
#pragma warning(disable : 4510) // Default constructor could not be generated.
#pragma warning(disable                                                                            \
                : 4610) // Class can never be instantiated - user-defined constructor required.

struct FixedText
{
    char const *const string;
    unsigned int const verticalPosition;
};

#pragma warning(pop)

static FixedText const fixedText[] = {
    {"Period", 24 + 9},
    {"Range", 62 + 9},
    {"Waveform", 99 + 9},
    {"Phase", 118 + 9},
};
} // anonymous namespace

void SpectrumWorxEditor::LFODisplay::paint(juce::Graphics &graphics)
{
    static_assert(lfoWidth == width, ""); //...mrmlj...clean this up...

    //...mrmlj...ugh...2.6.x quick-fix workarounds...reinvestigate and clean this up...
    if (!this->isEnabled())
        return;
    LE_ASSERT(editor().activeControl() != nullptr);
    LE_ASSERT(editor().activeControl() == &control());
    LE_ASSERT(control().isActive());
    LE_ASSERT(getParentComponent() == &editor());

    {
        graphics.setFont(DrawableText::defaultFont());
        graphics.setColour(juce::Colours::white);

        for (auto const &text : fixedText)
            graphics.drawSingleLineText(text.string, 9, text.verticalPosition);
    }

    sliderTexts[0].value = period_.getValue();
    sliderTexts[1].value = period_.milliseconds();
    sliderTexts[2].value = range_.getMaxValue();
    sliderTexts[3].value = range_.getMinValue();
    sliderTexts[4].value = phase_.getValue();

    for (auto const &text : LE::Utility::makeSpan(sliderTexts).subspan(skipPeriodRatio(period_)))
    {
        graphics.drawText(text.getString(*this, text.value), text.x, text.y, text.width,
                          text.height, text.justification, false);
    }

    paintImage(graphics, type_.getSelectedItemIcon(), 79, 96);
}

void SpectrumWorxEditor::LFODisplay::buttonClicked(juce::Button *const pButton)
{
    auto &lfo(this->lfo());

    if (pButton == &switch_)
    {
        bool const enable(switch_.getToggleState());
        LE_ASSERT(enable != lfo.enabled());
        updateParameterAndNotifyHost<LFO::Enabled>(enable);
        control().lfoStateChanged();
        editor().updateActiveControlValue();
    }
    else if (pButton == &typeArrow_)
    {
        //...mrmlj...
        if (!type_.menuActive())
        {
            juce::Component::SafePointer<LFODisplay> pThis(this);
            type_.showCenteredAtRight(typeArrow_, [pThis](bool const selectionChanged) {
                if (!pThis || !selectionChanged)
                    return;
                pThis->updateParameterAndNotifyHost<LFO::Waveform>(pThis->type_.getSelectedID());
                pThis->repaint();
            });
        }
    }
    else
    {
        LE_ASSERT(LFO::Timer::hasTempoInformation());

        LFO::SyncType syncType;
        if (pButton == &quarter_)
        {
            syncType = LFO::Quarter;
        }
        else if (pButton == &triplet_)
        {
            syncType = LFO::Triplet;
        }
        else
        {
            LE_ASSERT(pButton == &dotted_);
            syncType = LFO::Dotted;
        }

        bool const addSyncType(pButton->getToggleState());

        if (addSyncType)
            lfo.addSyncType(syncType);
        else
            lfo.removeSyncType(syncType);

        updatePeriodControl();
        updateLFOAndHostFromPeriodControl();
        this->repaint();
    }
}

void SpectrumWorxEditor::LFODisplay::sliderValueChanged(juce::Slider *const pSlider) noexcept
{
    repaint();

    if (pSlider == &range_)
    {
        auto const newLowerBound(rangeSliderValueToLFOValue(range_, range_.getMinValue()));
        auto const newUpperBound(rangeSliderValueToLFOValue(range_, range_.getMaxValue()));

        updateParameterAndNotifyHost<LFO::LowerBound>(newLowerBound);
        updateParameterAndNotifyHost<LFO::UpperBound>(newUpperBound);
    }
    else if (pSlider == &period_)
    {
        updateLFOAndHostFromPeriodControl();
    }
    else
    {
        LE_ASSERT(pSlider == &phase_);
        updateParameterAndNotifyHost<LFO::Phase>(phase_.getValue());
    }
}

void SpectrumWorxEditor::LFODisplay::updateForNewTimingInfo()
{
    updatePeriodControl();
    updateSnapControls();
    verifyGUIAndLFOConsistency();
    repaint();
}

void SpectrumWorxEditor::LFODisplay::updateForChangedParameters(
    ModuleUI const &moduleUI, std::uint8_t const parameterIndex,
    std::uint8_t const lfoParameterIndex, Plugins::AutomatedParameterValue /*const value*/)
{
    if ((&moduleUI == &control().moduleUI()) &&
        (parameterIndex == control().moduleParameterIndex()))
    {
        if (lfoParameterIndex == LE::Parameters::IndexOf<LFO::Parameters, LFO::Enabled>::value)
            control().lfoStateChanged();
        updateAutomatableControls();
        repaint();
    }
    verifyGUIAndLFOConsistency();
}

void SpectrumWorxEditor::LFODisplay::updateAllControls()
{
    updateAutomatableControls();
    updateSnapControls();
    type_.setSelectedID(lfo().waveForm());
    verifyGUIAndLFOConsistency();
}

void SpectrumWorxEditor::LFODisplay::updateAutomatableControls()
{
    updatePeriodControl();
    updateRangeControl();
    auto &lfo(this->lfo());
    switch_.setToggleState(lfo.enabled(), juce::dontSendNotification);
    phase_.setValue(lfo.phase(), juce::dontSendNotification);
}

void SpectrumWorxEditor::LFODisplay::updatePeriodControl()
{
    auto &lfo(this->lfo());

    float const rangeMinimum(LFO::PeriodScale::minimum());
    float const rangeMaximum(LFO::PeriodScale::maximum());
    double const rangeBeginning(LFO::snapPeriodScale(rangeMinimum, lfo.syncTypes()).first);
    double const rangeEnd(LFO::snapPeriodScale(rangeMaximum, lfo.syncTypes()).first);
    double const step((lfo.syncTypes() != LFO::Free) ? 0
                                                     : 1 / 1000.0 / LFO::Timer::basePeriod() // 1 ms
    );

    period_.setRange(rangeBeginning, rangeEnd, step);
    period_.setSkewFactorFromMidPoint(1);

    /// \note JUCE 8 passes a DragMode where 2016 passed a bool; the override
    /// ignores it, and notDragging is what "not a drag" spells now.
    double const resnappedValue(
        period_.Period::snapValue(lfo.periodScale(), juce::Slider::notDragging));
    lfo.setPeriodScale(static_cast<LFO::value_type>(
        resnappedValue)); //...mrmlj...rethink whether this should be done by the LFO class...
    period_.setValue(resnappedValue, juce::dontSendNotification);

    verifyGUIAndLFOConsistency();
}

void SpectrumWorxEditor::LFODisplay::updateRangeControl()
{
    auto &lfo(this->lfo());
    range_.setMaxValue(lfoValueToRangeSliderValue(range_, lfo.upperBound()),
                       juce::dontSendNotification, false);
    range_.setMinValue(lfoValueToRangeSliderValue(range_, lfo.lowerBound()),
                       juce::dontSendNotification, false);
}

void SpectrumWorxEditor::LFODisplay::updateSnapControls()
{
    if (LFO::Timer::hasTempoInformation())
    {
        auto &lfo(this->lfo());
        quarter_.setToggleState(lfo.hasEnabledSync(LFO::Quarter), juce::dontSendNotification);
        triplet_.setToggleState(lfo.hasEnabledSync(LFO::Triplet), juce::dontSendNotification);
        dotted_.setToggleState(lfo.hasEnabledSync(LFO::Dotted), juce::dontSendNotification);
    }
    else
    {
        quarter_.setEnabled(false);
        triplet_.setEnabled(false);
        dotted_.setEnabled(false);
    }
}

void SpectrumWorxEditor::LFODisplay::updateLFOAndHostFromPeriodControl()
{
    updateParameterAndNotifyHost<LFO::PeriodScale>(period_.getValue());
}

void SpectrumWorxEditor::LFODisplay::automatedParameterChanged(std::uint8_t const lfoParameterIndex,
                                                               float const parameterValue) const
{
    auto const moduleParameterIndex(control().moduleParameterIndex());

    if (moduleParameterIndex >= (SW::Constants::maxNumberOfParametersPerModule - 1))
        return;

    ParameterID::LFO const lfoParameterID = {lfoParameterIndex, moduleParameterIndex,
                                             moduleIndex()};
#ifdef LE_SW_FMOD
    Host2PluginInteropControler::AutomationBlocker const automationBlocker(
        const_cast<SpectrumWorxEditor &>(editor()).moduleChainOwner());
#endif // LE_SW_FMOD
    editor().host().automatedParameterChanged(lfoParameterID, parameterValue);
}

void SpectrumWorxEditor::LFODisplay::verifyGUIAndLFOConsistency() const
{
#ifndef NDEBUG
    //...mrmlj...
    //...mrmlj...the rounding error difference is too great even for the nearEqual() function...
    //LE_ASSERT( Math::nearEqual( lfo().periodScale(), static_cast<LFO::value_type>( period_.getValue() ) ) );
    //LE_ASSERT( Math::abs( lfo().periodScale() - period_.getValue() ) < 0.001 );
    double const guiPeriod(lfo().periodScale());
    double const lfoPeriod(period_.getValue());
    LE_ASSERT(Math::abs(guiPeriod - lfoPeriod) < 0.001);
#endif // NDEBUG
}

std::uint8_t SpectrumWorxEditor::LFODisplay::moduleIndex() const
{
    auto const moduleIndex(editor().program().moduleChain().getIndexForModule(control().module()));
    return moduleIndex;
}

SpectrumWorxEditor &SpectrumWorxEditor::LFODisplay::editor()
{
    SpectrumWorxEditor &editor(
        Utility::ParentFromOptionalMember<SpectrumWorxEditor, LFODisplay,
                                          &SpectrumWorxEditor::lfoDisplay_, false>()(*this));
    LE_ASSERT((&editor == this->getParentComponent()) || !this->getParentComponent());
    return editor;
}

SpectrumWorxEditor const &SpectrumWorxEditor::LFODisplay::editor() const
{
    return const_cast<SpectrumWorxEditor::LFODisplay &>(*this).editor();
}

double SpectrumWorxEditor::LFODisplay::Period::snapValue(double const attemptedValue,
                                                         DragMode /*dragMode*/)
{
    LFO::SnappedPeriod const result(
        LFO::snapPeriodScale(static_cast<float>(attemptedValue), parent().lfo().syncTypes()));
    lastSyncType_ = result.second;
    return result.first;
}

double SpectrumWorxEditor::LFODisplay::Period::milliseconds() const
{
#ifdef LE_SW_FMOD
    return 1000;
#else
    float const basePeriod(parent().editor().effect().lfoTimer().basePeriod());
    double const periodInMilliseconds(this->getValue() * basePeriod * 1000);
    return periodInMilliseconds;
#endif // LE_SW_FMOD
}

SpectrumWorxEditor::LFODisplay const &SpectrumWorxEditor::LFODisplay::Period::parent() const
{
    return Utility::ParentFromMember<LFODisplay, Period, &LFODisplay::period_>()(*this);
}

SpectrumWorxEditor::SampleArea::SampleArea()
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    addToParentAndShow(editor(), *this);
}

////////////////////////////////////////////////////////////////////////////////
//
// SpectrumWorxEditor::SampleArea::mouseUp()
// -----------------------------------------
//
////////////////////////////////////////////////////////////////////////////////
///
/// \note A menu rather than 2016's straight-to-the-file-dialog, because the
/// factory samples are in the binary now and a file dialog cannot show them.
/// The dialog is still one entry away, and the right button still clears, as it
/// always did.
///                                           (01.08.2026.) (SW port)
///
////////////////////////////////////////////////////////////////////////////////

void SpectrumWorxEditor::SampleArea::mouseUp(juce::MouseEvent const &event)
{
    SpectrumWorxEditor &editor(this->editor());
    juce::ModifierKeys const mouseButtons(event.mods);
    if (mouseButtons.isRightButtonDown())
    {
        editor.newSampleFileSelected(juce::File());
        return;
    }
    if (!mouseButtons.isLeftButtonDown() || PopupMenu::menuActive())
        return;

    auto const factorySamples(Sample::factorySamples());

    enum : PopupMenu::ItemID
    {
        browse = 0,
        clear,
        firstFactorySample
    };

    menu_.clear();
    menu_.addItem(browse, "Load audio file...");
    menu_.addItem(clear, "No external audio",
                  /*icon*/ juce::Image(),
                  /*enabled*/ editor.editorHost().currentSampleFile() != juce::File());
    menu_.addSectionHeader("Factory samples");
    for (std::size_t sample(0); sample < factorySamples.size(); ++sample)
        menu_.addItem(static_cast<PopupMenu::ItemID>(firstFactorySample + sample),
                      factorySamples[sample].getFileNameWithoutExtension().toRawUTF8());

    juce::Component::SafePointer<SpectrumWorxEditor> pEditor(&editor);
    menu_.showCenteredBelow(*this, [this, pEditor, factorySamples](PopupMenu::OptionalID chosen) {
        if (!pEditor || !chosen)
            return;
        switch (*chosen)
        {
        case browse:
            return browseForFile();
        case clear:
            return pEditor->newSampleFileSelected(juce::File());
        default:
            return pEditor->newSampleFileSelected(factorySamples[*chosen - firstFactorySample]);
        }
    });
}

void SpectrumWorxEditor::SampleArea::browseForFile()
{
    SpectrumWorxEditor &editor(this->editor());

    /// \note Only a real file is a place to start from: a factory sample is a
    /// bare name and no directory, and JUCE would resolve it against whatever
    /// the host's working directory happens to be.
    auto const currentFile(editor.editorHost().currentSampleFile());
    auto const startingFile(currentFile.existsAsFile()
                                ? currentFile
                                : juce::File::getSpecialLocation(juce::File::userMusicDirectory));

    /// \note Held rather than stack-allocated: launchAsync() returns
    /// immediately and the chooser must outlive the dialog.
    fileChooser_ = std::make_unique<juce::FileChooser>("Choose external audio file", startingFile,
                                                       Sample::supportedFormats(), true);
    juce::Component::SafePointer<SpectrumWorxEditor> pEditor(&editor);
    fileChooser_->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [pEditor](juce::FileChooser const &chooser) {
            if (!pEditor || chooser.getResults().isEmpty())
                return;
            LE_ASSERT(chooser.getResults().size() == 1);
            pEditor->newSampleFileSelected(chooser.getResults().getReference(0));
        });
}

SpectrumWorxEditor &SpectrumWorxEditor::SampleArea::editor()
{
    return Utility::ParentFromMember<SpectrumWorxEditor, SampleArea,
                                     &SpectrumWorxEditor::sampleArea_>()(*this);
}

////////////////////////////////////////////////////////////////////////////////
//
// SpectrumWorxEditor::Settings::Settings()
// ----------------------------------------
//
////////////////////////////////////////////////////////////////////////////////

#pragma warning(push)
#pragma warning(disable : 4355) // 'this' used in base member initializer list.

SpectrumWorxEditor::Settings::Settings() /// \throws std::bad_alloc Out of memory
    : juce::TabbedComponent(juce::TabbedButtonBar::TabsAtTop),

      fftSize_(enginePage_, xMargin, yMargin + yStep * 0, (Engine ::FFTSize *)(0)),
      overlapFactor_(enginePage_, xMargin, yMargin + yStep * 1, (Engine ::OverlapFactor *)(0)),
      windowFunction_(enginePage_, xMargin, yMargin + yStep * 2, (Engine ::WindowFunction *)(0))
/// \note pRegistrationData_(0) came last here; the member itself went with the
/// licence manager in stage 0 and the initialiser did not, so nothing has
/// compiled this constructor since. It was also the only unconditional entry
/// after this point, which is why every conditional one above it could end in a
/// comma. Leading commas instead, so that all four combinations of
/// LE_SW_ENGINE_WINDOW_PRESUM and LE_SW_ENGINE_INPUT_MODE are well formed.
///                                       (28.07.2026.) (SW port)
#if LE_SW_ENGINE_WINDOW_PRESUM
      ,
      windowSizeFactor_(enginePage_, xMargin, yMargin + yStep * 3, (Engine ::WindowSizeFactor *)(0))
#if LE_SW_ENGINE_INPUT_MODE >= 1
      ,
      inputMode_(enginePage_, xMargin, yMargin + yStep * 4, (GlobalParameters::InputMode *)(0))
#endif // LE_SW_ENGINE_INPUT_MODE
#elif LE_SW_ENGINE_INPUT_MODE >= 1
      ,
      inputMode_(enginePage_, xMargin, yMargin + yStep * 3, (GlobalParameters::InputMode *)(0))
#endif // LE_SW_ENGINE_WINDOW_PRESUM
{
    /// \note The height was editor().getHeight() -- 376 -- while what this
    /// paints is a 16 px tab bar over a 347 px page bitmap: 363. The 13 px
    /// difference was empty and invisible while this was a transparent desktop
    /// window, and would not be as an overlay over the editor. So it is the sum
    /// of what it draws, which is also what the preset browser measures.
    ///                                       (01.08.2026.) (SW port)
    this->setSize(resourceBitmap<SettingsEngineBg>().getWidth(),
                  resourceBitmap<SettingsEngineOn>().getHeight() +
                      resourceBitmap<SettingsEngineBg>().getHeight());

#if LE_SW_ENGINE_INPUT_MODE >= 2 && defined(__APPLE__)
    inputMode_->setEnabled(!editor().effect().completelyDisableIOChanges());
#elif LE_SW_ENGINE_INPUT_MODE == 1
    inputMode_->setEnabled(false);
#endif // LE_SW_ENGINE_INPUT_MODE
#if defined(LE_SW_FMOD)
    windowFunction_->setEnabled(false);
#endif
#if LE_SW_ENGINE_WINDOW_PRESUM
    windowSizeFactor_->setEnabled(false);
#endif // LE_SW_ENGINE_WINDOW_PRESUM

    updateEnginePage();
    updateLoadLastSessionOnStartup();

    aboutPage_.showUsersGuide_.addListener(this);

    setOutline(0);
    setIndent(0);
    setTabBarDepth(resourceBitmap<SettingsEngineOn>().getHeight());

    juce::String const dummyName("a");
    addTab(dummyName, juce::Colours::transparentBlack, &enginePage_, false);
    addTab(dummyName, juce::Colours::transparentBlack, &interfacePage_, false);
    addTab(dummyName, juce::Colours::transparentBlack, &aboutPage_, false);

    LE_ASSERT(getNumTabs() == numberOfSettingsPages);

    /// \note `OwnedWindow<Settings>::attach()` stood here; the editor parents
    /// and positions this now -- see SpectrumWorxEditor::openOverlay().
    ///                                       (01.08.2026.) (SW port)
}

SpectrumWorxEditor::Settings::~Settings()
{
    //...mrmlj..."desktop window" fade-out does not work with the current JUCE
    //getCurrentContentComponent()->fadeOutComponent( 200, 0, 0, 0.2f );
    //this->fadeOutComponent( 200, 0, 0, 0.2f );
    clearTabs();
}

void SpectrumWorxEditor::Settings::sliderValueChanged(juce::Slider *const pSlider) noexcept
{
    LE_ASSUME(pSlider == &interfacePage_.opacitySlider());
    Theme::singleton().settings().globalOpacity = pSlider->getValue();

    juce::Colour const tabBackground(juce::Colours::black.withAlpha(
        static_cast<float>(std::pow(Theme::singleton().settings().globalOpacity, 14))));
    LE_ASSERT(getTabbedButtonBar().getNumTabs() == 3);
    for (unsigned int i(0); i < 3; ++i)
        getTabbedButtonBar().setTabBackgroundColour(i, tabBackground);

    // Force repaint
    for (unsigned int i(0); i < static_cast<unsigned int>(juce::ComponentPeer::getNumPeers()); ++i)
    {
        juce::ComponentPeer &peer(*juce::ComponentPeer::getPeer(i));
        juce::Rectangle<int> bounds(peer.getBounds());
        bounds.setPosition(0, 0);
        peer.repaint(bounds);
    }

    LE::Utility::ignoreUnused(pSlider);
}

#pragma warning(push)
#pragma warning(disable : 4702) // Unreachable code.

void SpectrumWorxEditor::Settings::comboBoxValueChanged(ComboBox const &comboBox)
{
    auto &settings(*LE::Utility::polymorphicDowncast<Settings *>(
        comboBox.getParentComponent()->getParentComponent()));
    auto &editor(settings.editor());

    unsigned int const value(comboBox.getValue());

    using namespace GlobalParameters;
    typedef GlobalParameters::Parameters Parameters;

    if (&comboBox == &settings.fftSize_)
    {
        LE_VERIFY(
            editor.globalParameterChanged<FFTSize>(static_cast<FFTSize ::value_type>(value), true));
    }
    else if (&comboBox == &settings.overlapFactor_)
    {
        LE_VERIFY(editor.globalParameterChanged<OverlapFactor>(
            static_cast<OverlapFactor ::value_type>(value), true));
    }
    else if (&comboBox == &settings.windowFunction_)
    {
        LE_VERIFY(editor.globalParameterChanged<WindowFunction>(
            static_cast<WindowFunction ::value_type>(value), true));
    }
#if LE_SW_ENGINE_WINDOW_PRESUM
    else if (&comboBox == &settings.windowSizeFactor_)
    {
        LE_VERIFY(editor.globalParameterChanged<WindowSizeFactor>(
            static_cast<WindowSizeFactor::value_type>(value), true));
    }
#endif // LE_SW_ENGINE_WINDOW_PRESUM
#if LE_SW_ENGINE_INPUT_MODE >= 2
    else if (&comboBox == &settings.inputMode_)
    {
        LE_ASSUME(!editor.effect().completelyDisableIOChanges());
        /*LE_VERIFY*/ (editor.globalParameterChanged<InputMode>(
            static_cast<InputMode::value_type>(value), true));
        settings.updateLoadLastSessionOnStartup();
        settings.inputMode_->setValue(
            editor.program().parameters().template get<InputMode>().getValue());
    }
#endif // LE_SW_ENGINE_INPUT_MODE
    else if (&comboBox == &settings.interfacePage_.mouseOverComboBox())
    {
        Theme::singleton().settings().moduleUIMouseOverReaction =
            static_cast<Theme::ModuleUIMouseOverReaction>(value);
    }
    else if (&comboBox == &settings.interfacePage_.lfoUpdateComboBox())
    {
        Theme::singleton().settings().lfoUpdateBehaviour =
            static_cast<Theme::LFOUpdateBehaviour>(value);
    }
    else
    {
        LE_UNREACHABLE_CODE();
    }

    settings.enginePage_.setNewQualityFactor(editor.engineSetup().wolaRippleFactor());
}

#pragma warning(pop)

void SpectrumWorxEditor::Settings::updateEnginePage()
{
    auto const &editor(this->editor());
    // Implementation note:
    //   In rare circumstances this function gets called very often (if engine
    // setup parameters change rapidly, e.g. someone automates them using the
    // Ableton Live's 'dual control') and it gets called asynchronously to the
    // actual Engine::Setup instance updating. This can cause the Engine::Setup
    // instance to get 'out-of-date' which in turn would cause an assertion
    // failure if the SpectrumWorx::engineSetup() getter was used. Because
    // of this the SpectrumWorx::uncheckedEngineSetup() getter is used to
    // avoid the assertion failures.
    //   This is safe to do as a non-up-to-date engine setup is harmless here,
    // there will surely be a next message/asynchronous call when it will be up
    // to date).
    //                                        (15.06.2010.) (Domagoj Saric)
    /// \note And it was not used: the line above this note read
    /// `editor.engineSetup()`, the checked getter, which is precisely what the
    /// note says must not be called here. Whether that happened in the port or
    /// earlier, the comment has been describing a fix that was not present.
    ///                                       (31.07.2026.) (SW port)
    auto const &engineSetup(editor.effect().uncheckedEngineSetup());
    auto const &parameters(editor.program().parameters());

    fftSize_->setValue(parameters.get<Engine::FFTSize>());
    overlapFactor_->setValue(parameters.get<Engine::OverlapFactor>());
    windowFunction_->setValue(parameters.get<Engine::WindowFunction>());
#if LE_SW_ENGINE_WINDOW_PRESUM
    windowSizeFactor_->setValue(parameters.get<Engine::WindowSizeFactor>());
#endif // LE_SW_ENGINE_WINDOW_PRESUM
#if LE_SW_ENGINE_INPUT_MODE >= 1
    unsigned int const customInputMode(SpectrumWorxCore::InputMode::maximum() + 1);
    unsigned int const inputModeValue(
        (engineSetup.numberOfChannels() > 2)
            ? customInputMode
            : parameters.get<SpectrumWorxCore::InputMode>().getValue());
    if ((inputModeValue == customInputMode) &&
        (inputMode_->numberOfItems() == SpectrumWorxCore::InputMode::numberOfDiscreteValues))
    {
        inputMode_->addItem(customInputMode, "<custom>", juce::Image(), false);
    }
    inputMode_->setValue(inputModeValue);
#endif // LE_SW_ENGINE_INPUT_MODE
    enginePage_.setNewQualityFactor(engineSetup.wolaRippleFactor());
}

SpectrumWorxEditor::Settings::EnginePage::EnginePage()
    : BackgroundImage(resourceBitmap<SettingsEngineBg>())
{
}

void SpectrumWorxEditor::Settings::EnginePage::setNewQualityFactor(float const &qualityFactorParam)
{
    float const qualityFactor(qualityFactorParam);
    // Implementation note:
    //   In this document http://eprints.kfupm.edu.sa/21525/1/21525.pdf (at the
    // end of page 32) it is argued that a variation of 0.03% or less is
    // negligible.
    //                                        (25.01.2010.) (Domagoj Saric)
    char const *description;
    if (qualityFactor < 0.0003f)
        description = "% (excellent)";
    else if (qualityFactor < 0.01f)
        description = "% (average)";
    else
        description = "% (poor)";
    char buffer[32];
    LE_VERIFY(Utility::lexical_cast(qualityFactor * 100.0f, 2, buffer) < _countof(buffer));
    *engineQuality_.getCharPointer().getAddress() = 0;
    engineQuality_ += "Ripple amount: ";
    engineQuality_ += buffer;
    engineQuality_ += description;
}

namespace
{
void printEngineDiagnostics(juce::String &buffer, char const *const title, float const value,
                            char const *const suffix, unsigned int const verticalOffset,
                            juce::Graphics const &graphics)
{
    char valueStr[32];
    LE_VERIFY(Utility::lexical_cast(value, 1, valueStr) < _countof(valueStr));
    buffer = title;
    buffer += ": ";
    buffer += valueStr;
    buffer += ' ';
    buffer += suffix;
    graphics.drawFittedText(buffer, SpectrumWorxEditor::Settings::xMargin + 4, verticalOffset, 142,
                            12, juce::Justification::centred, 1);
}
} // anonymous namespace

void SpectrumWorxEditor::Settings::EnginePage::paint(juce::Graphics &g)
{
    BackgroundImage::paint(g);
    g.setColour(juce::Colours::white);
    g.setFont(DrawableText::defaultFont());
    g.drawFittedText(engineQuality_, xMargin + 4, yMargin + yStep * 5, 142, 12,
                     juce::Justification::centred, 1);

    Settings &settings(
        Utility::ParentFromMember<Settings, EnginePage, &Settings::enginePage_>()(*this));
    Engine::Setup const &engineSetup(settings.editor().engineSetup());
    juce::String tmp;
    tmp.preallocateBytes(sizeof(juce::String::CharPointerType::CharType) * 64);
    printEngineDiagnostics(tmp, "Frequency resolution", engineSetup.frequencyRangePerBin<float>(),
                           "Hz", yMargin + yStep * 5 + 20, g);
    printEngineDiagnostics(tmp, "Time resolution", engineSetup.stepTime() * 1000, "ms",
                           yMargin + yStep * 5 + 40, g);
    printEngineDiagnostics(tmp, "Latency", engineSetup.latencyInMilliseconds(), "ms",
                           yMargin + yStep * 5 + 60, g);

    //...mrmlj...for testing...
    //g.drawSingleLineText( engineQuality_, xMargin - 5, yMargin + yStep * 6 + 12 );
}

SpectrumWorxEditor::Settings::InterfacePage::InterfacePage()
    : BackgroundImage(resourceBitmap<SettingsIntrfcBg>()),
      /// \note "Side window & menu opacity" until 6.4, when the side windows
      /// stopped being windows. It still drives exactly the same thing --
      /// BackgroundImage::paint, whose only subclasses are this panel's three
      /// pages and the preset browser -- and over the editor rather than over
      /// the desktop it is more use than it was, not less.
      ///                                   (01.08.2026.) (SW port)
      opacityTitle_("Panel & menu opacity", xMargin + 7, yMargin + 3 * yStep + 15,
                    opacityWidth + 40, 16, juce::Justification::left),
      globalOpacity_(juce::String()),
      moduleUIMouseOverReaction_(*this, xMargin, yMargin + 0 * yStep, "Mouse over reaction"),
      lfoUpdateBehaviour_(*this, xMargin, yMargin + 1 * yStep, "LFO update behaviour"),
      loadLastSessionOnStartup_(*this, xMargin - 4, yMargin + 2 * yStep,
                                "Load last session on startup"),
      hideCursorOnKnobDrag_(*this, xMargin - 4, yMargin + 3 * yStep - 15,
                            "Hide cursor on knob drag")
{
    Settings &parent(
        Utility::ParentFromMember<Settings, InterfacePage, &Settings::interfacePage_>()(*this));

    globalOpacity_.setBounds(xMargin + 7, yMargin + 3 * yStep + 12 + 20, opacityWidth, 16);
    globalOpacity_.setSliderStyle(juce::Slider::LinearHorizontal);
    globalOpacity_.setTextBoxStyle(juce::Slider::NoTextBox, true, 10, 12);
    globalOpacity_.setRange(0.8, 1);
    globalOpacity_.setValue(Theme::singleton().settings().globalOpacity,
                            juce::dontSendNotification);
    globalOpacity_.setDoubleClickReturnValue(true, 0.9);
    //globalOpacity_.setTooltip               ( globalOpacity_.getName() );
    globalOpacity_.addListener(&parent);

    moduleUIMouseOverReaction_.addItem(Theme::Never, "Never");
    moduleUIMouseOverReaction_.addItem(Theme::WhenParentModuleSelected, "Module selected");
    moduleUIMouseOverReaction_.addItem(Theme::WhenParentOrNothingSelected,
                                       "Module/nothing selected");
    moduleUIMouseOverReaction_.setSelectedIndex(
        Theme::singleton().settings().moduleUIMouseOverReaction);

    lfoUpdateBehaviour_.addItem(Theme::NoUpdate, "Never");
    lfoUpdateBehaviour_.addItem(Theme::WhenControlSelected, "Control selected");
    lfoUpdateBehaviour_.addItem(Theme::WhenControlActive, "Control active");
    lfoUpdateBehaviour_.addItem(Theme::Always, "Always");
    lfoUpdateBehaviour_.setSelectedIndex(Theme::singleton().settings().lfoUpdateBehaviour);

    loadLastSessionOnStartup_.addListener(&parent);

    hideCursorOnKnobDrag_.setToggleState(Theme::singleton().settings().hideCursorOnKnobDrag,
                                         juce::dontSendNotification);
    hideCursorOnKnobDrag_.addListener(&parent);

    addToParentAndShow(*this, globalOpacity_);
}

void SpectrumWorxEditor::Settings::InterfacePage::paint(juce::Graphics &graphics)
{
    graphics.setColour(juce::Colours::white);
    BackgroundImage::paint(graphics);
    opacityTitle_.draw(graphics);
}

#pragma warning(pop)

#pragma warning(push)
#pragma warning(disable : 4355) // 'this' used in base member initializer list.
SpectrumWorxEditor::Settings::AboutPage::AboutPage()
    : BackgroundImage(resourceBitmap<SettingsAboutBg>()),
      versionText_(SW_VERSION_STRING SW_EDITION_STRING, 65, 43, 107, 16),
      showUsersGuide_(*this, resourceBitmap<UsersGuideDown>(), resourceBitmap<UsersGuideUp>())
{
    showUsersGuide_.setTopLeftPosition(101, 108);
    showUsersGuide_.setClickingTogglesState(false);
}
#pragma warning(pop)

void SpectrumWorxEditor::Settings::AboutPage::paint(juce::Graphics &graphics)
{
    BackgroundImage::paint(graphics);
    graphics.setColour(juce::Colours::white);
    versionText_.draw(graphics);
}

class SettingsTab : public juce::TabBarButton
{
  public:
    using Images = std::array<juce::Image const *, 2>; // [ inactive, active ]

  public:
    SettingsTab(juce::String const &tabName, juce::TabbedButtonBar &ownerBar, Images const &images)
        : TabBarButton(tabName, ownerBar), images_(images)
    {
    }

  private:
    int getBestTabLength(int /*depth*/) override { return images_[false]->getWidth(); }

    bool hitTest(int /*mx*/, int /*my*/) override { return true; }

    void paint(juce::Graphics &graphics) override
    {
        paintImage(graphics, *images_[getToggleState()]);
    }

  private:
    Images const images_;
};

juce::TabBarButton *SpectrumWorxEditor::Settings::createTabButton(juce::String const &tabName,
                                                                  int const tabIndex)
{
    SettingsTab::Images images;
    switch (tabIndex)
    {
    case 0:
        images[0] = &resourceBitmap<SettingsEngineOff>();
        images[1] = &resourceBitmap<SettingsEngineOn>();
        break;
    case 1:
        images[0] = &resourceBitmap<SettingsGUIOff>();
        images[1] = &resourceBitmap<SettingsGUIOn>();
        break;
    case 2:
        images[0] = &resourceBitmap<SettingsAboutOff>();
        images[1] = &resourceBitmap<SettingsAboutOn>();
        break;
        LE_DEFAULT_CASE_UNREACHABLE();
    }
    return new SettingsTab(tabName, getTabbedButtonBar(), images);
}

SpectrumWorxEditor &SpectrumWorxEditor::Settings::editor()
{
    return Utility::ParentFromOptionalMember<SpectrumWorxEditor, Settings,
                                             &SpectrumWorxEditor::settings_, false>()(*this);
}

void SpectrumWorxEditor::Settings::buttonClicked(juce::Button *const pButton)
{
    if (pButton == &interfacePage_.loadLastSessionOnStartup_)
    {
        editor().editorHost().shouldLoadLastSessionOnStartup(
            interfacePage_.loadLastSessionOnStartup_.getToggleState());
    }
    else if (pButton == &interfacePage_.hideCursorOnKnobDrag_)
    {
        Theme::settings().hideCursorOnKnobDrag =
            interfacePage_.hideCursorOnKnobDrag_.getToggleState();
    }
    else if (pButton == &aboutPage_.showUsersGuide_)
    {
        LE_VERIFY(juce::Process::openDocument(
            rootPath().getChildFile("Documents/User's Guide.PDF").getFullPathName(),
            juce::String()));
    }
}

void SpectrumWorxEditor::Settings::updateLoadLastSessionOnStartup()
{
    interfacePage_.loadLastSessionOnStartup_.setToggleState(
        editor().editorHost().shouldLoadLastSessionOnStartup(), juce::dontSendNotification);
}

//------------------------------------------------------------------------------
} // namespace GUI
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------

/*
    Alex's scrap:

	AudioEffectX *effect = (AudioEffectX*)getEffect ();
	VstFileType aiffType ("AIFF File", "AIFF", "aif", "aiff", "audio/aiff", "audio/x-aiff");
	VstFileType waveType ("Wave File", ".WAV", "wav", "wav",  "audio/wav", "audio/x-wav");
	VstFileType aifcType ("AIFC File", "AIFC", "aif", "aifc", "audio/x-aifc");
	VstFileType sdIIType ("SoundDesigner II File", "Sd2f", "sd2", "sd2");

	VstFileSelect vstFileSelect;
	memset (&vstFileSelect, 0, sizeof (VstFileType));

	vstFileSelect.command     = kVstFileLoad;
	vstFileSelect.type        = kVstFileType;
	strcpy (vstFileSelect.title, "Load sample..");
	vstFileSelect.nbFileTypes = 2;
	vstFileSelect.fileTypes   = &aiffType;
	vstFileSelect.returnPath  = new char[1024];
	sprintf(vstFileSelect.returnPath, "");
	//vstFileSelect.initialPath  = new char[1024];
	vstFileSelect.initialPath = 0;

	CFileSelector* cFile = new CFileSelector(effect);

	if (cFile->run (&vstFileSelect))
	{
		StandardAlert(0, "File", vstFileSelect.returnPath, 0,0);
		UpdateDisplay(vstFileSelect.returnPath);
	}

	delete cFile;

	delete [] vstFileSelect.returnPath;
	if (vstFileSelect.initialPath)	delete []vstFileSelect.initialPath;
*/
