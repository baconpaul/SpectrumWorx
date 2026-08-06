# SpectrumWorx — The threading model

Which thread owns what, how the two of them talk, and what neither may do to the
other. Companion to [`parameter_system.md`](parameter_system.md), which is how
parameters are addressed, and [`streaming_format.md`](streaming_format.md),
which is what reaches a file.

Everything described here is in the tree and has tests naming it. What the model
deliberately does *not* solve is in [`tech_debt.md`](tech_debt.md).

---

## 1. What the two sides may not do to each other

Three structural rules. Each is enforced by something that fails — a link error,
an assertion, or a sanitizer report — because each was violated by the 2016
design, and an argument is not an enforcement.

**The engine holds no widget.** `SW::Module` used to own its own editor region —
a `std::optional<GUI::ModuleUI>` member, plus four virtuals whose only purpose
was to push a value into it. It was structural rather than incidental:
`Module::Impl<Effect>` *inherited* `ModuleWidgets<Effect>`, so every module the
factory allocated carried that effect's JUCE widget storage inline, sized at
compile time. `SpectrumWorxEditor` owns the strips now and each one holds a
`Utility::IntrusivePtr<Module>` — the reference runs from the interface to the
engine and never back. §6 is what keeps it that way.

**Nothing the audio thread touches is destroyed under it.**
`ModuleChainBase::forEach` holds an `IntrusivePtr` per node deliberately
(`moduleChainImpl.hpp:314-320`), so when a module leaves the chain the reference
that hits zero can be the audio thread's. Whatever comes out of the engine goes
back to the main thread as a `ToUI::Retire` and is deleted there (§5). A module
that has a strip cannot reach zero on the audio thread at all, because the
strip's reference outlives the chain's.

**The engine reports; it does not raise.** Preset loading counts its problems
into a `PresetLoadReport` and hands it back —
`PresetLoadReport::worthTellingTheUser()` decides whether any of it is the
user's business, `GUI::loadPreset` turns the survivors into **one** dialog, and
`stateLoad` says nothing at all. Loading the 303 factory banks raises 722
`MissingParameter`s and not one window.

---

## 2. The rules

1. **The audio thread owns the engine.** `Program`, the module chain, the
   modules, the LFOs and `Engine::Setup` are its property while the plugin is
   activated. Nothing else dereferences them.
2. **The main thread owns a full copy of that state**, and the widgets. It is
   authoritative for what the user and the host asked for; the engine is
   authoritative for what happened.
3. **They exchange formal messages, in both directions, over two SPSC rings.**
   Ordered, all delivered, heterogeneous payloads.
4. **Continuously varying values cross in a mailbox of atomics, not the ring.**
5. **The audio thread takes no lock.** There is no `processCriticalSection_`; it
   was deleted rather than narrowed, and `Utility::CriticalSection` and
   `ConditionVariable` went with it.
6. **A parameter has a base value and a modulated value, and they are different
   things** (§4).

### Who owns the engine right now

One line, and it is rule 1 written down:

```cpp
bool currentThreadMayMutateEngineState()
{ return !engineIsRunning() || Threading::isAudioThread(); }
```

`engineIsRunning()` is `suspend()`/`resume()` — false until `activate()`, so the
main thread owns the engine before there is an audio thread and again after
there is not. Six assertion sites read it. It is not `#ifndef NDEBUG`: every
publish helper branches on it (§5).

### Which calls are the audio thread's

`Threading::isAudioThread()` answers **"is this call inside one of CLAP's
`[audio-thread]` entry points"**, not "is this thread the audio thread" — a host
with a worker pool (Bitwig and Reaper both have one) delivers successive blocks
of the same plugin on different threads, so there is no such thread to name.

`Threading::ScopedAudioThreadEntry` is one line at the top of every such entry
point. It makes `isAudioThread()` true for the duration and opens a
RealtimeSanitizer realtime region, so an allocation or a lock reached from
anywhere underneath is reported with a stack.

| `clap/plugin.h` | annotation |
|---|---|
| `process` | `[audio-thread & active & processing]` |
| `reset` | `[audio-thread & active]` |
| `start_processing` | `[audio-thread & active & !processing]` |
| `stop_processing` | `[audio-thread & active & processing]` |

Plus one whose annotation is conditional: `clap_plugin_params::flush` is
`[active ? audio-thread : main-thread]` (`ext/params.h:303`), so it takes the
scope **only while active**. Opening it unconditionally would be wrong in the
other direction — telling an inactive plugin that the audio thread owns an
engine the main thread owns.

> **Why the table is worth reading rather than assuming.** The marker was called
> `ScopedAudioCallback` and documented as *"this call is under `process()`"*,
> which is not what `[audio-thread]` means. `reset()` runs *between* blocks, and
> the first host to call it there — `vst3-validator`, through
> `ClapAsVst3::setProcessing(false)` — aborted the plugin on its own mutation
> assert while behaving perfectly correctly. The narrow reading was not a
> simplification; it was a hole.

---

## 3. The three channels

```
                 main thread                              audio thread
                 ───────────                              ────────────
  editor ──┬──▶  MainThreadModel  ──▶ ToEngine ring ──▶   drained at the top of
           │     (full copy)          (commands)          process(), then the engine runs
           │
           ├──◀  MainThreadModel  ◀── ToUI ring    ◀──    base changed, chain changed,
           │                          (events)            retire this pointer
           │
           └──◀  ValueMailbox     ◀── atomics      ◀──    modulated values, per block,
                 (const & to editor)                      coalescing, painting only
```

| `core/threading/messages.hpp` | cases |
|---|---|
| `ToEngine` | `SetBaseParameter`, `SetSlot`, `MoveModule`, `SwapChain`, `SwapSample` |
| `ToUI` | `BaseParameterChanged`, `ChainChanged`, `Retire` |

Both are tagged unions, trivially copyable, owning nothing. Each case says which
side is responsible for a pointer after it lands, and that is the entire
memory-management story: no shared ownership anywhere in the protocol, and
nothing destroyed on the audio thread.

**Why two transports and not one.** The ring is for things where order and
delivery matter: `SetSlot{2, Gain}` followed by `SetSlot{2, none}` does not
coalesce to the second one, and a dropped `Retire{ptr}` is a leak. The mailbox
is for a sampled signal, where every value but the newest is dead on arrival.
`Processor::preProcess()` runs once per host block and writes every enabled
LFO's target, so at 48 kHz with a 32-sample buffer that is 1500 updates per
second per enabled LFO — up to 120,000/s for a full rack — against a UI that
draws at 30 Hz. A FIFO would spend all its bandwidth on values nobody sees; a
mailbox cannot overflow and coalesces by construction. Same message list, two
transports.

**The ring refuses when full.** `SPSCQueue<Message, Capacity>`
(`core/threading/spscQueue.hpp`) keeps free-running counters, masked only on the
way into the array, and `push` declines rather than clobbering the unread tail.
That is the one place it differs from `sst::cpputils::SimpleRingBuffer`, and it
is the whole reason for having our own.

**The mailbox sweeps with `exchange`.** `core/threading/valueMailbox.hpp` is a
value per dense parameter index plus a bitset of what moved, so a write landing
mid-sweep is carried into the next sweep rather than lost.

**Where they are drained.** `drainCommands()` at the top of `process()`, before
the host's own events so a block's automation wins over anything queued before
it began — and also from `paramsFlush()`, because a host with the transport
parked may not be calling `process()` at all. Two callers is safe because CLAP
forbids a host from running flush and process concurrently, so there is still
one consumer. `drainEngineEvents()` runs in `onMainThread()`.

**Ownership: `SpectrumWorxCLAP` owns all three — not the editor.** `paramsValue`,
`paramsValueToText` and `stateSave` are `[main-thread]` calls that happen with
the window shut, so the main-thread model has to outlive the editor and be there
when there has never been one. The editor is handed references at construction
through two `EditorHost` virtuals; the mailbox as `const &`, since the UI only
ever reads it. A consequence worth having: the host's parameter reads do not
touch engine memory at all.

**One more ring, in the other direction.** `SpectrumWorxCLAP::UIEdits` is the
plugin→host leg — gestures and value changes on their way to
`clap_host_params`. It predates the rest and was already correct; `SPSCQueue` is
generalised from it.

---

## 4. Base versus modulated

An LFO does not overwrite its parameter. `Engine::ModuleParameters` keeps one
float per LFO-able parameter — **unmodulated** in the code, *base* here, because
"base parameters" already means the five shared ones — sized and indexed exactly
as the LFO storage is.

| | base | modulated |
|---|---|---|
| written by | user edit, host automation, preset load | the LFO, once per block |
| `paramsValue`, `stateSave`, `savePreset` | ✅ | ✗ |
| ToUI ring — latchable, ordered | ✅ | ✗ |
| ValueMailbox — coalescing, painting | ✗ | ✅ |

`setBaseParameter`/`setEffectParameter` — what a user, the host and a preset go
through — write both. `set*ParameterFromLFOAux` calls `set*ParameterLive` and
writes only the live one, and **emits no events at all**: the plugin publishes
what the LFOs did after the block, and the editor sweeps the mailbox on a 30 Hz
`juce::Timer`.

Two things this buys, and both were bugs before it: a host's generic panel no
longer polls the sweep, and saving a preset while an LFO is running no longer
freezes that LFO's instantaneous output into the file.

The main-thread model carries **both** values, so dragging the base with the LFO
active is a future UI change rather than a future re-plumbing.

**Not the same thing as depth.** `lowerBound`/`upperBound` are absolute values in
the parameter's own units and every preset since 2011 stores them that way.
"Base" here means *the value that applies when the LFO is off, remembered while
it is on*.

---

## 5. Lifetime: publish and retire

Structural change — a slot's effect, a preset, a session, a sample — does not
mutate live engine state. The main thread builds the replacement, publishes one
pointer, and the audio thread hands the old one back to be destroyed:

```
main thread                                  audio thread
───────────                                  ────────────
Program *next = build(…);                    // ← allocation, formatting, file IO
push(SwapChain{next})              ───────▶  live_ = next;              // one store
                                   ◀───────  push(Retire{old})
delete old;                                  // never destroyed under the callback
```

The engine side is two functions, both pure pointer surgery and both legal
inside `process()`: `installModuleInSlot()` — which takes a reference out before
relinking, because unlinking would otherwise call the deleter on the audio
thread — and `swapModuleChain()`, three splices of a circular list with no
destruction.

**`Threading::publish{Slot,ModuleMove,Chain}()` is where the branch lives**
(`core/threading/publish.hpp`), so that no caller has to know which side of
`activate()` it is on: with nothing processing they apply the change directly,
and with audio running they queue it. That is what keeps every headless test
working without a message pump, and what makes the preset loader one code path.

**Modules are built on the main thread.** `Threading::createModuleForSlot()`
does the allocation and the audio thread only links the result. The one
exception is a *host* writing a slot selector: that arrives as a parameter event
inside `process()`, and deferring it would mean a round trip to the main thread
and back — so it still allocates. Recorded in `tech_debt.md`.

**The rack is a function of the main thread's chain.** `resyncModuleRack()` drops
strips whose module has gone, builds strips for modules that have none, and puts
every one of them where `programMain_`'s chain says. It is a recomputation rather
than a diff, because between a click and the engine applying it the rack is what
the user asked for and the engine's chain is what is playing.

Two things ask for it, and both have to. **Whatever changed the main thread's
chain says so** — `addUserAddedModule()`, `moduleAdded()`/`moduleRemoved()` and
`GUI::loadPreset()` each call `refreshModuleRackAsync()`. **And the engine's echo
says so**, `ToUI::ChainChanged`, coalesced, for the changes that originate on the
audio thread — a host writing a slot selector inside `process()`.

The first is not redundant. A preset load fills `programMain_` outright and only
*queues* the engine's copy, so waiting for the echo makes the picture depend on
the host calling `process()` — which Logic does not do for an AudioUnit on a
track that is neither playing nor monitored. That was a live bug: the browser
changed the preset and the strips stayed on the previous one until something made
a block of audio happen. Pinned by *"A preset reaches the rack with no audio
thread running"* (`tests/clap/pluginTests.cpp`), which never calls `process()`.

**The sample is a pointer.** `SpectrumWorxCLAP` holds `Sample *pSample_`,
swapped by `SwapSample`, plus its own `sampleFile_`/`decodedSampleRate_` so that
`currentSampleFile()` and `activate()`'s re-read answer without touching the
audio thread's copy. `Sample::load()` takes no lock: it decodes into an object
nobody else can see.

### The spectral setup is the exception, and it uses CLAP

Changing the FFT size, the overlap factor or the window function reallocates the
whole working set and resizes every module — too much to publish as a pointer,
and `calculateWindowAndWOLAGain()` rewrites the very windows the WOLA path is
reading. So `setGlobalParameter` records `spectralSetupPending_` instead;
`drainCommands()` and `presetChangeEnd()` ask for `clap_host::request_restart()`;
and `deactivate()` applies it, where the audio thread definitionally is not
running.

That also keeps a contract the plugin was breaking. `clap_plugin_latency` says
latency may only change while the plugin is deactivated, and **FFT size *is* the
latency** — `activate()` caches `engineSetup().latencyInSamples()`. A preset that
changes the spectral setup therefore takes the restart with it and installs its
chain in `activate()`.

> `request_restart` has never been answered by a real host. The test hosts in
> `tests/clap/` implement it as a no-op observer, which is enough to say the
> plugin asks and not enough to say a DAW answers. [`todo.md`](todo.md) item 1
> owns finding out.

---

## 6. The layering: `sw-dsp` links no JUCE

The first rule of §1, expressed as a link line rather than as a convention.

**What `sw-dsp` links:** `sst-cpputils`, `sst-plugininfra`, `tinyxml`,
`sw::assets`, and Accelerate or pffft. No JUCE, and nothing that pulls it.

Two things enforce it, and they fail differently:

- **`checkNoJuceInDSP.cmake`** (`ctest -R engine-links-no-juce`) reads
  `compile_commands.json` and fails if any engine translation unit is compiled
  with `-DJUCE_MODULE_AVAILABLE_`. It catches a JUCE module arriving on
  `sw-dsp`'s link line *before* anything includes a header — which is the shape
  this rots back into, because a `target_link_libraries` line looks harmless
  until six months later.
- **`sw-dsp-tests`** links `sw-dsp` and Catch2, full stop. `otool -L` lists
  Foundation, Accelerate and libc++; `nm -u` finds no JUCE symbol at all. It
  catches the other direction — a *test* reaching above the layer it is testing
  — and does so as a link error.

`sw-plugin-tests` is the rest: the CLAP cases, the editor cases and the decoder.
Two live there that look like they belong below — `threadCheckTests` drives a
real `clap_plugin` through its C entry point, and `parameterTableTests`
exercises the host-facing parameter enumeration.

Between them sits **`sw-io`**: `external_audio/sample.cpp` and
`le/spectrumworx/presetFile.cpp`, the two things that open a file. Not `sw-gui`,
because neither draws anything — a plugin reads a session's sample with no
editor open. The format itself speaks no JUCE: `presetStorage.cpp` is
`std::filesystem::path` and `<fstream>`, and `presetFile.cpp` above it is only
`juce::File` conversion. That is what lets the preset tests link without JUCE.

---

## 7. Where the cross-thread state is

The inventory the model is measured by.

| What | Shared how | State |
|---|---|---|
| Parameter edits, interface → engine | `ToEngineQueue`, drained at the top of `process()` and in `paramsFlush()` | ✅ |
| Base-value changes, engine → interface | `ToUIQueue`, drained in `onMainThread()` | ✅ |
| Modulated values, for painting | `ValueMailbox`, written per block, swept at 30 Hz | ✅ |
| Plugin → host notifications | `UIEdits` ring | ✅ |
| Which thread is which | `Threading::{isMainThread,isAudioThread}` | ✅ |
| Who owns the engine | `engineIsRunning()`: the audio thread while activated, the main thread otherwise | ✅ |
| The module chain | `ToEngine::{SetSlot,MoveModule,SwapChain}` + `ToUI::Retire`; built on the main thread, linked on the audio thread | ✅ |
| `Engine::Setup` and the spectral storage | `spectralSetupPending_` + `clap_host::request_restart()`, applied in `deactivate()` | ✅ |
| The `Sample` | `ToEngine::SwapSample` + `ToUI::Retire`; the main thread keeps the file name and the decoded rate | ✅ |
| The module rack | recomputed from `programMain_`'s chain, by whatever changed it and by `ToUI::ChainChanged` | ✅ |
| Editor selection and active control | `SpectrumWorxEditor::{pSelectedModule_,pActiveControl_}`, per editor | ✅ |
| `PopupMenu::menuActive_` | a member, per menu and therefore per editor | ✅ |
| **`LFOImpl::Timer`'s tempo** | three process-wide statics, `std::atomic` — no longer a race, still shared between instances | `tech_debt.md` |
| `Theme::settings()` | process-wide, and arguably correct: these are application preferences | not addressed |

**JUCE has one owner.** `SkinLifetime` builds the `Theme` and installs it as the
default LookAndFeel for as long as at least one editor exists, and touches
JUCE's lifetime not at all — the shim's `juce::ScopedJuceInitialiser_GUI` is the
only thing counting it. Closing an editor used to call `shutdownJuce_GUI()`
against a counter JUCE's own initialiser never saw, which with two instances is
one of them tearing down the message loop the other is running on.
`isThisTheGUIThread()` and `isGUIInitialised()` ask JUCE through
`getInstanceWithoutCreating()` rather than asking a count of *our editors*
whether JUCE was up.

---

## 8. How this is checked

**Assertions.** Six sites read `currentThreadMayMutateEngineState()`. Every
unimplemented message case asserts, so a caller that sends a message nobody
handles says so rather than dropping it.

**Sanitizers.** One `SW_SANITIZER` cache variable rather than a pair of blessed
build directories, applied before `add_subdirectory(libs)` so that it reaches
the dependencies too. It link-tests the flag and refuses with a diagnostic
naming the compiler, because `-fsanitize=realtime` *compiles* on Apple clang and
fails to link:

```
cmake -B build-rtsan -D SW_SANITIZER=realtime \
      -D SW_BUILD_PLUGIN_BUNDLES=OFF \
      -D CMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++
```

`SW_BUILD_PLUGIN_BUNDLES=OFF` because clap-wrapper fetches the VST3 and
AudioUnit SDKs over the network at *configure* time, and a sanitizer tree wants
the test binaries and nothing else.

**A clean sanitizer run and an inactive one look identical**, so check the
instrument by reversion: a `std::malloc` planted in `runEngine()` is reported
with a stack naming the file and line. A `delete new int` is **not** — the
optimiser removes that pair — which is worth knowing before trusting a null
result.

**The cases that carry it.**

| | |
|---|---|
| `tests/core/threadCheckTests.cpp` | thread identity, driven through the C entry point rather than read off the source |
| `tests/core/engineOwnershipTests.cpp` | who may mutate the engine and when; that every block is written; that a spectral change waits and then lands; that a published chain comes back holding what it displaced |
| `tests/core/protocolTests.cpp` | refusal when full, survival past the end of the storage, and two cases that run real threads — 100k messages through an eight-slot ring arriving once and in order, and a mailbox swept while a writer runs flat out |
| `tests/gui/twoInstanceTests.cpp` | closing one editor leaves the other's `MessageManager` alive; selection is independent; ejecting a module and then its ghost |
| `tests/clap/hostInteropTests.cpp` | `reset()` between blocks; flush conditional on `isActive()`; both arms of every `canUseThreadCheck()` branch |
| `tests/clap/pluginTests.cpp` | *"A full rack with LFOs running and an editor open processes cleanly"* and *"Two instances process while their editors come and go"* — the latter with **two real audio threads** and a message thread opening and closing both windows underneath them |

The last two are ordinary functional tests that *become* the acceptance test
when the tree is built with a sanitizer.

**Under tsan, mind the harness.** `REQUIRE` from a worker thread writes Catch2's
shared assertion counter and is reported as a race in
`Catch::RunContext::handleExpr`. `ActivePlugin` has a non-asserting
`processStatus()` for exactly this: the workers record and the main thread
asserts.

**And what no headless case substitutes for.** Logic, Bitwig and Reaper, by
hand. [`todo.md`](todo.md) item 1 has the list.
