////////////////////////////////////////////////////////////////////////////////
///
/// moduleControlFocusTests.cpp
/// ---------------------------
///
///   What happens to a module control's keyboard focus when the control is
/// disabled under it -- which is what clicking a knob whose LFO is on does.
///
///   `ModuleKnob::mouseDown` calls `setEnabled( !isLFOEnabled() )` so that
/// juce::Slider's own drag handling cannot move a value the LFO owns. JUCE's
/// `Component::setEnabled( false )` sets the disabled flag *and then*, if the
/// component has keyboard focus, hands that focus to the parent -- and
/// `getWantsKeyboardFocus()` is `wantsKeyboardFocusFlag && !isDisabledFlag`
/// (juce_Component.cpp), so by the time our `focusLost()` runs the answer is
/// already false. There is no `--render` page for that: it needs a click, and a
/// click needs focus, which needs a peer.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "gui/editorHarness.hpp"

/// \note Before anything that names SW::Module: the module chain downcasts a
/// node to it, and this is the header with the complete type.
#include "core/modules/moduleDSPAndGUI.hpp"

#include "gui/modules/moduleControl.hpp"
#include "gui/modules/moduleUI.hpp"

#include "le/parameters/lfoImpl.hpp"

#include <juce_gui_basics/juce_gui_basics.h>

#include <catch2/catch_test_macros.hpp>
//------------------------------------------------------------------------------
#if JUCE_LINUX || JUCE_BSD
////////////////////////////////////////////////////////////////////////////////
///
/// \note Declared rather than included. <X11/Xlib.h> reaches a consumer of
/// juce_gui_basics only behind JUCE_GUI_BASICS_INCLUDE_XHEADERS, and turning
/// that on here would put X11's macros -- None, Status, Bool, Success, Complex
/// -- into a translation unit that also compiles Catch2 and our own headers.
/// Three functions and one opaque type is the whole of what aWindowCanBeMade()
/// below needs, and libX11 is already on this target's link line through JUCE.
///                                           (05.08.2026.) (SW port)
///
////////////////////////////////////////////////////////////////////////////////
extern "C"
{
    struct _XDisplay;
    _XDisplay *XOpenDisplay(char const *displayName);
    unsigned long XInternAtom(_XDisplay *display, char const *name, int onlyIfExists);
    int XCloseDisplay(_XDisplay *display);
} // extern "C"
#endif // JUCE_LINUX || JUCE_BSD
//------------------------------------------------------------------------------
namespace
{
using namespace LE;
using namespace LE::SW;

////////////////////////////////////////////////////////////////////////////////
///
/// \brief Whether a desktop window can be made here at all.
///
/// \note X11 only, and it is not the same question as "is there a display".
/// `xvfb-run` gives a window server and no *window manager*, and JUCE cannot
/// make a window on one: `XWindowSystem::createWindow` writes the WM_PROTOCOLS
/// property unguarded --
///
///     xchangeProperty( windowH, atoms.protocols, XA_ATOM, 32, atoms.protocolList, 2 );
///
/// -- and `atoms.protocols` is `getIfExists( display, "WM_PROTOCOLS" )`, an atom
/// that only a window manager ever interns. With none running it is None, the
/// property written is 0, and the server answers BadAtom -- on which Xlib's
/// default handler calls `exit( 1 )`, killing the case where it stands. The
/// leak-detector output that follows in a CI log is that exit, not a second bug.
///
///   So the atom is asked for the way JUCE asks for it, and its absence is the
/// signal. macOS and Windows have no such hole and run these cases normally.
///                                           (05.08.2026.) (SW port)
///
////////////////////////////////////////////////////////////////////////////////
bool aWindowCanBeMade()
{
#if JUCE_LINUX || JUCE_BSD
    /// \note Our own connection rather than JUCE's, which is not open yet the
    /// first time this is asked. Atoms belong to the server rather than to a
    /// connection, so the answer is the one JUCE will get.
    auto *const pDisplay(XOpenDisplay(nullptr));
    if (!pDisplay)
        return false; // No window server either.
    auto const protocols(XInternAtom(pDisplay, "WM_PROTOCOLS", 1 /* only if it exists */));
    XCloseDisplay(pDisplay);
    return protocols != 0; // ...which is None.
#else
    return true;
#endif // JUCE_LINUX || JUCE_BSD
}

/// \brief The editor, on the desktop, so that JUCE will hand out keyboard focus.
///
/// \note `Component::grabKeyboardFocusInternal` returns early on `!isShowing()`,
/// and `isShowing()` walks up to a peer -- so an editor that is merely
/// constructed can never be focused, which is why `sw-show-ui --render` trips
/// JUCE's own `jassert( isShowing() || isOnDesktop() )` and renders anyway. A
/// window is the only way to drive focus, and if this machine has no window
/// server the cases below skip rather than fail.
class DesktopEditor
{
  public:
    explicit DesktopEditor(SWTest::Instance &instance) : instance_(instance)
    {
        instance_.openEditor();
        editor().setVisible();
        editor().addToDesktop(0);
        editor().toFront(true);
    }

    /// \note The editor is destroyed while it is still on the desktop, which is
    /// the order a host closes a window in: the view goes away with the editor
    /// rather than before it. juce::Component's destructor calls
    /// removeFromDesktop() itself.
    ~DesktopEditor() { instance_.closeEditor(); }

    GUI::SpectrumWorxEditor &editor() const { return instance_.editor(); }

    bool usable() const { return editor().isShowing(); }

  private:
    SWTest::Instance &instance_;
}; // class DesktopEditor

/// \brief A left-button event over \p component, \p offset from its centre. The
/// press is at the centre either way, which is what a slider measures a drag
/// against.
///
/// \note Hand-built and handed straight to `Component::mouseDown()` and friends,
/// which is *half* a mouse: JUCE tracks a gesture in the MouseInputSource, and
/// nothing here touches that, so `isMouseOverOrDragging()` stays false
/// throughout. That is enough for everything below, and the whole mouse is not
/// available: driving `ComponentPeer::handleMouseEvent` -- the entry point the
/// window server itself uses -- was tried and never reaches the component,
/// because `Desktop::findComponentAt` ends at `NSViewComponentPeer::contains`,
/// which is an NSView hit test, and a plain test binary is not an app whose
/// views the window server will hit-test. See the second case for what that
/// costs.
///                                           (03.08.2026.) (SW port)
juce::MouseEvent eventOver(juce::Component &component, juce::Point<float> const offset,
                           bool const dragged)
{
    auto const centre(component.getLocalBounds().getCentre().toFloat());
    return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), centre + offset,
                            juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1.0f, 0.0f,
                            0.0f, 0.0f, 0.0f, &component, &component, juce::Time(), centre,
                            juce::Time(), 1, dragged);
}

/// Press, drag, release. The drag is upwards, because the knobs are
/// RotaryVerticalDrag, and far enough to clear setMouseDragSensitivity( 800 ).
void dragKnob(juce::Component &knob)
{
    auto const up(juce::Point<float>(0.0f, -200.0f));
    knob.mouseDown(eventOver(knob, {}, false));
    knob.mouseDrag(eventOver(knob, up, true));
    knob.mouseUp(eventOver(knob, up, true));
}

/// \brief The first effect-specific control of \p moduleUI that is a knob.
///
/// \note By parameter index rather than by walking the children: the widget
/// storage is a compile-time chain of one base class per parameter, so there is
/// no runtime list of controls to iterate.
GUI::ModuleControlBase *firstKnob(GUI::ModuleUI &moduleUI)
{
    auto const parameters(moduleUI.module().numberOfEffectSpecificParameters());
    for (std::uint8_t index(0); index < parameters; ++index)
    {
        auto &control(moduleUI.effectSpecificParameterControl(index));
        if (dynamic_cast<GUI::ModuleKnob *>(&control.widget()) != nullptr)
            return &control;
    }
    return nullptr;
}
} // anonymous namespace

TEST_CASE("Dragging a knob the LFO owns moves nothing and deselects nothing", "[gui][modules][lfo]")
{
    SWTest::HostSideJuce const juceIsUp;

    if (!aWindowCanBeMade())
        SKIP("No window manager: JUCE cannot put a component on the desktop here, and these "
             "cases need a real window to drive the keyboard with.");

    SWTest::Instance instance;
    DesktopEditor const window(instance);
    if (!window.usable())
    {
        WARN("No window server: keyboard focus cannot be driven here.");
        return;
    }

    auto &editor(window.editor());
    editor.addUserAddedModule(0);
    editor.resyncModuleRack();
    auto *const pModuleUI(editor.regionInSlot(0));
    REQUIRE(pModuleUI != nullptr);

    auto *const pControl(firstKnob(*pModuleUI));
    REQUIRE(pControl != nullptr);
    auto &control(*pControl);
    auto &knob(control.widget());

    // The LFO, on, the way the LFO display's switch sets it.
    control.lfo().parameters().set<Parameters::LFOImpl::Enabled>(true);
    REQUIRE(control.isLFOEnabled());

    auto const valueBefore(control.getValue());

    // The click's own half: ModuleKnob::mouseClickCanGrabFocus is true, so a
    // press on a knob takes the keyboard focus, and taking it is what makes the
    // control the selected one -- which is what puts its LFO on screen.
    knob.grabKeyboardFocus();
    REQUIRE(knob.hasKeyboardFocus(false));
    REQUIRE(editor.activeControl() == &control);

    // The reported case. Against the previous implementation mouseDown disabled
    // the knob, so JUCE handed that focus straight back to the parent and
    // ModuleControlImpl::focusLost's `LE_ASSERT( getWantsKeyboardFocus() )`
    // fired -- SIGINT in a debug build.
    dragKnob(knob);

    // The requirement that disabling was there to meet: the LFO owns this value
    // and the mouse may not move it.
    CHECK(control.getValue() == valueBefore);

    // ...and the three things disabling cost. The control is still live, still
    // focused, and still the selected one -- so its LFO display is still on
    // screen, which is the whole reason for pressing a modulated knob.
    CHECK(knob.isEnabled());
    CHECK(knob.hasKeyboardFocus(false));
    CHECK(editor.activeControl() == &control);
}

TEST_CASE("An LFO switches every gesture that would move the knob under it", "[gui][modules][lfo]")
{
    // \note The other side of the case above, and it is the switches rather than
    // a second drag. Moving an unmodulated knob is what proves the first case
    // blocks something -- but a value only moves through
    // ModuleKnob::valueChanged(), which asserts isMouseOverOrDragging(), and no
    // synthesised event sets that (see the note on eventOver). What can be said
    // without a real mouse is that the three gestures agree with each other: the
    // drag joined the wheel and the double click, which is the whole change.
    SWTest::HostSideJuce const juceIsUp;

    if (!aWindowCanBeMade())
        SKIP("No window manager: JUCE cannot put a component on the desktop here, and these "
             "cases need a real window to drive the keyboard with.");

    SWTest::Instance instance;
    DesktopEditor const window(instance);
    REQUIRE(window.usable());

    auto &editor(window.editor());
    editor.addUserAddedModule(0);
    editor.resyncModuleRack();
    auto *const pModuleUI(editor.regionInSlot(0));
    REQUIRE(pModuleUI != nullptr);

    auto *const pControl(firstKnob(*pModuleUI));
    REQUIRE(pControl != nullptr);
    auto &control(*pControl);
    auto &knob(dynamic_cast<juce::Slider &>(control.widget()));

    REQUIRE(!control.isLFOEnabled());

    // \note Selecting it first: the wheel is off on an unselected knob whatever
    // the LFO says -- moduleControlDeactivated() switches it off so that
    // scrolling the rack does not move whatever it passes over -- and
    // moduleControlActivated() is what asks syncMouseWheelAndLFOState().
    knob.grabKeyboardFocus();
    REQUIRE(editor.activeControl() == &control);

    CHECK(knob.isScrollWheelEnabled());
    CHECK(knob.isDoubleClickReturnEnabled());

    auto const valueBefore(control.getValue());

    control.lfo().parameters().set<Parameters::LFOImpl::Enabled>(true);
    control.lfoStateChanged();

    CHECK(!knob.isScrollWheelEnabled());
    CHECK(!knob.isDoubleClickReturnEnabled());
    // ...and the third, which used to be spelt setEnabled( false ) inside
    // mouseDown and is now a return at the top of mouseDrag.
    dragKnob(knob);
    CHECK(control.getValue() == valueBefore);
    CHECK(knob.isEnabled());
}
