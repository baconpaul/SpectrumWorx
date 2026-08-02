# SpectrumWorx — Correcting the threading model

This is `week_two.md` §1 item 3, taken as its own project. That item said the 2016
threading model needs replacing rather than patching, listed six violations as *the
inventory the redesign has to satisfy, not the plan*, and deferred the design. This is the
design.

It supersedes §1 item 3 and §2.2 of [`week_two.md`](week_two.md) and the paragraph on 5.8
in [`implementation_sequence.md`](implementation_sequence.md). Those keep their value as
the evidence; what follows is what to do about it.

---

## 1. Four symptoms, three causes

The reported symptoms are not independent bugs.

| Symptom | What it is a view of |
|---|---|
| The engine makes slider values and asserts that the message manager is not locked | **A.** The UI is a member of the engine |
| The UI objects are attached to the engine objects in a way which is obviously wrong | **A**, structurally |
| The UI deadlocks with two instances | **B.** Two owners of JUCE's lifetime, and process-wide UI state |
| The preset load test leaks a JUCE object | **C.** The engine reports problems by raising a dialog |

Under all of them: there is no ownership boundary, only a lock — and the lock is not taken
on every path that mutates the thing it protects.

### A. The UI is a member of the engine

`SW::Module` holds its own editor region:

```cpp
class LE_NOVTABLE Module : public Engine::ModuleDSP, …      // moduleDSPAndGUI.hpp:41
{
    …
    OptionalUI ui_;                                          // :99  std::optional<GUI::ModuleUI>
};
```

and four virtuals exist for no other purpose than to push a value into it
(`moduleDSPAndGUI.hpp:87-93`, defined at `moduleDSPAndGUI.cpp:140-186`). The stack in the
report is the LFO arm of exactly that:

```
Engine::Processor::preProcess                       processor.cpp:43       [audio thread]
  ModuleParameters::updateEffectParametersFromLFOs  moduleParameters.cpp:247
    Module::setEffectParameterFromLFO               moduleDSPAndGUI.cpp:183
      ModuleUI::setEffectParameter                  moduleUI.cpp:562
        Knob::setValue → juce::Slider::setValue     gui.cpp:1031
          juce::Component::repaint                  juce_Component.cpp:1627   ← assertion
```

It is structural, not incidental. `Module::Impl<Effect>` *inherits* `ModuleWidgets<Effect>`
(`finalImplementations.hpp:87-96`), so every module the factory `malloc`s
(`factory.cpp:173`) carries the JUCE widget storage for its effect inline, sized at compile
time. That is why `sw-dsp` — the target whose own comment says "the engine, the effects and
everything they need. No host." — links `juce::juce_gui_basics` and `sw-gui-resources`
(`dsp.cmake:183`).

Three consequences worth naming separately, because each will need its own fix:

- **The audio thread can be the last owner of a module.** `ModuleChainBase::forEach` holds
  an `IntrusivePtr` per node deliberately (`moduleChainImpl.hpp:314-320`, and the note says
  so). So when the editor removes a module while `process()` is walking the chain, the
  reference that hits zero is the audio thread's. That lands in
  `intrusive_ptr_release_deleter` (`moduleDSPAndGUI.cpp:205`) → `destroyGUI()` → not the GUI
  thread → `GUI::postMessage`, which is `new (std::nothrow) Detail::Message<…>` followed by
  a JUCE message post. **An allocation and a JUCE queue lock, on the audio thread, in a
  destructor.**
- **`destroyGUI()` on the message thread takes the processing lock, blocking**
  (`moduleDSPAndGUI.cpp:123`). The message thread holds the message-manager lock by
  definition — and the shim's `guiDestroy()` takes it explicitly
  (`clap_juce_shim_impl.cpp:305`). Meanwhile the audio thread, holding the processing lock,
  calls `repaint()`, which on macOS reaches AppKit. That is a lock-order inversion with a
  plausible cycle. **Unconfirmed** — see §7, which asks for the backtrace pair.
- **`juce::String` is constructed on the audio thread** whenever an automated parameter's
  control happens to be the active one: `setParameterControl` → `updateActiveControlValue()`
  (`moduleUI.cpp:534-537`).

### B. Two owners of JUCE's lifetime

`SpectrumWorxEditor` privately inherits `ReferenceCountedGUIInitializationGuard`
(`spectrumWorxEditor.hpp:78`), which keeps a count of its own and calls JUCE's free
functions directly:

```cpp
if (guiInitializationReferenceCount++ == 0) {          // gui.cpp:80
    juce::initialiseJuce_GUI();                        // :90
    juce::MessageManager::getInstance()->setCurrentThreadAsMessageThread();   // :91
    …
}
…
if (--guiInitializationReferenceCount == 0) {          // :126
    juce::shutdownJuce_GUI();                          // :147
}
```

The shim owns the same lifetime through `juce::ScopedJuceInitialiser_GUI`
(`clap_juce_shim_impl.cpp:115,235`), and JUCE reference-counts *that* with a counter our
calls never touch:

```cpp
JUCE_API void JUCE_CALLTYPE shutdownJuce_GUI()          // juce_MessageManager.cpp:465
{
    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}
static int numScopedInitInstances = 0;                                      // :474
ScopedJuceInitialiser_GUI::~ScopedJuceInitialiser_GUI()
{ if (--numScopedInitInstances == 0) shutdownJuce_GUI(); }                   // :477
```

So **closing an editor deletes the MessageManager and runs `DeletedAtShutdown::deleteAll()`
while the shim still holds a live initialiser.** With one instance that is a close/reopen
hazard; with two it is one instance tearing down the message loop the other is running on,
because the count, the `DeletedAtShutdown` list and the `MessageManager` singleton are all
per-binary and every instance of SpectrumWorx in that host shares them.

Two more from the same file:

- `setCurrentThreadAsMessageThread()` — a plugin declaring which thread is the host's
  message thread.
- An explicit refusal:
  ```cpp
  if (guiInitializationReferenceCount && !isThisTheGUIThread())   // gui.cpp:71
      … "does not currently support multiple editor instances with this host." … throw
  ```

And the UI keeps process-wide mutable state that two instances silently share:

| | |
|---|---|
| `ModuleUI::pSelectedModule_` | `moduleUI.cpp:318` |
| `ModuleControlBase::pActiveControl` | `moduleControl.cpp:45` — with a 2011 note arguing it is safe because "no two windows can have focus at the same time", and a `\todo Verify this on the Mac` |
| `PopupMenu::menuActive_` | `gui.hpp:571` |
| `LFOImpl::Timer::{barDuration_, measureNumerator_, hasTempoInformation_}` | `lfoImpl.hpp:206-208` — every instance shares one tempo, and this is also the cause of the flaky `[preset-corpus]` case recorded in `tech_debt.md` |

### C. The engine reports problems by raising a dialog

The default `PresetProblemReporter` is a message box per problem
(`presets.cpp:444-478`), and a 2011 preset raises one `MissingParameter` per parameter its
effect grew afterwards — 806 across the 303 factory banks. `SWTest::ScopedProblemCounter`
exists precisely so the test suite does not leak 809 `AsyncUpdater`s
(`presetHarness.hpp:126-128`).

That is the reported leak. It is also a live shipping bug, already in `tech_debt.md`:
restoring a session puts up one modal dialog per missing parameter, without anyone asking,
possibly before there is a window to put one in front of.

---

## 2. The rules

1. **The audio thread owns the engine.** `Program`, the module chain, the modules, the LFOs
   and `Engine::Setup` are its property. Nothing else dereferences them.
2. **The main thread owns a full copy of that state**, and the widgets. It is authoritative
   for what the user and the host asked for; the engine is authoritative for what happened.
3. **They exchange formal messages, in both directions, over two SPSC rings.** Ordered, all
   delivered, heterogeneous payloads.
4. **Continuously varying values cross in a mailbox of atomics, not the ring.**
5. **The audio thread takes no lock.** `processCriticalSection_` is deleted, not narrowed.
6. **A parameter has a base value and a modulated value, and they are different things.**

---

## 3. The three channels

```
                 main thread                              audio thread
                 ───────────                              ────────────
  editor ──┬──▶  MainThreadModel  ──▶ ToEngine ring ──▶   drained at the top of
           │     (full copy)          (commands)          process(), then the engine runs
           │
           ├──◀  MainThreadModel  ◀── ToUI ring    ◀──    base changed, slot changed,
           │                          (events)            retire this pointer, setup
           │                                              changed, preset report
           │
           └──◀  ValueMailbox     ◀── atomics      ◀──    modulated values, per block,
                 (const & to editor)                      coalescing, painting only
```

**Why two transports and not one.** The ring is for things where order and delivery matter:
`SetSlot{2, Gain}` followed by `ClearSlot{2}` does not coalesce to the second one, and a
dropped `Retire{ptr}` is a leak. The mailbox is for a sampled signal, where every value but
the newest is dead on arrival. `Processor::preProcess()` runs once per host block
(`processor.cpp:143`) and writes every enabled LFO's target, so at 48 kHz with a 32-sample
buffer that is 1500 updates per second per enabled LFO — up to 120,000/s for a full rack —
against a UI that draws at 30 Hz. A FIFO would spend all its bandwidth on values nobody
sees; a mailbox cannot overflow and coalesces by construction. Same message list, two
transports.

**Ownership.** `SpectrumWorxCLAP` owns all three — **not the editor**. `paramsValue`,
`paramsValueToText` and `stateSave` are `[main-thread]` calls that happen with the window
shut, so the main-thread model has to outlive the editor and be there when there has never
been one. The editor is handed references at construction; the mailbox as `const &`, since
the UI only ever reads it.

A consequence worth having: the host's parameter reads stop touching engine memory.
`paramsValue` currently reaches `getAutomatedParameter` on a live module
(`spectrumWorxCLAP.cpp:504`, `plugin2HostImpl.inl:114`) from the main thread while
`process()` may be writing it.

**The queue itself.** `SpectrumWorxCLAP::UIEdits` (`spectrumWorxCLAP.hpp:124-154`,
`spectrumWorxCLAP.cpp:877-897`) is already a correct SPSC ring — inline storage, no
allocation, relaxed/acquire/release written correctly — and it becomes the template for
both directions. It *refuses* on full where `sst::cpputils::SimpleRingBuffer` clobbers the
unread tail, and refusing is the right policy for commands. It keeps its existing job as
the plugin→host leg unchanged.

---

## 4. Base versus modulated

There is no base value today. The LFO does not modulate a parameter, it **overwrites** it:

```cpp
parameter_value_t ModuleParameters::setEffectParameterFromLFOAux(   // moduleParameters.cpp:262
    std::uint8_t const parameterIndex, LFO::value_type const lfoValue)
{
    auto const &info(effectSpecificParameterInfo(parameterIndex));
    auto const parameterValue(normalisedToParameterValue(lfoValue, info));
    return setEffectParameter(parameterIndex, parameterValue);      // ← into the parameter
}
```

Three things follow, and all three are wrong:

- **The CLAP parameter value is base + internal LFO.** `paramsValue` reads that same
  storage, so a host's generic panel polls the sweep. It is never *sent* as an event — the
  LFO path never calls `automatedParameterChanged` — but polled amounts to the same thing.
- **Saving freezes the sweep into the file.** `savePresetParameters` writes
  `getEffectParameter(i)` (`moduleParameters.cpp:355`), so saving a preset or a session
  while an LFO is running stores that LFO's instantaneous output as the parameter's value.
- **The UI is built around the absence of a base.** The knob is disabled on `mouseDown`
  while its LFO runs (`moduleUI.cpp:169`), the scroll wheel is off (`:281`),
  double-click-to-default is off (`:144`), and both edit paths assert `!isLFOEnabled()`
  (`moduleDSPAndGUI.cpp:163`, `moduleControl.cpp:108`). There is nothing to drag *to*: the
  next block would overwrite it.

So `Engine::ModuleParameters` gains a base per LFO-able parameter, written by user edits,
host automation and preset load. The LFO writes only the live parameter storage, and
**emits no events at all**.

| | base | modulated |
|---|---|---|
| written by | user edit, host automation, preset load | the LFO, once per block |
| `paramsValue`, `stateSave`, `savePreset` | ✅ | ✗ |
| ToUI ring — latchable, ordered | ✅ | ✗ |
| ValueMailbox — coalescing, painting | ✗ | ✅ |

The main-thread model carries **both**, so dragging the base with the LFO active is a
future UI change rather than a future re-plumbing.

**Not proposed:** turning `lowerBound`/`upperBound` into a depth around the base. They are
absolute values in the parameter's own units and every preset since 2011 stores them that
way. Here "base" means *the value that applies when the LFO is off, remembered while it is
on*.

---

## 5. Lifetime: publish and retire

Structural change — a slot's effect, a preset, a session, a sample, the spectral setup —
does not mutate live engine state. The main thread builds the replacement, publishes one
pointer, and the audio thread hands the old one back to be destroyed:

```
main thread                                  audio thread
───────────                                  ────────────
Program *next = build(…);                    // ← allocation, formatting, file IO
push(SwapProgram{next})            ───────▶  live_ = next;              // one store
                                   ◀───────  push(Retire{old})
delete old;                                  // never destroyed under the callback
```

`SpectrumWorxCore` already holds `Program *pProgram_` rather than a `Program`
(`spectrumWorxCore.hpp:402`), so the indirection this needs is there.

**The spectral setup is the exception, and it uses CLAP.** Changing the FFT size or the
overlap factor reallocates the whole working set and resizes every module, and today it
takes the processing lock, blocking, from whichever thread wrote the parameter
(`spectrumWorxCore.cpp:650-663`). It becomes: record the pending setup, call
`clap_host::request_restart()`, reconfigure inside `activate()` — where the audio thread is
definitionally not running, so there is no lock, no flag and nothing to race.

That also fixes a contract this plugin is currently breaking. `clap_plugin_latency` says
latency may only change while the plugin is deactivated; FFT size *is* the latency
(`activate()` caches `engineSetup().latencyInSamples()`, `spectrumWorxCLAP.cpp:230`), and
`reportNewLatencyInSamples` calls `latencyChanged()` from wherever the parameter was
written (`:1026-1034`). A preset that changes the spectral setup therefore takes the
restart with it and installs its chain in `activate()`.

---

## 6. Stages

Each leaves a working plugin and a green `sw-tests` in both build trees. One commit per
stage, on a branch.

**0 — Instruments and evidence.** ✅ *done, 02.08.2026.* No behaviour change; see §6.0
below for what it found.

**1 — One owner for JUCE; per-instance statics; the reporter stops being a dialog.**
✅ *done, 02.08.2026;* see §6.1.

**2 — The protocol, empty.** ✅ *done, 02.08.2026;* see §6.2.

**3 — The base value in the engine.** ✅ *done, 02.08.2026;* see §6.3.

**4 — UI → engine: every edit is a command.** ✅ *done, 02.08.2026;* see §6.4.

**5 — Engine → UI: `Module` loses its `ModuleUI`.** ✅ *done, 02.08.2026, in two commits;*
see §6.5.

**6 — Publish and retire; the lock is deleted.** §5, applied to slot edits, preset load,
state load, sample loads and the spectral setup. Also **`LFOImpl::Timer`'s three tempo
statics**, deferred here from stage 1 (see §6.1): the transport becomes a per-instance value
the audio thread publishes, and `clampFreePeriod`, `snapSyncedPeriod`, `defaultSyncType` and
`adjustValue{For,From}Preset` take it rather than reading a global.
**`processCriticalSection_` is deleted**,
`SpectrumWorxCore::process` returns `void` again, and `runEngine`'s silence branch goes with
it — which is `week_two.md`'s own stated measure of whether this worked, and closes the
first threading entry in `tech_debt.md`.

**7 — `sw-dsp` links no JUCE; the test binary splits.** `presets.{hpp,cpp}` drop
`juce::String`/`juce::File` for `std::string` (8 sites); `presetFile.cpp` and
`external_audio/sample.{hpp,cpp}` move up, since only `spectrumWorxCLAP` and
`spectrumWorxEditor` use them; `assertionHandler.cpp`'s two `juce::String` uses go.
`sw-tests` splits into **`sw-dsp-tests`** — math, effects, goldens, parameters, presets,
utility, with no JUCE on the link line — and **`sw-plugin-tests`**, plus a CMake gate in the
shape of `checkODRHeaderScope.cmake` that fails if `sw-dsp`'s compile or link lines mention
JUCE. The gate is the point: without it the layering rots back in a fortnight.

**8 — Prove it, and delete the corpses.** Editor open, every knob on all five slots dragged,
**zero rtsan reports**. tsan over the two-instance test.
`SharedModuleControls::FrequencyRange::canUseWriteAccessIndex()`'s `!isThisTheGUIThread()`
hack (`auxiliaryComponents.cpp:451-456`) and `~SpectrumWorxEditor`'s two `volatile bool`
spin-waits (`spectrumWorxEditor.cpp:199-204`) go, because nothing but the GUI thread writes
a widget any more. Delete `src/spectrumWorx.{cpp,hpp}` — 1,899 lines, which `week_two.md`
§2.5 says this item owns — along with `GUI::Lock` and `BackgroundThread`. Drive Logic and
Bitwig through the situations that deadlocked, plus `clap-validator`.

### 6.0 — What stage 0 built, and what it found

**The lock knows its owner.** `Utility::CriticalSection` is a class rather than a
`using` for `std::recursive_mutex`, and it publishes a per-thread token under the mutex.
`currentThreadOwnsTheProcessLock()` is one line and true on every platform.

**Which found a live falsehood in the first run.** `setNumberOfChannelsImpl` asserts the
lock, and `clap_plugin::activate` reaches it *without* the lock — correctly, the audio
thread not having started. The same is true of `setBlockSize`. So the six sites did not
mean "hold the lock"; they meant **nothing else may be inside `process()` right now**,
which holding the lock is only one way to achieve. They assert
`currentThreadMayMutateEngineState()` now — the lock *or* `suspended_`, which the
constructor sets and `resume()` clears. Both terms go at stage 6.

That is the whole argument for doing this stage first: the assertion had been vacuous for
the life of the port, and the first thing it did on becoming real was contradict the
codebase's own account of its invariant.

**Thread identity.** `core/threading/threadCheck.hpp`. `markMainThread()` at
`clap_plugin::init`, which is `[main-thread]` by contract and the only answer available
when the host offers no `clap.thread-check`. `isAudioThread()` is deliberately *"is this
call underneath `process()`"* rather than *"is this thread the audio thread"* — a host
with a worker pool delivers successive blocks of one plugin on different threads, so
there is no such thread to name.

**Sanitizers.** `cmake/sw-sanitizers.cmake`, one `SW_SANITIZER` cache variable rather
than a pair of blessed build directories, applied before `add_subdirectory(libs)` so that
it reaches the dependencies too. It link-tests the flag and refuses with a diagnostic
naming the compiler, because `-fsanitize=realtime` *compiles* on Apple clang 21 and fails
to link. Measured: Homebrew clang 22.1.6 links it, so the acceptance test is

```
cmake -B build-rtsan -D SW_SANITIZER=realtime \
      -D CMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++
```

`Threading::ScopedAudioCallback` at the top of `process()` opens the realtime region.
It is the runtime entry point and not `[[clang::nonblocking]]`: the attribute would also
run a static analysis this tree cannot answer until stage 6.

**Tests.** `tests/core/threadCheckTests.cpp`, plus one case in `processLockTests.cpp`
whose last `CHECK` — the lock held on another thread, so this one does not own it — is
what fails against the hardcoded `true`. The wiring is checked through the host API
rather than by reading the source: a host offering `clap.state` and no
`clap.thread-check` makes `markCurrentProgramAsModified()` defer, so a parameter event
delivered to `process()` arrives at `request_callback` from inside the audio callback,
and `mark_dirty` arrives only later at `on_main_thread`. Take the guard out of
`process()` and the case goes red.

### 6.1 — What stage 1 did

**JUCE has one owner.** `ReferenceCountedGUIInitializationGuard` is `SkinLifetime`: it
builds the `Theme` and installs it as the default LookAndFeel for as long as at least one
editor exists, and it touches JUCE's lifetime not at all. `setCurrentThreadAsMessageThread()`
and the "does not currently support multiple editor instances" throw are gone.
`isThisTheGUIThread()` and `isGUIInitialised()` ask JUCE through
`getInstanceWithoutCreating()` rather than asking a count of *our editors* whether JUCE was
up — which answered "no" for the whole of a plugin's life with no window open.
`GUI::Lock` went too: its body was an assertion about that count, and §2.5 of `week_two.md`
had already found it has no callers anywhere.

**Selection is per editor.** `ModuleUI::pSelectedModule_` and
`ModuleControlBase::pActiveControl` are `SpectrumWorxEditor::pSelectedModule_` and
`::pActiveControl_`. The enabler was `ModuleUI` holding its editor instead of recovering it
from `getParentComponent()` — `Module::createGUI` writes every parameter into the widgets
*before* parenting the region, which worked only because everything it reached went through
those statics rather than through the editor.

**The preset layer counts.** `PresetLoadReport`, `takePresetLoadReport()`, and a default
reporter that raises nothing. `GUI::loadPreset` turns the count into **one** dialog, and
only when an editor is open; `stateLoad` says nothing at all.

**`PopupMenu::menuActive_` stays a static, deliberately.** A menu really is modal to the
process — JUCE's own `getNumCurrentlyModalComponents()`, which `tryActivateControl()` reads
two lines away, is process-wide for the same reason. It is a `bool` with no lifetime, so it
dangles nothing.

**`LFOImpl::Timer`'s three tempo statics did not move, and stage 6 gets them.** The plan put
them here and that was the wrong home. They are read by `clampFreePeriod`,
`snapSyncedPeriod`, `defaultSyncType` and `adjustValue{For,From}Preset` — LFO members that
hold no `Timer` and would all have to start taking a tempo — so making them per-instance is
a change to the shape of the parameter model, not a change to a variable. Stage 6 publishes
the transport as a value the UI reads anyway, which is where a per-instance tempo belongs.
Until then two instances share one tempo, which is *nearly* right — every instance in a host
sees the same transport — and it is what makes `[preset-corpus]` flaky when the suite runs
in one process, as `tech_debt.md` records.

**Tests.** `tests/gui/twoInstanceTests.cpp`. The decisive assertion is one line: with a
`ScopedJuceInitialiser_GUI` held across the case — standing in for the shim's — closing one
editor must leave `MessageManager::getInstanceWithoutCreating()` non-null. Against the old
implementation it is null. Plus: the survivor is still a working editor, reopening the
closed one works, selection is independent, and a run over a whole factory bank with **no
reporter installed** counts 806-style missing parameters and leaks nothing — which is
symptom 1, stated as a test.

### 6.2 — What stage 2 built

Three headers under `core/threading/`, owned by `SpectrumWorxCLAP`, reached by the editor
through two new `EditorHost` virtuals — which is where they belong, because the host reads
parameters through `clap_plugin_params` with the window shut, so the protocol has to outlive
any editor and exist when there has never been one.

- **`spscQueue.hpp`** — `SPSCQueue<Message, Capacity>`, generalised from `UIEdits`, which
  was already exactly this and is now one instantiation of it. Free-running counters masked
  only on the way into the array, and `push` **refuses** when full rather than overwriting
  the unread tail. That is the one place it differs from `sst::cpputils::SimpleRingBuffer`
  and it is the whole reason for having our own: a dropped `Retire` is a leak, and
  `SetSlot{2,Gain}` then `ClearSlot{2}` does not coalesce to the second.
- **`valueMailbox.hpp`** — a value per dense parameter index plus a bitset of what moved,
  swept with `exchange` so a write landing mid-sweep is carried into the next one rather
  than lost.
- **`messages.hpp`** — `ToEngine` and `ToUI`, tagged unions, trivially copyable, owning
  nothing. Each case says which side is responsible for a pointer after it lands, which is
  the entire memory-management story: no shared ownership anywhere in the protocol and
  nothing destroyed on the audio thread.

Both drains are wired and run from the start — `drainCommands()` at the top of `process()`,
before the host's own events so a block's automation wins over anything queued before it
began, and `drainEngineEvents()` in `onMainThread()`. Every unimplemented case asserts, so a
stage that sends a message before writing its handler says so rather than dropping it.

**Tests.** `tests/core/protocolTests.cpp`. The refusal-when-full property, survival past the
end of the storage, and two cases that actually run two threads: 100k messages through an
eight-slot ring arriving once and in order, and a mailbox swept while a writer runs flat out
ending on the last value written.

### 6.3 — What stage 3 did

`Engine::ModuleParameters` keeps one float per LFO-able parameter, sized and indexed exactly
as the LFO storage is and provisioned the same way — an array in `ModuleEffectImpl<Effect>`,
whose length the effect decides, handed down to the base.

It is called **unmodulated** in the code and *base* in this document, because "base
parameters" already means the five shared ones. `setBaseParameter`/`setEffectParameter` —
what a user, the host and a preset go through — write both it and the live value;
`set*ParameterFromLFOAux` calls a new `set*ParameterLive` and writes only the live one.

Three things move as a result:

| | before | after |
|---|---|---|
| `clap_plugin_params::get_value` | the sweep, polled | the value the user set |
| `savePresetParameters` | the LFO's instantaneous output | the value the user set |
| GUI pushes per LFO tick | **two**, one of them tagged `AutomationOrPreset` | one |

That last one was a real defect and fell out of the change: `setEffectParameterFromLFOAux`
called the *virtual* setter, so `Module::setEffectParameter` pushed the value into the widget
with the wrong source, and then `Module::setEffectParameterFromLFO` pushed it again with
`LFOValue`. Calling the non-virtual live setter leaves exactly one push.

**The Armonizer's quick-patch had to change with it.** `ModuleFactory` set that effect's Wet
to 50 % by writing the parameter directly, behind the setter — which would now leave the
module claiming 100 to a host and to a preset while sounding like 50. It goes through
`setBaseParameter` now.

**Tests.** A new `[preset-roundtrip]` case sets Gain, enables its LFO, runs 64 blocks,
checks the live value really has moved, and then requires the *saved* value to be the one
that was set. Verified by reversion: with the unmodulated write removed it fails, and it was
run and watched to fail rather than reasoned about.

Three existing `[clap][lfo]` cases had to change their observation point, and that is worth
being explicit about: they asserted "an enabled LFO modulates something" by watching
`get_value`, which is exactly what must now stay still. They read the live engine parameter
through `plugin_data` instead, and a new case pins both halves at once — the DSP's value
takes many values over a run, the host's takes one.

### 6.4 — What stage 4 did

**One command covers all four kinds of parameter**, because `SetBaseParameter` carries a
packed `ParameterID` and a value in the parameter's own units — which is exactly what
`Host2PluginInteropImpl::setParameter` already takes, `handleEvent` having converted off the
CLAP edge before calling it. So an edit made in the interface and one made in the host's
panel became the same operation arriving by two routes, applied on one thread in a defined
order. The three editor sites that wrote the engine directly now push:

| | wrote | now |
|---|---|---|
| a module knob, button or combo box | `Module::setParameterValueFromUI` | `SetBaseParameter` |
| an LFO control | `lfo().parameters().set<…>()` | `SetBaseParameter` |
| a global — including **FFT size and overlap factor** | `SpectrumWorxCore::setGlobalParameter` | `SetBaseParameter` |

That last row is the important one. `setGlobalParameter(FFTSize&, …)` took the processing
lock **blocking, on the message thread**, and reallocated the entire spectral working set
under it — the single worst path in the old model and the most likely half of the deadlock.

**Two things follow from the write no longer being synchronous.**

`drainCommands()` is called from `paramsFlush()` as well as `process()`. A host with the
transport parked may not be calling `process()` at all, and `requestParameterFlush()` — which
the editor already asks for after every edit — is what then gets the command applied. Two
callers is safe for the same reason `flushUIEdits` already had two: CLAP forbids a host from
running flush and process concurrently, so there is still one consumer.

`ModuleControlBase::getValueText()` formats the **widget's** value rather than the engine's.
It called `getValueString(nullptr)`, which reads the module — so with the write queued the
caption under a knob would lag the drag by a block, or for ever with a host that is not
processing. What is on screen is what the caption should say.

**Nothing is lost by not waiting for the snapped value.** Snapping is a pure function of the
parameter's static description, and the widget already carries it: a knob's range and
interval come from the same `ParameterInfo`, a combo box's value is an index, a button's is a
bool. The assertion in `Module::setEffectParameter` says the same thing.

**One write is deliberately left.** The LFO's Waveform and SyncTypes are past
`lfoExportedParameters`, so they have no `ParameterID` and no route through the queue; they
still go straight into the LFO. Stage 5 takes them with the rest of the LFO's state.

**Test.** `[clap][protocol]`: push a command, check the engine has *not* changed — a queue
that applied itself on push would be the direct write with extra steps — then flush and check
it has, and that the host reads the same value.

### 6.5 — What stage 5 did

Two commits, because the two halves are separable and only the first one is the crash.

**The engine stops telling anyone anything.** The four virtuals on `Module` are gone, and
nothing replaces them *in the engine*: the plugin publishes what the LFOs did after the
block, and pushes a `ToUI::BaseParameterChanged` when a host event moves something. The
editor sweeps the mailbox on a 30 Hz `juce::Timer`. `ModuleParameters`' two remaining
setters stop being virtual, which is the second of the two ABIs `dsp.cmake` had been
complaining about since the `LE_SW_GUI` flag was deleted.

`publishModulatedValues()` is deliberately not gated on an editor being open. The loop is
five modules by ten `enabled()` checks — noise beside one FFT — and gating it would have
made the only thing that reads the mailbox also the only thing that can test it.

**The strips move to the editor.** `Module` was a `juce::Component` container: a
`std::optional<GUI::ModuleUI>` member plus a `ParameterWidgetsVTable` base holding two
function pointers, planted in every module at construction, that built and destroyed that
effect's widgets. So `Module::Impl<Effect>` inherited `ModuleWidgets<Effect>` and every
module the factory `malloc`ed carried the widget storage for its effect *inline*.

Now `SpectrumWorxEditor` owns the strips and each one holds a
`Utility::IntrusivePtr<Module>`. Three things fall out:

- **`intrusive_ptr_release_deleter` no longer posts a JUCE message.** It called
  `destroyGUI()`, and the reference that reaches zero can be the audio thread's — the chain
  holds one per node while it walks it, on purpose (`moduleChainImpl.hpp:314-320`) — so
  removing a module mid-block allocated a message and posted it from inside the callback.
  A module with a strip simply does not reach zero now.
- **`destroyGUI()` took the processing lock on the message thread.** That was one half of a
  plausible deadlock cycle (§1A). Gone with it.
- **Strips are found by module, not by slot** — at most five, a pointer comparison. An array
  indexed by slot has to be reordered on every drag and every removal, which is the
  bookkeeping that used to go wrong.

**Two things moved out of `sw-dsp` on the way past.** A `ModuleKnob::QuantizationFor`
specialisation was sitting in `core/modules/factory.cpp` — a statement about a widget, in
the module factory. And that factory raised a `juce::AlertWindow` for an effect not in the
edition, which `week_two.md` §2.4 flags as "a live branch that raises a juce::AlertWindow
from a path the audio thread can reach"; it reports through the counted preset reporter now.

**`sw-dsp` is down to `juce_core`, `juce_audio_basics` and `juce_audio_formats`** — no
`juce_gui_basics`, no `juce_graphics`, no `juce_events`. Checked against the generated
compile lines, not against the CMake. Stage 7 takes the last three.

**One thing to know for stage 6.** The strips are built and torn down by the editor now, but
the *chain* is still mutated from the message thread — `setModuleInSlot`, `removeModule`,
`moduleDragEnd`. That is what stage 6 is for; nothing in this stage made it better or worse.

---

## 7. What is still missing

**A backtrace pair from the two-instance deadlock**, one from each side, while it still
reproduces. §1A names a plausible cycle — the message thread holding the message-manager
lock and blocking on the processing lock in `destroyGUI()`, against the audio thread holding
the processing lock and calling `repaint()` into AppKit — but that is derived from reading,
not observed. §1B is a second, independent mechanism and it may be the whole story on its
own. The two are not exclusive, and stage 1 addresses only the second. It stops being
collectable the moment the model changes.

---

## 8. Deliberately not done

These go to `tech_debt.md` as this project's own leavings.

- **Creating an effect allocates on the audio thread.** A granted concession. The command
  handler calls `ModuleFactory::create` inline under `SST_CPPUTILS_RTSAN_DISABLE`. Doing it
  properly needs a main-thread-readable snapshot of the storage factors, which is the one
  piece of machinery this design does not otherwise require.
- **Dragging the base with the LFO active.** The data is plumbed in stage 3 — the model
  carries both values — but the four sites that disable the knob stay, and drawing the sweep
  around a live knob is a skin decision.
- **The preset browser's file IO** stays synchronous on the message thread.
- **`UIEdits` drops gestures when full** — an existing entry, unchanged by this.

---

## 9. Verification

- `sw-tests` green in **both** build trees at every stage; the goldens run in Release only.
- `presetCorpus.txt`, `streamingNames.txt` and `parameterTable.txt` must not move. If a
  digest shifts, a value changed, and that is a bug rather than a snapshot to refresh.
- **By reversion**, as this project does elsewhere: drop the base write and the preset-save
  test reddens; restore one `gui()->` call in `Module` and stage 7's link gate fails; drop
  the `Retire` message and tsan finds the use-after-free.
- rtsan clean with the editor open and every knob dragged; tsan clean on two instances.
- By hand in Logic and Bitwig: the situations that deadlocked, a preset load while audio
  runs, an FFT-size change while audio runs, a slot change from the host's generic panel,
  and save/reload.

## 10. Risks

- **`request_restart` is host-dependent.** CLAP says a host should honour it; `week_two.md`
  item 1's DAW drive is where we find out. Fallback for one that does not: apply the setup
  in the command handler under the same concession as effect creation. The test hosts in
  `tests/clap/` need to implement it either way.
- **The editor is 2,388 lines and reaches the engine in roughly forty places.** Stages 4, 5
  and 7 carry the schedule risk, and `LFODisplay` is the most tangled widget in the tree.
- **Size.** Roughly 10k lines across the editor, module and core layers; two to three weeks.
