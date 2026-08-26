# SpectrumWorx — Undo and redo

What can be taken back, how a step is recorded and applied, and which of the
several ways of doing that each kind of action gets.

Companion to [`threading_model.md`](threading_model.md), which is who owns what,
and [`parameter_system.md`](parameter_system.md), which is how a parameter is
addressed. Issue #101 is the ask.

---

## 1. What undo covers

One history per plugin instance, of things the **user** did in **our** editor.

| Action | Record | Step name |
|---|---|---|
| A knob drag, wheel notch, typed value or menu row | value delta | the parameter's name |
| Add, remove, insert or replace a module | chain edit | `"Add module"`, `"Remove module"`, … |
| Drag a module to reorder or swap | chain edit | `"Drag module"`, `"Swap modules"` |
| Load a preset | state snapshot | `"Load preset"` |
| Change FFT size, overlap factor or window | value delta | the parameter's name |

Deliberately **not** covered:

- **Anything the host wrote.** §3 is why that costs nothing to enforce.
- **Interface preferences.** Zoom, colour scheme, animation style, the LED
  switches. They are not part of the sound and they outlive the instance.
- **Where the user was.** Which panel is up, which settings tab, which browser
  folder — `SW::DawExtraState`'s first half. §6 keeps it out by construction.
- **The host's undo history.** `clap/ext/draft/undo.h` is vendored and
  unreferenced; see §9.

Not covered yet: a sample or side-chain change, which is not recorded at all.

---

## 2. The mutation surface is three functions

`spectrumWorxCLAP.cpp` says it where they are defined:

```cpp
// the three editor entry points each do the same two things: apply the change to
// the Program this thread owns, and queue it for the engine
```

`editParameter`, `editSlot`, `editModuleMove` — plus `setNewSample`,
`setSideChainSource` and `GUI::loadPreset`. Every undoable action ends in one of
those.

## 3. Host writes are excluded structurally

A host-written parameter never reaches those three. It arrives as a CLAP event
inside `process()` on the audio thread, is applied to the engine's `Program`,
and comes back to the main thread as a `ToUI::BaseParameterChanged` echo, which
lands in a **different** `setParameterIn` call site inside `drainEngineEvents()`.
`paramsFlush`'s main-thread arm routes host events through `handleEvent()`, which
only ever *reads* `programMain_`.

So "automation playback does not fill the undo stack" needs no origin flag and no
suppression window. It is a property of where the hook goes.

---

## 4. Where a step comes from

Two notions of gesture were already threaded through the editor, and they decide
which kind of record an action leaves.

**The per-parameter gesture** brackets a knob drag through
`ModuleControlBase::beginGesture()`/`endGesture()`, and everything else through
`publishValue()` — a wheel notch, a menu row and a typed value are each one whole
edit and bracket themselves, `needsOwnGesture()` deciding which. There is no
gestureless editor path.

`editParameter()` sets the outgoing value aside before it writes, because that is
the only moment it still exists; `automatedParameterEndEdit()` turns what was set
aside into a step. The order is *write, then report the gesture around it*, which
is what a discrete edit does and is why the value cannot be captured at the
gesture's beginning instead.

**The named block gesture** — `gestureBegin("Add module")` / `gestureEnd()`, six
call sites, every one a chain edit. It is called **after** the edit it names at
all six, three of them with an early return in between, which is why a chain step
is built as the edits happen rather than captured when the name arrives (§5).

> **The trap.** Four values could be set aside at once, and a write that no
> gesture ever claims holds its place. A preset writes every global parameter
> through `editParameter()` and brackets none of them, so one preset load filled
> all four and nothing released them: from then on a knob had nowhere to record
> where it started from and no drag was ever a step. A load now drops what it set
> aside at both ends of itself, and a full set evicts its **oldest** rather than
> refusing the newest — which turns "wedged for the life of the instance" into
> "self-clearing".

---

## 5. The three kinds of record

`src/undoHistory.hpp`. The history is bookkeeping only: a step says *what to put
back*, and the plugin knows how.

**`ValueDelta`** — one parameter and the value it held. Undone by one
`editParameter()`, no chain rebuild.

**`ChainEdit`** — a list of slot commands, in the order that puts things back:
*this effect into this slot*, *this module back there*. A slot command carries
the parameter values too, because refilling a slot builds a **new** module with
nothing in it; the values are read off `parameterIDs_` so what belongs to a slot
cannot drift from what a parameter is, and are applied behind the slot change in
the ring so the module they name exists when they land.

**`StateSnapshot`** — the whole program serialised, for a preset load.

The snapshot is the general one and could express all three. It is not used for
the other two because restoring one goes through the preset loader, and a preset
load builds an entirely new chain and swaps it in (`Loader::publishChain`). Every
module is then a new object, `resyncModuleRack()` matches strips by module
pointer and finds none, and the whole rack is destroyed and rebuilt — so undoing
a single added module played the add-and-remove animation for every strip. A
chain command touches the slot it is about, and the strips that stay keep their
pointers and merely slide.

### Coalescing

A second `ValueDelta` for the same parameter within **500 ms** is dropped: the
step already there holds the value to go back to. A comparison against the top of
the stack rather than a timer, so a step that was never pushed cannot be left
dangling by a window closing mid-gesture. The joined step's timestamp is not
refreshed, so a wheel held down becomes one step per window rather than one for
the whole spin.

Depth is 64, oldest dropped. Redo is a second stack, cleared by any new step.

---

## 6. Applying a step

A **delta** is `editParameter()` plus a `CLAP_PARAM_RESCAN_VALUES` — announced as
a rescan rather than as an automation gesture, the way
`resyncSpectralParametersToEngine()` announces a value nobody dragged to, because
a gesture would have to carry the value in the units the host is shown and only
the editor knows that conversion per family.

**And the widget, which nothing else would move.** A parameter reaches the
interface by one of two roads and an undo is neither: the host's write comes back
as an echo and takes `parameterChangedElsewhere()`, and a user's edit moved the
widget on the way past. So an undo makes that same call itself.

A **chain edit** replays its commands, taking each one's inverse *before* running
it — once a slot is emptied there is nothing left to ask what it held.

A **snapshot** goes through `GUI::loadPreset()`. It carries
`loadedPresetState()`: the name, bank, file and comment of the preset that was
playing, because the browser draws its selection from those and a sound that came
back under the wrong name is worse than either. It does **not** carry the panel,
the settings tab or the browser folder — the other half of the same block —
because those are places the user was and taking back an edit should not move
them. The browser is told afterwards, and navigates, an undo being able to land
in another bank.

> **Where a preset snapshot is taken.** Before *both* of the loader's passes.
> `GUI::loadPreset()` fills the main thread's copy in a pass of its own first, so
> a snapshot taken between the passes held the new `Program`, the old sample and
> the old preset name — three moments in one picture, and three loads in a row all
> recorded the state before the first of them.

### Suspension

Recording is off while a step is applied — or an undo would land on the stack and
put redo out of reach — and while a host restores a session, that not being
something the user did. Both use a scope guard. They used to use a
save-and-restore pair, and `stateLoad` has an early return between the halves, so
a session this build could not read switched undo off for the life of the
instance.

---

## 7. Where it lives

`SpectrumWorxCLAP`, main thread, beside `programMain_`.

- **Not the editor.** Parameters change with no window open, and a history that
  died when the user closed the editor would be a surprise.
- **Not in the session.** Reopening a session does not restore its history, and
  restoring one clears it.
- `undoHistory.{hpp,cpp}` sit beside the plugin rather than under `core/`, which
  is the engine and links no JUCE — `checkNoJuceInDSP.cmake` reads `src/core`
  that way.

### Thread safety

None needed. Every entry point in §2 is `[main-thread]`, the history is only
touched from there, and the audio thread never learns it exists.

---

## 8. Reaching it

A split pill under the rack: undo on the left, redo on the right, one arrow each,
and a half whose stack is empty goes dim. Two widgets rather than one with two
hit zones, so hovering, pressing and being unavailable are each JUCE's own answer
per half. `EditorHost` carries the six calls, defaulted to "no history" so that
the show-ui harness host keeps telling the truth about itself.

Whether either stack has anything on it is polled from `timerCallback()`, and has
to be: the stacks move for reasons the editor is never told about.

**There is no keyboard shortcut.** The buttons are the only way in.

---

## 9. Deliberately not done

**`clap.undo/4`.** The draft's thesis is that a plugin should delegate undo to
the host so there is one shared history. It is the better end state and is not
pursued: the extension is a draft with almost no host support, and three of the
four shipping formats go through clap-wrapper, which has nothing like it. The
transaction boundary in §4 is where it would attach — `begin_change()` /
`change_made(name)` is very nearly `gestureBegin(name)` / `gestureEnd()`.

**Sample and side-chain changes.** Not recorded. Recording them as snapshots
would work but would redraw the whole rack for a change that does not touch the
chain; they want a command of their own, as the slots got.

---

## 10. What the tests reach, and what they do not

`tests/clap/undoTests.cpp` drives a real plugin: what a step is, what one takes
back, and what a *sequence* of them does. `tests/core/undoHistoryTests.cpp` is the
two stacks on their own, with time passed in so the coalescing window can be
crossed without waiting for it.

Sequences are the point. Every fault this feature has had survived cases that did
one thing and checked it — the values set aside that filled up and stayed full,
the snapshot assembled from three moments at once, the flag left off by an early
return. They only showed when one action followed another. The cases read the
*name* of what undo would take back and the values it restores: no pixels, no
timing, no message loop, and a name is what the user reads off the control.

**What no test reaches is the editor.** No case in the tree drives a real
`SpectrumWorxCLAP` with an editor open — `SWTest::Instance` is a host of its own
with no CLAP layer under it, so it has no history to undo. What an undo does to a
*widget* and to the browser's selected row is checked by hand. Two of the faults
above lived exactly there.
