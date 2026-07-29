////////////////////////////////////////////////////////////////////////////////
//
// ---
// GUI
// ---
//
//   SpectrumWorx GUI implementation (based on our patched JUCE GIT tip).
//
// Copyright (c) 2009 - 2016. Little Endian Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
//
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef gui_hpp__3B545D5D_569F_4056_BEA8_159015782EC9
#define gui_hpp__3B545D5D_569F_4056_BEA8_159015782EC9
//------------------------------------------------------------------------------
#include "le/parameters/enumerated/tag.hpp"
#include "le/parameters/powerOfTwo/tag.hpp"
#include "le/utility/cstdint.hpp"
#include "le/utility/platformSpecifics.hpp"
#include "le/utility/rvalueReferences.hpp"
#include "le/utility/tchar.hpp"

#include <optional>
#include <string_view>
#include "le/utility/span.hpp"

#include "le/utility/intrusivePtr.hpp"

#include "resources.hpp"
#include "theme.hpp"

/// \note Individual JUCE 8 headers have no include guards and open
/// `namespace juce {` mid-file; they may only be reached through the module
/// umbrella header. The old "juce/..." prefix was the deleted fork's layout.
///                                       (28.07.2026.) (SW port)
#include <juce_gui_basics/juce_gui_basics.h>

#if defined(_WIN32)
#include "windows.h"
#elif defined(__APPLE__)
#ifdef __LP64__
typedef unsigned int ATSFontContainerRef;
#else
typedef unsigned long ATSFontContainerRef;
#endif // __LP64__
#endif // _WIN32
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------

namespace Parameters
{
template <class Parameter> struct DiscreteValues;
template <class Parameter> struct Name;
} // namespace Parameters

//------------------------------------------------------------------------------
namespace SW
{
//------------------------------------------------------------------------------

namespace Engine
{
class Setup;
}
class SpectrumWorx;

//------------------------------------------------------------------------------
namespace GUI
{
//------------------------------------------------------------------------------

/// \note `enum ResourceBitmaps` and `resourceBitmap()` used to be declared here
/// too, numbered with multi-character literals ('01') and read off disk. Both
/// now come from resources.hpp, which numbers them as plain integers, reads them
/// out of the binary and can release the cache. The two spellings coexisted only
/// while this header did not compile; the name sets were identical, so nothing
/// at a call site changes.
///                                       (28.07.2026.) (SW port)

////////////////////////////////////////////////////////////////////////////////
///
/// \class ReferenceCountedGUIInitializationGuard
///
/// \brief Responsible for GUI library initialization and cleanup handling.
///
////////////////////////////////////////////////////////////////////////////////

class ReferenceCountedGUIInitializationGuard
{
  public:
    static bool isGUIInitialised();

  protected:
    ReferenceCountedGUIInitializationGuard();
    ~ReferenceCountedGUIInitializationGuard();

  private:
    static std::uint8_t guiInitializationReferenceCount;
}; // class ReferenceCountedGUIInitializationGuard

void paintImage(juce::Graphics &, juce::Image const &);
void paintImage(juce::Graphics &, juce::Image const &, int x, int y);

void setSizeFromImage(juce::Component &, juce::Image const &);

class SpectrumWorxEditor;

bool isThisTheGUIThread();
bool isGUIInitialised();

float displayScale();

////////////////////////////////////////////////////////////////////////////////
// Global paths.
////////////////////////////////////////////////////////////////////////////////

/// \note The two mapPathsFile() overloads went with the boost::mmap dependency
/// stage 2 removed. They mapped the `SpectrumWorx.paths` file the 2016 installer
/// wrote, to find the skin and the most-recently-used presets folder; the skin
/// is compiled in now (resources.hpp), so the only thing left worth keeping is
/// the MRU folder, and that wants to be an ordinary user-data path rather than a
/// writable mapping. The rest of this block goes with gui.cpp's implementation
/// of it.
///                                       (28.07.2026.) (SW port)
bool initializePaths();
bool havePathsBeenInitialised();

juce::File const &rootPath();
juce::File &presetsFolder();
juce::File resourcesPath();

/// \note makeFSRefFromPath() went with it: FSRef is Carbon, which was never
/// ported to arm64. Its one caller was external_audio/sampleMac.cpp, which
/// stage 5.0 replaces with juce::AudioFormatManager.

////////////////////////////////////////////////////////////////////////////////
///
/// Theme
///
////////////////////////////////////////////////////////////////////////////////

/// \note Theme moved to theme.hpp, where it is a LookAndFeel_V2 (not a
/// LookAndFeel, which is abstract in JUCE 8, and not a LookAndFeel_V4, which
/// would paint linear sliders whole and silently drop the skinned thumb).
///
///   These two were static members of Theme purely because they read
/// Theme::settings(). They ask about a ModuleControlBase and a ModuleUI, which
/// a LookAndFeel has no business knowing about, and being members is what kept
/// Theme from being separable at all. aModuleControlNeedsLFOUpdate went with
/// them: it had no callers.
///                                       (28.07.2026.) (SW port)

class ModuleControlBase;
class ModuleUI;

bool shouldUpdateLFOControl(ModuleControlBase const &);

////////////////////////////////////////////////////////////////////////////////
/// \internal
/// \class WidgetBase
////////////////////////////////////////////////////////////////////////////////

namespace Detail
{
void setName(juce::Component &widget, juce::String const &newName);
void setName(juce::Component &widget, char const *const newName);

bool hasDirectFocus(juce::Component const &);
bool hasFocus(juce::Component const &);

bool isParentOf(juce::Component const &parent, juce::Component const &possibleChild);
bool isParentOf(juce::Component const &parent, juce::Component const *pPossibleChild);
} // namespace Detail

template <class BaseComponent = juce::Component> class LE_NOVTABLE WidgetBase : public BaseComponent
{
  protected:
    WidgetBase() : BaseComponent(juce::String()) {}
    LE_NOINLINE WidgetBase(char const *const componentName) : BaseComponent(juce::String())
    {
        setName(componentName);
    }
    WidgetBase(juce::String const &componentName) : BaseComponent(componentName) {}
    LE_FORCEINLINE ~WidgetBase() {}

  public:
    void setName(char const *const newName) { Detail::setName(*this, newName); }
    using BaseComponent::setName;

    void setVisible() { BaseComponent::setVisible(true); }
    void setInvisible() { BaseComponent::setVisible(false); }
    void setIsVisible(bool const isVisible) { BaseComponent::setVisible(isVisible); }

    void setEnabled(bool const isEnabled) { BaseComponent::setEnabled(isEnabled); }

    bool hasDirectFocus() const { return Detail::hasDirectFocus(*this); }
    bool hasFocus() const { return Detail::hasFocus(*this); }

    bool isParentOf(juce::Component const &control) const
    {
        return Detail::isParentOf(*this, control);
    }

    static void *operator new(std::size_t const count, void *LE_RESTRICT const pStorage)
    {
        (void)count;
        LE_ASSUME(pStorage);
        return pStorage;
    }
    static void operator delete(void *LE_RESTRICT const /*pObject*/,
                                void *LE_RESTRICT const /*pStorage*/)
    {
    }

#ifdef __clang__
    void *operator new(std::size_t const count) { return ::operator new(count); }
    void operator delete(void *const pObject) { return ::operator delete(pObject); }
#endif // __clang__

  private:
    using BaseComponent::isParentOf;
    using BaseComponent::setVisible;
}; // class WidgetBase

////////////////////////////////////////////////////////////////////////////////
///
/// \class OwnedWindowBase
/// \internal
/// \brief OwnedWindow core/non-templated implementation.
///
////////////////////////////////////////////////////////////////////////////////

class PresetBrowser;
class SpectrumWorxEditor;

class OwnedWindowBase
{
  protected:
    static void attach(SpectrumWorxEditor &, juce::Component &);
    static void detach(SpectrumWorxEditor &, juce::Component &);

    static void adjustPositions(juce::Component *pFirstWindow, juce::Component *pSecondWindow,
                                unsigned int editorX, unsigned int editorY, unsigned int flags);
    static void adjustPositions(SpectrumWorxEditor &parent, juce::Component *pFirstWindow,
                                juce::Component *pSecondWindow);

    static void adjustPositionsForPresetBrowser(SpectrumWorxEditor &parent,
                                                juce::Component *pCurrentWindowState);
    static void adjustPositionsForSettings(SpectrumWorxEditor &parent,
                                           juce::Component *pCurrentWindowState);

  private:
#ifdef _WIN32
    static LRESULT __stdcall callWndHookProc(int, WPARAM, LPARAM);

    static HHOOK wndProcHook;
#endif // _WIN32
}; // class OwnedWindowBase

////////////////////////////////////////////////////////////////////////////////
///
/// \class OwnedWindow
///
/// \brief A base class for windows/widgets/components that need "owned window"
/// behaviour with automatic main (editor) window position tracking/following.
///
////////////////////////////////////////////////////////////////////////////////

#pragma warning(push)
#pragma warning(disable : 4127) // Conditional expression is constant.

template <class Window> class OwnedWindow : private OwnedWindowBase
{
  private:
    static bool const isPresetBrowser = std::is_same<Window, PresetBrowser>::value;
    static bool const isSettingsWindow = !isPresetBrowser;

  public:
    // Implementation note:
    //   Unfortunately we cannot make the attachment automatic in the
    // constructor because, under OSX, attachment causes the 'paint' method to
    // be immediately invoked which can cause undefined behaviour/crashes
    // because the wrapping object's constructor still has not finished (and
    // thus the 'paint' method gets called on an incomplete object). Because of
    // this, the only reliable usage (for now) is for the wrapping class
    // (Window) to call the attach() function manually at the end of its
    // constructor.
    //                                        (07.10.2010.) (Domagoj Saric)
    /// \todo Think of a smarter solution or assert that attach() gets called
    /// exactly once.
    ///                                       (07.10.2010.) (Domagoj Saric)
    void attach()
    {
#ifdef _MSC_VER
        static_assert(std::is_same<Window, SpectrumWorxEditor::Settings>::value == isSettingsWindow,
                      "");
#endif // _MSC_VER

        SpectrumWorxEditor &parent(static_cast<Window *>(this)->editor());
        juce::Component &window(static_cast<Window *>(this)->window());

        OwnedWindowBase::attach(parent, window);

        if (isPresetBrowser)
            adjustPositionsForPresetBrowser(parent, &window);
        else
            adjustPositionsForSettings(parent, &window);

        LE_ASSERT(window.isOnDesktop() && window.getPeer());
    }

    ~OwnedWindow()
    {
#ifdef _MSC_VER
#endif // _MSC_VER
        SpectrumWorxEditor &parent(static_cast<Window *>(this)->editor());
        juce::Component &window(static_cast<Window *>(this)->window());
        OwnedWindowBase::detach(parent, window);
        if (isPresetBrowser)
            adjustPositionsForPresetBrowser(parent, 0);
        else
            adjustPositionsForSettings(parent, 0);
    }
}; // class OwnedWindow
#pragma warning(pop)

void warningMessageBox(std::string_view title, std::string_view message, bool canBlock);
bool warningOkCancelBox(TCHAR const *title, TCHAR const *question);

void addToParentAndShow(juce::Component &parent, juce::Component &childToBe);

void fadeOutComponent(juce::Component &, float finalAlpha, unsigned int duration,
                      bool useProxyComponent);

class Lock : private juce::MessageManagerLock
{
  public:
    /// \note The cast picks the Thread* overload. JUCE 8 also has one taking a
    /// ThreadPoolJob*, so a bare nullptr is ambiguous.
    LE_NOINLINE Lock() : juce::MessageManagerLock(static_cast<juce::Thread *>(nullptr))
    {
        LE_ASSERT(lockWasGained() || !ReferenceCountedGUIInitializationGuard::isGUIInitialised());
    }
}; // class Lock

namespace Detail
{
template <class GUIHolder, class Functor>
class Message final : public juce::MessageManager::MessageBase
{
  public:
    Message(GUIHolder &guiHolder, Functor &&functor)
        : pGUIHolder_(&guiHolder), functor_(std::move(functor))
    {
    }

  private:
    LE_COLD void messageCallback() final
    {
        if (pGUIHolder_->gui())
            if (!functor_(*pGUIHolder_->gui()))
                this->post();
    }

  private:
    LE::Utility::IntrusivePtr<GUIHolder> const pGUIHolder_;
    Functor functor_;
}; // class Message

template <class Functor> class MessageDirect final : public juce::MessageManager::MessageBase
{
  public:
    MessageDirect(Functor &&functor) : functor_(std::move(functor)) {}

  private:
    LE_COLD void messageCallback() final { functor_(); }

  private:
    Functor const functor_;
}; // class MessageDirect

inline void postMessage(juce::MessageManager::MessageBase *LE_RESTRICT const pMessage)
{
    if (pMessage)
        pMessage->post();
}
} // namespace Detail

template <class GUIHolder, class Functor> void postMessage(GUIHolder &guiHolder, Functor &&functor)
{
    Detail::postMessage(new (std::nothrow)
                            Detail::Message<GUIHolder, Functor>(guiHolder, std::move(functor)));
}

template <class Functor> void postMessage(Functor &&functor)
{
    Detail::postMessage(new (std::nothrow) Detail::MessageDirect<Functor>(std::move(functor)));
}

template <class GUIHolder, class Functor>
void postOrExecuteMessage(GUIHolder &guiHolder, Functor &&functor)
{
    if (isThisTheGUIThread() && functor(*guiHolder.gui()))
        return;
#if LE_SW_SEPARATED_DSP_GUI
    LE_UNREACHABLE_CODE();
#else
    postMessage(/*std::forward<GUIHolder>*/ (guiHolder), std::move(functor));
#endif
}

#if 0 //...mrmlj...does not work with the latest juce...cleanup...
////////////////////////////////////////////////////////////////////////////////
///
/// \class AsyncRepainter
/// \internal
/// \brief Helper for creating asynchronous/'on-GUI-thread' repainting widgets.
///...mrmlj...thoroughly document...
////////////////////////////////////////////////////////////////////////////////

// http://lachand.free.fr/cocoa/Threads.html
// https://developer.apple.com/library/mac/documentation/Cocoa/Conceptual/Multithreading/Introduction/Introduction.html
// http://www.cocoawithlove.com/2009/08/safe-threaded-design-and-inter-thread.html

class LE_NOVTABLE AsyncRepainter : public juce::Component
{
public:
    static void repaint( juce::Component &, int x, int y, int w, int h );

#define LE_IMPLEMENT_ASYNC_REPAINT                                                                 \
  private:                                                                                         \
    void internalRepaint(int const x, int const y, int const w, int const h) noexcept override     \
    {                                                                                              \
        AsyncRepainter::repaint(*this, x, y, w, h);                                                \
    }

private:
    AsyncRepainter();
};
#else
#define LE_IMPLEMENT_ASYNC_REPAINT
#endif

////////////////////////////////////////////////////////////////////////////////
///
/// \class DrawableText
///
////////////////////////////////////////////////////////////////////////////////

/// \note Holds a GlyphArrangement rather than deriving from one: JUCE 8 marks
/// GlyphArrangement final. Only the constructor and draw() were ever used, so
/// forwarding the one is the whole of the change.
class DrawableText
{
  public:
    DrawableText(char const *text, unsigned int x, unsigned int y, unsigned int width,
                 unsigned int height, juce::Justification = juce::Justification::centredLeft,
                 juce::Font const &font = defaultFont());

    void draw(juce::Graphics &graphics) const { glyphs_.draw(graphics); }

    static juce::Font defaultFont();

  private:
    juce::GlyphArrangement glyphs_;
}; // class DrawableText

////////////////////////////////////////////////////////////////////////////////
///
/// \class BitmapButton
///
////////////////////////////////////////////////////////////////////////////////

class BitmapButton : public WidgetBase<juce::ImageButton>
{
  public: // ModuleUI control traits
    typedef bool value_type;
    typedef value_type param_type;

    static value_type valueRangeMinimum() { return false; }
    static value_type valueRangeMaximum() { return true; }
    static value_type valueRangeQuantum() { return true - false; }

    value_type getValue() const { return getToggleState(); }
    void setValue(param_type const newValue)
    {
        setToggleState(newValue, juce::dontSendNotification);
    }

  public:
    BitmapButton(juce::Component &parent, juce::Image const &on, juce::Image const &off,
                 juce::Colour const &overlayColourWhenOver = defaultOverOverlay(),
                 bool toggled = true);

  public
      : //...mrmlj...as a workaround for the juce::ImageButton class that hides all this information...
    juce::Image getCurrentImage() const;

    static juce::Colour const &normalOverlay();
    static juce::Colour const &downOverlay();
    static juce::Colour const &defaultOverOverlay();

    static float normalOpacity() { return 1.00f; }
    static float downOpacity() { return 1.00f; }
    static float overOpacity() { return 0.88f; }

  protected:
    static char const *getTextFromValue(unsigned int const booleanValue)
    {
        static char const *const strings[] = {"off", "on"};
        return strings[booleanValue];
    }

  private:
    LE_IMPLEMENT_ASYNC_REPAINT

    using juce::Button::getToggleStateValue;
}; // class BitmapButton

////////////////////////////////////////////////////////////////////////////////
///
/// \class PopupMenu
///
////////////////////////////////////////////////////////////////////////////////

class PopupMenu
{
  public:
    PopupMenu(PopupMenu const &) = delete; // makes non-copyable
    PopupMenu &operator=(PopupMenu const &) = delete;

    using ItemID = std::uint32_t;
    using OptionalID = std::optional<ItemID>;

  public:
    PopupMenu();
    ~PopupMenu() {}

    void addItem(ItemID, char const *newItemText, juce::Image const &icon = juce::Image(),
                 bool enabled = true);
    void addSubMenu(PopupMenu &, char const *name);
    void addSectionHeader(char const *title);

    void clear();

    unsigned int numberOfItems() const;

    OptionalID showCenteredAtRight(juce::Component const &) const;
    OptionalID showCenteredBelow(juce::Component const &) const;

    static bool menuActive() { return menuActive_; };

  protected:
    typedef unsigned int MangledID;
    static MangledID mangleID(ItemID);
    static ItemID unmangleID(MangledID);

  private:
    void updateDimensionsForNewItem(juce::String const &itemText);

    OptionalID showAt(unsigned int x, unsigned int y, unsigned int width,
                      unsigned int height) const;

  protected: //...mrmlj...
    juce::PopupMenu menu_;

  private:
    static unsigned int const zeroIDMaskWorkaround = 0xFF000000;

    unsigned short menuHeight_;
    unsigned short menuWidth_;

    static bool menuActive_;
}; // class PopupMenu

////////////////////////////////////////////////////////////////////////////////
///
/// \class PopupMenuWithSelection
///
////////////////////////////////////////////////////////////////////////////////

class PopupMenuWithSelection : public PopupMenu
{
  public:
    PopupMenuWithSelection();

    unsigned int getSelectedID() const;
    void setSelectedID(unsigned int newSelectionID);

    unsigned int getSelectedIndex() const;
    void setSelectedIndex(unsigned int newSelectionIndex);

    juce::String const &getSelectedItemText() const;
    juce::Image const &getSelectedItemIcon() const;

    bool showCenteredAtRight(juce::Component const &);
    bool showCenteredBelow(juce::Component const &);

    void clear();

    bool hasValidSelection() const;

    juce::String const &getItemText(unsigned int itemIndex) const;

  private:
    bool handleNewSelection(OptionalID const &chosenMenuEntryID);
    void updateSelection(unsigned int newSelectionIndex);

  private:
    int currentSelection_;
    int currentSelectionID_;
}; // class PopupMenuWithSelection

////////////////////////////////////////////////////////////////////////////////
///
/// \class ComboBox
///
////////////////////////////////////////////////////////////////////////////////

class LE_NOVTABLE ComboBox : public WidgetBase<>, public PopupMenuWithSelection
{
  public: // ModuleUI control traits
    typedef unsigned int value_type;
    typedef value_type param_type;

    static value_type valueRangeMinimum() { return 0; }
    value_type valueRangeMaximum() const { return numberOfItems(); }
    static value_type valueRangeQuantum() { return 1; }

    value_type getValue() const { return getSelectedID(); }
    void setValue(param_type const newValue) { setSelectedID(newValue); }

  public:
    void setSelectedID(unsigned int newSelectionID);
    void setSelectedIndex(unsigned int newSelectionIndex);

  protected:
    ComboBox(juce::Component &parent, juce::Image const &normalBackground,
             juce::Image const &selectedBackground);

    bool showMenu();

  protected: // juce::Component overrides
    void paint(juce::Graphics &) override;

    LE_IMPLEMENT_ASYNC_REPAINT

  private:
    juce::Image const &normalBackground_;
    juce::Image const &selectedBackground_;
}; // class ComboBox

////////////////////////////////////////////////////////////////////////////////
///
/// \class BackgroundImage
///
////////////////////////////////////////////////////////////////////////////////

class LE_NOVTABLE BackgroundImage : public WidgetBase<>
{
  protected:
    BackgroundImage(juce::Image const &image) : pImage_(&image) {};

    juce::Image const &image() const;
    void setImage(juce::Image const &);

  protected: // juce::Component overrides
    void paint(juce::Graphics &) override;

  private:
    juce::Image const *pImage_;
}; // class BackgroundImage

////////////////////////////////////////////////////////////////////////////////
///
/// \class LEDTextButton
///
////////////////////////////////////////////////////////////////////////////////

namespace Detail
{
void paintTextButton(BitmapButton const &, juce::Graphics &, unsigned int textX, unsigned int textY,
                     unsigned int imageX, unsigned int imageY, bool isMouseOverButton,
                     bool isButtonDown);
} // namespace Detail

class LEDTextButton : public BitmapButton
{
  public:
    LEDTextButton(juce::Component &parent, unsigned int x, unsigned int y, char const *text);

  protected: // juce::Component overrides
    void paintButton(juce::Graphics &, bool isMouseOverButton, bool isButtonDown) override;
}; // class LEDTextButton

////////////////////////////////////////////////////////////////////////////////
///
/// \class TextButton
///
////////////////////////////////////////////////////////////////////////////////

class TextButton : public WidgetBase<juce::Button>
{
  public:
    TextButton(juce::Component &parent, unsigned int x, unsigned int y, char const *text);

  private:
    void paintButton(juce::Graphics &, bool isMouseOverButton, bool isButtonDown) override;

    static unsigned int const height = 11;
}; // class TextButton

////////////////////////////////////////////////////////////////////////////////
///
/// \class Knob
///
////////////////////////////////////////////////////////////////////////////////

class LE_NOVTABLE Knob : public WidgetBase<juce::Slider>
{
  public:
    typedef double value_type;
    typedef float param_type;

    LE_COLD value_type getValue() const
    {
        return static_cast<value_type>(juce::Slider::getValue());
    }
    LE_COLD void setValue(param_type);

    param_type getNormalisedValue() const;

    void setupForParameter(char const *title, juce::Image const &filmStripToSizeFor,
                           param_type defaultValue);

  protected:
    Knob(juce::Component &parent, unsigned int x, unsigned int y, unsigned int xMargin,
         unsigned int yMargin);

  public:
    static void removeValueListeners(juce::Slider &, juce::Value::Listener &);

  protected:
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Woverloaded-virtual"
#endif // __clang__
    void paint(juce::Image const &filmStrip, unsigned int xMargin, unsigned int yMargin,
               juce::Graphics &);
#ifdef __clang__
#pragma clang diagnostic pop
#endif // __clang__

  protected: // juce::Component overrides
    void startedDragging() noexcept override;
    /// \note Was declared and defined only under !NDEBUG, but
    /// EditorKnob::stoppedDragging calls it unconditionally -- an undefined
    /// symbol in every release build. The body is nothing but an assertion, so
    /// it costs nothing to exist: LE_ASSERT compiles away on its own.
    ///                                       (28.07.2026.) (SW port)
    void stoppedDragging() noexcept override;

    LE_IMPLEMENT_ASYNC_REPAINT

  private:
    using juce::Slider::getMaxValueObject;
    using juce::Slider::getMinValueObject;
    using juce::Slider::getValueObject;

  protected:
    static unsigned int const numberOfKnobSubbitmaps = 127;
}; // class Knob

////////////////////////////////////////////////////////////////////////////////
///
/// \class EditorKnob
///
////////////////////////////////////////////////////////////////////////////////

class EditorKnob final : public Knob
{
  public:
    EditorKnob(SpectrumWorxEditor &parent, unsigned int x, unsigned int y);

    void setupForParameter(std::uint8_t parameterIndex, param_type minimumValue,
                           param_type maximumValue, param_type defaultValue);

  private: // juce::Component overrides
    void paint(juce::Graphics &) override;
    void valueChanged() noexcept override;

    void startedDragging() noexcept override;
    void stoppedDragging() noexcept override;

  private:
    SpectrumWorxEditor &editor();

  private:
    std::uint8_t parameterIndex_;
}; // class EditorKnob

////////////////////////////////////////////////////////////////////////////////
///
/// \class TitledComboBox
///
////////////////////////////////////////////////////////////////////////////////

class TitledComboBox : public ComboBox
{
  public:
    TitledComboBox(juce::Component &parent, unsigned int x, unsigned int y, char const *title);

  private:
    void mouseDown(juce::MouseEvent const &) override;
    void paint(juce::Graphics &) override;

  private:
    DrawableText const title_;
}; // class TitledComboBox

// Implementation note:
//   The following fillComboBoxForParameter<>() implementation supports only
// enumerated and power-of-two parameters and uses their internal knowledge
// (that they do not use a DisplayValueTransformer and how they are printed,
// a simple 'lexical_cast' for power-of-two parameters or a direct fetch of
// a string from the DiscreteValues<>::strings[] array) in order to slightly
// reduce compile time and runtime overhead. In case it becomes needed again, a
// generic solution was used up to revision 4636.
//                                            (15.07.2011.) (Domagoj Saric)

namespace Detail
{
void addPowerOfTwoValueStringsToComboBox(unsigned int firstValue, unsigned int lastValue,
                                         ComboBox &comboBox);

void addEnumeratedParameterValueStringsToComboBox(
    LE::Utility::Span<char const *LE_RESTRICT const> strings, ComboBox &comboBox);

template <class Parameter>
void fillComboBoxForParameter(ComboBox &comboBox, LE::Parameters::PowerOfTwoParameterTag)
{
    addPowerOfTwoValueStringsToComboBox(Parameter::minimum(), Parameter::maximum(), comboBox);
}

template <class Parameter>
void fillComboBoxForParameter(ComboBox &comboBox, LE::Parameters::EnumeratedParameterTag)
{
    addEnumeratedParameterValueStringsToComboBox(
        LE::Utility::makeSpan(LE::Parameters::DiscreteValues<Parameter>::strings), comboBox);
}
} // namespace Detail

template <class Parameter> void fillComboBoxForParameter(ComboBox &comboBox)
{
    Detail::fillComboBoxForParameter<Parameter>(comboBox, typename Parameter::Tag());
}

////////////////////////////////////////////////////////////////////////////////
///
/// \class DiscreteParameterComboBox
///
///   A wrapper for the TitledComboBox widget class that helps reduce verbosity/
/// boiler plate code by automatically calling name<Parameter>() and
/// fillComboBoxForParameter<Parameter>.
///
////////////////////////////////////////////////////////////////////////////////
// Implementation note:
//  It contains a TitledComboBox instance rather than inheriting from
// TitledComboBox in order to 'emulate' __declspec( novtable ) for non-MSVC
// compilers. This requires the various helper operators and the -> syntax.
//  Rather than making the whole class a template only the constructor is
// templatized so that headers that use the class do not have to also know about
// the actual parameters for which a DiscreteParameterComboBox will be created.
//                                            (18.07.2011.) (Domagoj Saric)
////////////////////////////////////////////////////////////////////////////////

struct DiscreteParameterComboBox
{
    template <class Parameter>
    DiscreteParameterComboBox(juce::Component &parent, unsigned int const x, unsigned int const y,
                              Parameter const * = 0)
        : comboBox_(parent, x, y, LE::Parameters::Name<Parameter>::string_)
    {
        fillComboBoxForParameter<Parameter>(comboBox_);
    }

    TitledComboBox *operator->() { return &comboBox_; }
    TitledComboBox const *operator&() const { return &comboBox_; }
    operator TitledComboBox &() { return comboBox_; }
    operator TitledComboBox const &() const { return comboBox_; }

    TitledComboBox comboBox_;
}; // struct DiscreteParameterComboBox

//------------------------------------------------------------------------------
} // namespace GUI
//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // gui_hpp
