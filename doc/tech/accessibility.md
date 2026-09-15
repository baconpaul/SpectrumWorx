# Accessibility

What a screen reader is told, what the keyboard does, and the one rule the
modulation display must not break. The code is `src/gui/accessibility/`; the
widgets opt in where they are declared.

## Where the answers come from

The skin paints its own controls, so JUCE's handlers know neither a control's
name nor its reading. Every widget that stands for a parameter is already a
`ParameterMenu`, and that is where both come from:

| | Title | Value |
|---|---|---|
| module knob, shared gain and wet | `Module 2 - Gain` | the parameter's own text |
| editor knob | the host's name for it | as printed on the knob |
| LED, bypass, LFO switch | `ParameterMenu::accessibleName()` | checked / unchecked |
| trigger | the same | none: a press |
| combo box, waveform, sidechain source | the same, or the box's caption | the selected row |
| LFO sliders, frequency range | the same, following the thumb | the parameter's own text |
| module strip | `Module 2: Freeze`, a group | the effect's description |
| header lines, engine information | the painted text, as labels | |

A slider's value interface is the **proportion of its travel**, not the value,
so a screen reader's increment is an even step on a skewed range. The text it
reads is still the parameter's.

## Nothing an LFO does is announced

A screen reader re-reads the focused control on every `valueChanged` event, so
a modulated control that announced its sweep would talk over everything.

- An LFO, a host and a preset write widgets through `setValue()` with
  `dontSendNotification`, which posts nothing.
- `juce::Button::setToggleState()` posts whatever it is told, so the module LED
  keeps its own state rather than juce::Button's. Tested by
  `An LED's value moves without juce::Button announcing it`.
- The header lines are labels with no events at all: a screen reader reads them
  when it arrives on them.
- Only an edit made from the keyboard or a screen reader calls
  `Accessibility::announceValueChange()`.

## Keys

The mapping is sst-jucegui's `AccessibilityKeyboardEditSupport`
(`keyboardEdits.hpp`), so they match Six Sines.

| Key | On a slider or knob | On a toggle | On a combo box |
|---|---|---|---|
| Up / Down | a fortieth of the travel | on / off | next / previous row |
| Shift + Up / Down | a four-hundredth | | |
| Cmd + Up / Down | one step of the parameter | | |
| Left / Right | the same as Down / Up; **choose the thumb** on a two-thumbed slider | | previous / next row |
| Home / End | maximum / minimum | on / off | last / first row |
| Delete | default | | |
| Return | | toggle | open the menu |
| Shift + F10, the menu key | the parameter menu | the parameter menu | the parameter menu |
| Shift + Return | the parameter menu, for its type-in field | | |

A trigger fires on Return. A module strip raises the effect menu on the menu
keys. **Cmd + N** anywhere in the editor opens the navigation menu.

In the preset list, Return and Right open a folder and Backspace, Delete and
Left go up one (issue #150). Neither deletes a preset; the Delete button does.

## Tab order

`Traverser` is sst-jucegui's `KeyboardTraverser`: a component carries an order,
and its place is the sum of its own and its ancestors'. So a strip that moves
slot carries its controls with it, and anything with no order of its own sorts
with its nearest ordered ancestor. `SpectrumWorxEditor::TabOrder` lays out the
editor:

1. the panel column: the preset list and its buttons, or the settings tabs and
   the page showing
2. input, output and mix
3. the rack, a strip at a time: its controls top to bottom, bypass, remove, and
   the add button in the slot it would fill
4. the header lines (screen reader only)
5. the selected module's gain, wet and frequency range
6. the selected control's LFO
7. the sidechain source and its lock, undo, redo and the panel button

A click still never takes the keyboard where it did not before: newly focusable
widgets set `setMouseClickGrabsKeyboardFocus( false )`. The keyboard selecting a
module control is the same selection a click makes.

The same order serves a screen reader at the editor level. A strip, the shared
controls, the LFO strip and the preset browser are focus containers, so VoiceOver
reads each as a group; the tab key walks straight through them.

## The navigation menu and the focus debugger

Cmd + N lists every place the keyboard can go: the panels, the three knobs,
each strip's controls, the selected module's controls, the LFO and the sidechain
source. Its last entry toggles the focus debugger, a red box over whatever has
the keyboard and a line on stdout with its title and traversal order.

## Not done

- The About page's body text has no label; its links are reachable.
- Moving a module from the keyboard.
- A screen reader's increment on a combo box: it opens the menu instead.
- The preset list's key listener is not reachable from a headless test; the
  model's half is.
