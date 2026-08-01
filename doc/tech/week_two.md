# SpectrumWorx — Week Two

Companion to [`implementation_sequence.md`](implementation_sequence.md), which is
the plan, and [`initial_scan.md`](initial_scan.md), which is the analysis. This
is the **re-plan**: what the nine-stage sequence still owes, what re-reading the
tree afterwards turned up that the sequence never knew about, and what order to
do it in now that the thing runs.

**Week one was 27–31 July 2026** — 84 commits, five days. It closed stages 0, 1
(bar CI and installers), 2, 3, 4 (on Linux/arm64), 5.1–5.7, most of 6, 7 and 8.
The plugin builds in four formats, loads, shows its real 2016 editor, passes
audio, exposes 286 parameters, survives a reload, and loads any of its 303
factory presets. `sw-tests` is **108/108 on macOS**, debug and release.

Two things changed the shape of the work in the last day of it, and both are
reasons this document exists rather than another pass over the old one:

1. **It has now been driven by a mouse.** Four bugs came out of that in one
   afternoon — an uninitialised path, a disengaged `std::optional`, a missing
   data source, and an assertion firing on the audio callback — and *none* of
   them had headless coverage. That is the strongest available evidence about
   where the remaining risk is, and it is not where the sequence says.
2. **The sequence's own accounting has drifted from the code.** Stage 8 ends
   "the browser still lists a directory rather than [the factory banks]". It
   does not; `refreshRoot`/`refreshFactory`/`refreshUserDirectory` and
   `FactoryPresets` landed with the presets-button fix. 6.4 is smaller than
   advertised, and §1 says by how much.

**Since then — 01.08.2026 — a third thing, and it outweighs both.** The plugin
has been loaded in Logic and in Bitwig, and it **deadlocks in both** in certain
situations. Everything this document says about threading was written from
reading; it is now written from running, and the conclusion changed: §1 item 3 is
a **redesign of the threading model**, taken as its own project, not the six-step
fixup the audit implied. The audit in §2.2 keeps all of its value as the
inventory that redesign has to satisfy. It is no longer the plan.

---

## Where this actually stands

| | |
|---|---|
| Builds | CLAP, VST3, AUv2, standalone — macOS arm64. Linux arm64 proven at stage 4. Windows arrives as logs. |
| Runs | Standalone, with audio, with the real editor, with presets. **Loads in Logic and in Bitwig, and deadlocks in both** in certain situations — see §1 item 3. |
| Tests | 113 ctest cases: 103 Catch2 + 9 `sw-show-ui --render` + 1 build-property check. Goldens run in Release only. |
| CI | **None.** There is no `.github/`. |
| Warnings | 254 unique sites in a from-scratch Debug build, 248 of them ours — see §3. |
| Identity | ✅ `org.surge-synth-team.spectrumworx`, AU `aufx`/`SpWx` by `SSTx` — see §4. |

**The one feature flag still switched on** is `LE_SW_DISABLE_SIDE_CHANNEL`. It is
`PUBLIC` on `sw-dsp` and changes the layout of `SpectrumWorxEditor`, so every
translation unit has to agree on it. It stands for unfinished work (5.0), not
for a decision. **It does not disable the side-chain input port** — despite the
name it is the external audio *file* loader, and the side chain is wired and
live. §2.8 is the whole story.

---

## 1. The ordered attack

Ordering principle, as written on 01.08.2026: *the plugin has never met a host,
and the audit in §2 says the host is where it will break.*

**It has now met two, and it broke where §2.2 said it would.** SpectrumWorx loads
in Logic and in Bitwig and **deadlocks in both** in certain situations. That is
the single most important fact in this document and it re-sorts the list below:
everything that makes a DAW session survive comes before everything that makes
the build tidy, and item 3 is no longer a fixup.

| # | What | Stage | Size |
|---|---|---|---|
| 0 | **Three bugs found writing this document** | — | hours |
| 1 | **Load it in a DAW**, and run `clap-validator` / `auval` | 1, 5.9 | 1–2 days |
| 2 | ✅ **Plugin identity** — Surge Synth Team ids, before any binary exists | new | *done* |
| 3 | **Threading — a redesign, not the fixup below.** Deadlocks in Logic and Bitwig | 5.8 | its own project |
| 4 | **A real state format**, and the tests it has never had | 5.6 | 3–4 days |
| 5 | ✅ **The owned-window collapse** | 6.4 | *done* |
| 6 | **CI**, three OSes × four formats, with the gates that already exist | 1.5, 5.9 | 3–5 days |
| 7 | **The audio file loader**, and dropping the last flag | 5.0 | 2–3 days |
| 8 | **Property tests for the nine amplifying effects** | 4.4 | 2 days |
| 9 | **The stage 7 tail** — include-what-you-use, and seven macros no build can define | 7 | 3–4 days |
| 10 | **Ship** — licence, README, manual, installers, notarisation | 9 | 1–2 weeks |

### 0 — Three bugs, first, because they are one line each

Found while auditing for this document, not by a test. Details and evidence in
§2.1.

- **`markCurrentProgramAsModified()` calls `_host.isMainThread()` without
  `canUseThreadCheck()`** (`spectrumWorxCLAP.cpp:877`). In a host with no
  `clap.thread-check` that is an `assert` in debug and a **null dereference in
  release**, reachable from every parameter write including from `process()`.
- **A failed `try_lock` returns without writing the output buffer**
  (`spectrumWorxCore.cpp:95`, `spectrumWorxCLAP.cpp:693`). Whenever the UI
  thread holds the processing lock — any preset load, any FFT-size change — the
  host gets whatever was in the buffer, not silence.
- **The preset browser's Save-As button can never be clicked**
  (`presetBrowser.cpp:69`).

### 1 — Load it in a DAW — *begun; it deadlocks*

**Logic and Bitwig have both loaded it, and it deadlocks in both** in certain
situations. So this row is no longer "everything below is speculation until it
happens" — it happened, and it produced the single largest finding in this
document. See item 3, which it re-scoped from a fixup into a redesign.

What it still owes: Reaper, `clap-cpp-validator validate` (not the Rust one — see
stage 1's findings) and `auval -v aufx SpWx SSTx`, and the deliberate drive
through the joins listed below. None of that is worth doing carefully until the
threading is settled, because a deadlock will mask every other finding — but the
*situations that deadlock* are worth writing down now, precisely, while they
still reproduce.

Specifically worth driving, because §2 says these are where the joins are — and
because the first two are prime suspects for the deadlocks:
loading a preset **while audio runs** (fixed once, and the fix is one getter);
changing the FFT size from the host's generic panel while audio runs (§2.2 says
this takes a blocking mutex on the audio thread); putting an effect in a slot
from the host panel rather than the editor (§2.2 says this `malloc`s and
constructs JUCE components on the audio thread); and saving and reloading a
session.

### 2 — Plugin identity ✅ *done, 01.08.2026*

§4 has what was done and what it found. It was second for one reason: **it was
free then and expensive later.** clap-wrapper derives the VST3 class id by
hashing the CLAP id, so changing the CLAP id after a release orphans every VST3
session that used it. Nothing had shipped, so it cost four edits, one copied
plist and a test.

One thing it leaves for item 1: the CLAP installed in
`~/Library/Audio/Plug-Ins/CLAP` is still the old build under the old id.

### 3 — Threading (5.8): a redesign, and its own project

**SpectrumWorx deadlocks in Logic and in Bitwig**, in certain situations. That is
first-hand, from running it, and it settles an argument this document was still
having with itself: the six-step list below is **too small a goal**. It is a list
of individual violations to correct, and what the 2016 threading model needs is
to be replaced rather than patched. That work is deliberately **not** scoped
here; it gets its own pass.

Why the list is the wrong shape for it: every step below makes one call site
safe. None of them changes the fact that there is a single recursive
`processCriticalSection_` taken by the audio thread with `try_lock` and by the UI
thread with a blocking lock, that the same lock is reached from `setParameter`
via the host's event queue, and that the editor mutates the chain without taking
it at all. A deadlock is a property of that arrangement, not of any one site in
it. Fixing six sites inside an arrangement that permits deadlock leaves an
arrangement that permits deadlock.

So treat the audit in §2.2 as **the inventory the redesign has to satisfy**, not
as the plan. It is still the most valuable thing in this document for that
purpose: it names, with file and line, every path from `process()` and
`paramsFlush()` into code the 2016 build only ever ran from the UI thread.

Two things from it are worth doing *first regardless* of what replaces the model,
because they are the instruments the redesign will be read by:

- **The two one-liners in item 0** — the `isMainThread()` null dereference and
  the `try_lock` that returns without writing the output buffer. Both are
  outright bugs and neither depends on the design.
- **Make `currentThreadOwnsTheProcessLock()` real.** It returns a hardcoded
  `true` on every platform this port builds (`spectrumWorxCore.cpp:385`), so all
  six `LE_ASSERT(currentThreadOwnsTheProcessLock())` sites are vacuous. Until it
  is real, the codebase's own account of who holds what is fiction — and a
  redesign that cannot assert its invariants is a rewrite, not a redesign.

Also worth capturing before the deadlocks are fixed, while they still reproduce:
**which situations, in which host, and a stack from each side of the deadlock.**
A held-lock backtrace pair is worth more than the whole of §2.2 to whoever does
this, and it stops being collectable the moment the model changes.

The original list, retained as the inventory rather than the plan:

1. The two one-liners in item 0. *(Promoted above — do these regardless.)*
2. Make `currentThreadOwnsTheProcessLock()` real. *(Promoted above — the
   redesign needs it to be able to assert anything at all.)*
3. Route `stateLoad` through the same deferral as everything else — today it
   rebuilds the module chain on the main thread with no lock at all, while
   `process()` may be walking it.
4. Get `malloc`, `free` and the blocking mutex out of the event handler: slot
   changes and FFT/overlap changes become a main-thread request.
5. Sever `Module::set*Parameter → gui()`. That is what puts `juce::Component`
   mutation and `juce::String` construction on the audio thread, and it is the
   most frequently executed violation of the lot.
6. The editor's three unlocked chain mutations.

Then the things the sequence already asked for: an SPSC ring for audio→main
(`UIEdits` is the pattern and is already correct), thread-identity asserts, and
CI under `-fsanitize=realtime`. **Run under `-fsanitize=address` first** — stage
5's heap corruption was found that way in one run after an afternoon of reading
had not, and `build-asan/` already exists but is configured from an older CMake
and registers only three of the nine GUI tests.

### 4 — A real state format (5.6), and its tests

`stateSave`/`stateLoad` write `(id, value)` pairs and carry **no version stamp**,
while the preset format has carried `Version="2.6"` since 2011. 8.0 discharged
the dependency that was in front of this: `savePreset(std::span<char>, …)` /
`loadPreset(char const *, …)` are exactly the in-memory pair a state blob wants.

It is also **the largest untested surface in the shipping path** — `grep -r
CLAP_EXT_STATE tests/` returns nothing. Untested today: the round trip; the
two-pass load that applies slot selectors first; the "skip whatever no effect
owns" filter; and every error path — bad magic, truncated stream, a `count`
larger than the stream (it sizes a `std::vector` from it before any bound
check), an id no build knows. A `clap_ostream`/`clap_istream` over a
`std::vector<char>` is about thirty lines and the rest are mutations of it.

### 5 — The owned-window collapse (6.4) ✅ *done, 01.08.2026*

> **Done.** Both panels are ordinary child components of the editor, sharing one
> 191 × 363 rectangle at (362, 6) — over the module strips, right edge flush with
> theirs. `OwnedWindowBase`/`OwnedWindow<>`, the Win32 `SetWindowsHookEx`, the
> Carbon `HIView` path and `-framework Carbon` are gone: **923 lines out of
> `src/`, 169 in**. `otool -L` shows no Carbon in the CLAP or in `sw-show-ui`.
> `sw-tests` is **113/113 in both build trees**, the extra case being a new
> `editor-settings` render — the settings panel had never been drawn by anything
> headless, because until now it was a separate desktop window.

**Smaller than the sequence said, and the part that shrank was the part it
worried about.** The preset browser's data source was already done:
`Location{Root,Factory,User}`, `Item{Parent,Section,Folder,Preset}`, three
refreshers, and `selectedPresetData()` joining `FactoryPresets::load()` and
`readPresetFile()` into one `InMemoryPreset`.

**The hard question was: there is no free 191 px column.** The editor is 563 × 376
and fixed. The left column is 213 px wide and every pixel is spoken for — the
in/out/mix knobs, the module-info and LFO column, and, fatally, the two buttons
that open these panels: an overlay that covers its own toggle cannot be shut
again. Three answers were weighed:

| | Verdict |
|---|---|
| **Right-hand overlay** | **Chosen.** Costs the sight of module slots 3–5 while a panel is open — and `globalOpacity` already lets them show through, since `BackgroundImage::paint` applies it and its only subclasses are these two panels. |
| Grow the editor while open | Closest to 2016 and covers nothing, but needs a host that honours `clap_host_gui::request_resize`, and 6.6 ships non-resizable. Cannot be trusted until item 1. |
| `juce::CallOutBox` | Does not fit: 363 px of panel plus ~20 of arrow and border into a 376 px editor. In-editor it clips; on the desktop it is the thing being deleted. |

The panels are therefore **mutually exclusive** — opening either shuts the other
and un-toggles its button. `openOverlay()` is the single place both callers pass
through and it asserts the invariant, and the `editor-settings` render page opens
the browser *first* so that the ctest case actually exercises the swap.

The three smaller decisions that rode along, and one more the overlay exposed:

- `Gradient` set `setAlwaysOnTop(true)` and never cleared it, so a panel opened
  after any module drag painted underneath. Cleared in `moduleDragEnd`.
- The settings panel's 13 px gap (376 of editor height vs 16 of tab bar + a
  347 px page bitmap) is closed: it is sized to what it draws, which is also
  what the preset browser measures.
- `Theme::Settings::globalOpacity` is now labelled "Panel & menu opacity". It
  drives exactly what it always did; over the editor rather than the desktop it
  is *more* use than it was, not less.
- **New: clicking the SpectrumWorx logo opened the settings panel with no page
  at all.** `mouseDown` asked for tab 3 of three, and JUCE clamps an out-of-range
  index to −1. Invisible as a transparent desktop window; an empty panel over the
  editor. It means the About tab, which is index 2, and the indices are an enum
  now with the tab count asserted against it.

Two things deliberately **not** done. `fft.cpp:35`'s `CarbonDummyPointName`
stays — `<Accelerate/Accelerate.h>` still drags in `MacTypes.h`'s `Point`
(verified by compiling one, not by reading), so that workaround is load bearing
whatever the frameworks say. And the JUCE focus assertion the render pages print
is **not** this work's: it comes from `fillFirstSlot()` — `editor` alone renders
silently, `editor-module` asserts with no panel in sight. An offscreen editor can
never be `isShowing()`, so every `grabKeyboardFocus()` in the harness trips it.

### 6 — CI

Nothing exists. The gates, however, mostly do — they are just not wired:
`scripts/check_boost_allowlist.sh` (run by hand), `tests/checkODRHeaderScope.cmake`
(a ctest already), `clang-format` (see §2.5 — the tree is **not** clean any
more), and 108 ctest cases that need **both** build types to mean 108.

Model it on OB-Xf's `build-plugin.yml` with `sst-githubactions/prepare-for-juce`.
Matrix: macos-universal, windows-msvc-x64, windows-arm64, linux-x64, linux-arm64.
Take `add_clapfirst_installer` from two-filters' `basic_installer_clapfirst.cmake`.

The one thing to get right on day one: **run `ctest` in Debug and Release**, not
one of them. The goldens `SKIP` under `!NDEBUG` (documented at
`goldenTests.cpp:287-298`), so Release is the only configuration that renders
DSP, and Debug is the only one that runs the ~1200 asserts. Neither alone is
"108/108".

### 7 — The audio file loader (5.0)

One `doLoad` over `juce::AudioFormatManager`, delete `sampleWin.cpp` (DirectShow
filter graphs) and `sampleMac.cpp` (`FSRef`), drop `LE_SW_DISABLE_SIDE_CHANNEL`.
Roughly fifty lines; the editor code inside the guard is already ported.

Two decisions travel with it, and neither should be made before it: where the
1.4 MB of `assets/samples` lives (embedded like the presets, or installed
beside them), and whether the loader gets a thread of its own — the 2016 answer
was a raw-`pthread` `BackgroundThread` that is now in no target, so this is
design rather than port.

### 8 — Property tests for the nine amplifying effects (4.4)

The golden contract deliberately holds `Pitch_Spring`, `Pitch_Magnet`, `Octaver`,
`PVD_start`, `PVD_stop`, `Imploder`, `Exploder`, `Slew_Limiter` and
`Pitch_Spring_(pvd)` loosely, because a one-ulp FFT difference becomes a
percent-level output difference in each. Loosely is not the same as untested:
they want properties (monotonicity, energy bounds, stability under a repeated
transient) rather than a hash. The list is a measurement, not a property, and a
third platform may add a tenth — which is another reason this follows CI.

### 9 — The stage 7 tail, and the macros that make live code lie

`leConfigurationAndODRHeader.h` is still force-included rather than `#include`d,
and 47 files use `LE_IMPL_NAMESPACE_BEGIN` without declaring where it comes
from. `LE_SW_SDK_BUILD` is defined nowhere and cannot be, so the macro is
unconditionally `namespace X {` — the work is a mechanical deletion across those
47 files plus an include-what-you-use pass, not an argument.

`LE_SW_SDK_BUILD` turns out to be one of **seven** such macros, two of which
silently delete a global parameter from the plugin. §2.4 has the list; it
belongs here because it is the same pass and the same judgement.

### 10 — Ship

Unchanged from stage 9, with one item that should not wait for it: **the licence
contradiction**. `doc/manual/EULA.txt` is the commercial end-user agreement and
it contradicts the repo's GPL-3.0 `LICENSE`; every file header now says
`SPDX-License-Identifier: GPL-3.0-or-later`. JUCE 8 is AGPLv3-or-commercial.
Settling that is a decision, and decisions do not get cheaper.

One thing arrives here from §4: **the standalone's `CFBundleIdentifier` is
clap-wrapper's `SpectrumWorx.standalone`**, and notarisation is the step that
cares. The fix belongs upstream — a `BUNDLE_IDENTIFIER` the standalone wrapper
honours the way the plugin wrappers already do — so it wants a clap-wrapper PR
rather than a local workaround.

---

## 2. What a re-read of the tree found

Four audits over the tree as it stands, after the whole transformation — port
leftovers and dead code, threading and RT-safety, test coverage, and the scope of
6.4. Items here are **not** in the nine-stage sequence; they are what the
sequence did not know about.

The evidence for each claim is a file and a line. The load-bearing ones were
re-read by hand rather than taken on trust: the null dereference and why the test
suite cannot see it, the `try_lock` return, the vacuous lock assertion, the dead
Save-As button, the undefined macros and the table that keeps `factory.cpp`'s
message box unreachable, the linked Carbon framework, the three
declared-never-defined members, the panel and editor bitmap sizes, and every
number in §3 and §4. **The multi-hop call-tree claims in 2.2 were not** — they
are traced from the sources and each names its steps, but re-read the chain
before acting on any one of them.

### 2.1 Three bugs to fix before anything else

**a. A null dereference reachable from every parameter write.**
`spectrumWorxCLAP.cpp:870-885`:

```cpp
void SpectrumWorxCLAP::markCurrentProgramAsModified() const
{
    if (!_host.canUseState())
        return;
    …
    if (_host.isMainThread())            // :877
```

`clap-helpers`' `HostProxy::isMainThread()` is
`assert(canUseThreadCheck()); return _hostThreadCheck->is_main_thread(_host);`
(`host-proxy.hxx:135-138`), and `_hostThreadCheck` is an **optional** extension.
A host without `clap.thread-check` gets an assertion in debug and a null
dereference in release.

Why 108/108 is green anyway, and why that is the interesting part: the test
host in `tests/clap/pluginTests.cpp:71-73` returns an extension for
`CLAP_EXT_PARAMS` **and nothing else**, so `canUseState()` is false and the
function returns one line earlier. A real DAW provides state. *The test host is
too thin to reach the bug* — see 2.3.

**b. A failed `try_lock` leaves the host's buffer untouched.**
`SpectrumWorxCore::process` opens with `if (!processCriticalSection_.try_lock())
return;` (`spectrumWorxCore.cpp:95`), and `SpectrumWorxCLAP::runEngine` only
zeroes channels **at or above** the configured count afterwards
(`spectrumWorxCLAP.cpp:698-699`). So when the lock is held — a preset load, an
FFT-size change from the settings panel — the plugin returns
`CLAP_PROCESS_CONTINUE` having written nothing, and the host plays whatever was
in the buffer. Dropping to silence or passing the input through are both
defensible; writing nothing is not.

**c. Save-As is dead.** `presetBrowser.cpp:69` calls
`saveAs_.setEnabled(enablePresetSaving())` during member initialisation, when
`location_` is still its default `Root`, so it is disabled — and nothing ever
re-enables it. `presetSelectionChanged` (`:195-198`) and `refreshAndSelectPreset`
(`:886-888`) touch `save_`, `delete_` and the comment box, and not `saveAs_`.
Mine, from stage 8.

### 2.2 Threading: the audit

The sequence budgets 1–2 weeks for 5.8 and describes the starting point as "a
`BackgroundThread`, a `GUI::Lock` over `MessageManagerLock`, and no lock-free
parameter queue". Two thirds of that is stale, and what is actually there is
worse.

**Stale:** `GUI::Lock` (`gui.hpp:377-386`) has **no call sites anywhere in
`src/`**. `BackgroundThread` (`spectrumWorx.hpp:71-205`, raw `pthread_create`) is
in `spectrumWorx.cpp`, which is in no target. `juce::AsyncUpdater` and
`juce::Timer` appear **zero** times in `src/`. All three should simply be
deleted.

**Correct already:** `SpectrumWorxCLAP::UIEdits` is a genuine SPSC ring —
1024 entries stored inline, no allocation, relaxed/acquire/release ordering
written correctly, producer the UI thread at three call sites, consumer whichever
of `process()`/`paramsFlush()` runs. Its drop-on-full policy is right for
`Kind::Value` and wrong for gestures: dropping a `GestureBegin` whose
`GestureEnd` survives leaves the host with an unbalanced gesture. Worth a
follow-up, not a blocker. `pendingRescan_`/`pendingMarkDirty_` are exactly the
deferral pattern 5.8 wants, applied to two flags rather than to engine state.

**What is actually wrong**, all of it reachable from `process()` or
`paramsFlush()` via `handleEvent → setParameter`:

| | What happens | Where |
|---|---|---|
| **malloc/free + JUCE construction** | A host writing a **slot selector** destroys and creates a module: `std::free`, `std::malloc` (`factory.cpp:173`), an `AlignedHeapBuffer::resize`, and then `ModuleUI::moveToSlot` → `juce::Component::setTopLeftPosition` **with no thread check at all** | `automatedModuleChain.hpp:103-138`, `editorModuleInitialiser.hpp:61-74` |
| **A blocking mutex + full engine realloc** | A host writing **FFT size or overlap factor** takes `getProcessingLock()` unconditionally and then reallocates the whole spectral working set and every module | `spectrumWorxCore.cpp:648-661` |
| **`juce::String` on the audio thread** | Any automated **module parameter** whose control is the active one reaches `updateActiveControlValue()` → `getValueText()` | `moduleDSPAndGUI.cpp:140-167`, `spectrumWorxEditor.cpp:617-631` |
| **A widget write every block** | Every enabled **LFO** drives `control.setValue()` from `preProcess()`. `FrequencyRange::canUseWriteAccessIndex()` even encodes this as intended: `return (!isThisTheGUIThread() \|\| …)` | `moduleDSPAndGUI.cpp:170-186`, `auxiliaryComponents.cpp:452` |
| **`stateLoad` rebuilds the chain unlocked** | On the main thread, while active, with no synchronisation against a running `process()`. An intrusive linked list that `preProcessAll` walks | `spectrumWorxCLAP.cpp:925-978` |
| **The editor mutates the chain unlocked** | `setModuleInSlot`, `moduleDragEnd`, `removeModule` — none takes `getProcessingLock()`, which exists and is never called | `spectrumWorxEditor.cpp:717`, `:483`, `:672` |
| **The guard is a no-op** | `currentThreadOwnsTheProcessLock()` returns a hardcoded `true` outside `_WIN32`; the Windows arm `reinterpret_cast`s a `std::recursive_mutex` to a `CRITICAL_SECTION`, which the header admits is invalid | `spectrumWorxCore.cpp:376-388` |

Two more worth knowing, both dormant rather than live:

- `~SpectrumWorxEditor` spin-waits on two plain `bool`s read through a
  `volatile` cast (`spectrumWorxEditor.cpp:216-222`). Their producers carry the
  comment "This gets called from a non GUI thread", but in this build both
  callers turn out to be GUI-thread-only. Re-wiring the LFO display in 5.8 would
  wake them up.
- `SpectrumWorxEditor::updateGlobalParameterWidget<>` and
  `updateForGlobalParameterChange()` (`spectrumWorxEditor.cpp:1065-1104`) have
  **no callers** — their only caller was the deleted 2016 plugin class. So host
  automation of the six global parameters currently never moves the editor's
  knobs. That is a live UX bug, and wiring it back naively recreates row three of
  the table above.

### 2.3 Tests: where the coverage is not

108 cases is a good number attached to an uneven distribution. The DSP, the
parameter model and the preset format are covered thoroughly; the host layer and
the GUI are covered at the edges.

**The holes, by value:**

| Hole | State |
|---|---|
| **CLAP state save/load** | Nothing. Zero hits for `CLAP_EXT_STATE` under `tests/`. The largest untested surface in the shipping path. |
| **Sequential preset loads into one engine** | `presetCorpusTests.cpp:110` deliberately uses one engine per preset to avoid the merge path — so "load preset B on top of preset A", which is what a user does, is untested. |
| **Malformed / truncated / missing preset** | Nothing. The corpus proves 303 happy paths. The `unknownEffect` and `missing` counters are asserted zero and never driven above zero, so the reporting path is unexercised. `saveTo()`'s refusal to overrun is never triggered because every test hands it a 1 MiB buffer. |
| **The side-chain port is never fed** | Re-verified 01.08.2026 and it is worse than one line: `ActivePlugin::process` hardcodes `audio_inputs_count = 1` (`pluginTests.cpp:218`), the port test only ever calls `ports->get(…, 0, …)` so the Side Chain port's *info* is unasserted, and `goldens/engineHarness.cpp:181` passes `inputPointers.data()` as **both** main and side. **Seven** side-chain effects are golden-pinned only in the degenerate side == main case. §2.8 has the recipe. |
| **The test host is too thin** | It offers `clap.params` and nothing else — which is precisely why 2.1a is invisible. Adding `clap.state`, `clap.thread-check`, and a deliberate *without*-thread-check variant is the fix, and it is small. |
| **`lfoImpl.cpp` has no direct test** | Only LFO 0 of module 0 targeting Gain is ever exercised. Waveform shapes, sync types, `PeriodScale` snapping, `LowerBound > UpperBound`, an LFO on an enumerated target, several at once — none. A value-table golden fits the existing pattern. |
| **Both text conversions are stubs, and they are one job** | `paramsTextToValue` is `return false` (`spectrumWorxCLAP.cpp:469`); `paramsValueToText` **ignores the value it is given** and prints the parameter's current one (`:414-441`). Both are documented at length with a shared `\todo`: give `AutomatedParameterPrinter` an arm that takes a value *and* the live parameter, so an LFO's dynamic range has an owner to validate against. Host-visible in every automation lane tooltip, and unpinned by any test. |
| **The GUI tests assert exit code only** | `renderPage()` writes a PNG and returns 0. A page that paints solid black passes. Blank/uniform-colour detection is about ten lines and would make the existing nine tests assert something — and it is what would have caught the empty settings panel 6.4 found by looking at a render. |
| **1 of 57 effects, 1 of 18 banks** | `SW_SHOW_UI_EFFECT`, `SW_SHOW_UI_PRESET` and `SW_SHOW_UI_PRESET_SWEEP` exist and are manual-only. A CMake `foreach` over the effect list is four lines for 57× the GUI breadth. |
| **`ctest -LE slow` skips nothing** | No test in the repo sets `LABELS`; the one labelled case went with `check_gui_flag_parity.py`. Either re-establish the label or stop recommending the flag. |

### 2.4 Macros that make live code lie

The highest-value finding of the four audits, and one no stage in the sequence
owns. **Seven feature macros are tested by `#if` in compiled translation units
and defined nowhere a live build can reach.** Two of them are defined *only* in
`src/le/spectrumworx/engine/configuration.cmake`, which is reachable only from
`src/legacy-build.cmake`, which nothing includes.

| Macro | Live `#if` sites | Consequence |
|---|---|---|
| `LE_SW_ENGINE_INPUT_MODE` | ~20, across `spectrumWorxCore.{hpp,cpp}`, `host2Plugin.hpp`, `plugin2Host.{hpp,cpp}`, `engine/parameters.{hpp,cpp}`, `spectrumWorxEditor.{hpp,cpp}` | Permanently 0, so the **InputMode global parameter can never exist**. Stage 5 already found one bug caused by miscounting it. |
| `LE_SW_ENGINE_WINDOW_PRESUM` | ~10, incl. `presets.{hpp,cpp}`, `setup.cpp`, `processor.cpp` | Same: **WindowSizeFactor is permanently absent**. |
| `LE_SW_SDK_BUILD` | `musicalScales`, `presets.{hpp,cpp}`, `module.cpp`, `tuneWorxImpl.cpp` | Already known dead (7's tail names it). |
| `LE_SW_FMOD` | ~13, incl. `spectrumWorxCore.cpp`, `plugin2Host.cpp`, `spectrumWorxEditor.cpp` | The FMOD backend was deleted in stage 0. |
| `LE_SW_FULL` | `factory.cpp:160`, `presets.cpp:525`, `moduleMenuHolder.cpp:45` | The paid/demo SKU gate — the last remnant of what 0.4 removed. |
| `LE_SW_PURE_ANALYSIS` | `pitchDetector.{hpp,cpp}`, `module.cpp`, `processor.cpp` | Dead SKU. |
| `LE_SW_TW_RETUNE_TEST` | `tuneWorxImpl.{hpp,cpp}` ×4 | Dead debug switch, its `#define` commented out beside it. |

Two of these are worth more than tidying:

- **`LE_SW_FULL` leaves a live branch that raises a `juce::AlertWindow` from a
  path the audio thread can reach** (`factory.cpp:159-167`). The only thing
  stopping it is `Effects::includedEffects`, a `constexpr` table that is
  currently all-`true`. A compile-time table is not a thread guard.
- **`LE_HAS_NT2` / `LE_MATH_USE_NT2`** account for roughly **534 lines across 33
  regions of `le/math/vector.cpp`** — about a quarter of the file — that cannot
  be reached now stage 4 is done. Careful when deleting: `math.cpp:414` and
  `conversion.cpp:83` spell it `#if defined(__APPLE__) || !defined(LE_HAS_NT2)`,
  and those are the **live** arms.

The fix is a decision, not a sweep: either give the two engine macros a real
definition in a live `.cmake` (they describe engine features somebody may want
back), or strip the branches. Either way, **the four orphan build files that are
their only definition sites** — `src/core/{configuration,sources}.cmake`,
`src/le/spectrumworx/engine/configuration.cmake`,
`src/le/utility/CMakeLists.txt` — should carry the same "record, not build"
banner `legacy-build.cmake` has.

### 2.5 Dead weight

37 `.cpp`/`.mm` files, **8,717 lines**, are in the tree and in no target.
`tests/` and `tools/` have none — every orphan is under `src/`. Worth removing
not for tidiness but because each one makes the next audit harder: the threading
audit spent real effort proving that `BackgroundThread` and `GUI::Lock` do not
matter.

**Free deletions, no decision needed** (~1,500 lines):
`core/modules/{moduleGUI.cpp,moduleGUI.hpp,moduleDSP.hpp}` (superseded by
`moduleDSPAndGUI.cpp`, and their own comments say so), `src/debugConsole.cpp`,
`le/build/{precompiledHeaders.{cpp,hpp},juceIncludeWrapper.hpp}` (no target uses
a PCH), `le/utility/{conditionVariable.hpp,pimpl.hpp,pimplPrivate.hpp,
entryPoint.hpp,filesystemImpl.inl}`, `le/plugins/{entryPoint.hpp,plugin.hpp}`
(0.3 kept `le/plugins/` "until `le/plugins/clap/` works" — the VST2/AU/FMOD/Unity
backends are already gone, and these two are what is left), `GUI::Lock`,
~~`gui.mm`'s `hideCursor`/`showCursor`~~ (gone with 6.4), and eight symbols that
are declared and never defined — including **three public members of `SpectrumWorxCLAP`**
(`cycleModuleFromUI`, `requestRescanFromUI`, `effectIn`, `spectrumWorxCLAP.hpp:227-233`).
The sequence says stage 6 deleted those three with `stubEditor.cpp`; it deleted
their definitions and callers and left the declarations.

**Platform arms with no platform** (~1,100 lines): `le/utility/`'s Android, JNI,
Matlab and MSVC-universal-build files. `filesystemWindows.cpp` and
`filesystemApple.cpp` are the two to think about rather than delete — either they
come back for the Windows build, or `sst-plugininfra` already covers them.

**Decisions rather than cleanup:**

- **`src/spectrumWorx.{cpp,hpp}`** (1,899 lines) — the 2016 plugin class the CLAP
  replaced, the last `boost/mmap` reference, and the home of `BackgroundThread`
  and its `pthread_cancel`. Stage 0 kept it as a reference for stage 5; stage 5
  is done, and 5.0 is the last thing that needs to read it.
- **`effects/_unfinished/`** — 16 effects, 3,778 lines. `initial_scan.md:768`
  says read before deleting. A branch or an `attic/` gets it out of
  `git ls-files 'src/**'` without losing it.
- **Four finished effects that were never shipped** — `vocoder`, `synth`,
  `talk_box`, `dissonancizer`, 1,325 lines. **Not port leftovers**: the 2016
  `effectsList.cmake` already had three of them commented out. `effectsList.hpp`
  fixes the count at 57 and the order is ABI, so appending them is legal and
  reordering is not.
- **`src/nt2_static_fft/`** — 5,519 lines, the largest orphan, kept by 0.3 as the
  stage 4 reference. Stage 4 is done and `tests/math/fftTests.cpp` is the grading
  harness now. Does the reference still earn its place?

### 2.6 Live platform code that should not be

- ✅ **Carbon is not just present, it is linked.** `src/gui.cmake:52-53` compiled
  `gui/gui.mm` into `sw-gui-widgets` **and linked `-framework Carbon`**, on the
  only platform this has run on. `gui.mm:25` included `Carbon/Carbon.h`
  unconditionally, and ~155 of its 367 lines were `#if !JUCE_64BIT` — unreachable
  on any 64-bit build. `SpectrumWorxEditor::attachToHostWindow` had three
  overloads and **no callers at all** now the shim parents the editor.
  *All gone with 6.4; `otool -L` confirms.* The one survivor is `fft.cpp:35`'s
  `CarbonDummyPointName`, and it has to be: `<Accelerate/Accelerate.h>` still
  pulls in `MacTypes.h`'s `Point` regardless of what is linked.
- ✅ **A process-wide `SetWindowsHookEx(WH_CALLWNDPROC, …)`** — `gui.cpp:567`, with
  `LE_ASSERT(wndProcHook); //...mrmlj...better error handling desired...` on the
  next line. A message hook installed process-wide by a plugin is the kind of
  thing that gets a plugin blacklisted by a host. It was `OwnedWindowBase`'s, and
  went with it — **before** the Windows build gets driven in anger, which was the
  argument for doing 6.4 when it was done.
- **Hand-written weak `strnlen`/`wcsnlen`** — `gui.cpp:1657-1680`, "OSX 10.6 does
  not provide std::strnlen", with the `!__LP64__` guard **commented out**. So
  they are compiled into every macOS build today, in 2026.
- **`FSRef` and DirectShow** — `sampleMac.cpp` and `sampleWin.cpp`, 1,355 lines
  between them. `sampleMac.cpp` already calls `makeFSRefFromPath()`, which no
  longer exists; it only "works" because the file is in no target. Both go with
  5.0.

### 2.7 Small things that have drifted

- **The tree is no longer clang-format clean.** 67 files fail
  `clang-format --dry-run -Werror` (21.1.5), and **56 of them were touched by the
  stage 7 de-Boost range** `42c720a..db8c423` — mostly effect headers, from the
  variadic-macro conversion. Stage 0.6 established "format-stable" and recorded
  the reformat in `.git-blame-ignore-revs`; that property has lapsed. One
  reformat commit plus the CI gate in item 6. Pin the clang-format version in CI
  while you are there, since nothing in `.clang-format` does.
- **The preset browser's header strip prints
  `currentDirectory_.getFullPathName()` unconditionally**
  (`presetBrowser.cpp:907-912`),
  so `Root` and `Factory` show a stale or empty path. Four other file paths —
  `file()`, `selectedFile()`, the rename path, `browseArrow_`'s folder chooser —
  are gated only by button enablement, and the chooser will happily walk into the
  on-disk `assets/presets` and present a factory bank as writable.
- **The browser does not remember where it was.** `~PresetBrowser` persists
  `currentDirectory_` but not `location_`, so it always reopens at `Root`.
- **`README.md` is the 2016 one.** Ten lines, says "The code does not compile,
  the build does not work", and links to `source/…` paths that stage 0 deleted.
  It is the first thing anyone sees.
- **`build-asan/` is configured from an older CMake** and registers three of the
  nine GUI ctest cases, so ASan never runs the editor, the preset browser or the
  module pages — the three things most likely to be interesting under it.
- **The `#pragma message` in `vector.cpp:30`** says "LE.Math.Vector using OS X
  10.4 Accelerate framework". It is 2026.

### 2.8 The side chain: wired, plausible, unverified

Traced end to end on 01.08.2026, because "what is the state of the side chain"
turned out to have four different answers depending on which file you opened.

**The ports are right.** `audioPortsCount` is 2 in, 1 out
(`spectrumWorxCLAP.cpp:178`); all three are `CLAP_PORT_STEREO` with a
`channel_count` of 2. Input 0 is "Main In" with `CLAP_AUDIO_PORT_IS_MAIN`,
input 1 is "Side Chain" with no flags, output 0 is "Main Out".
`in_place_pair` is `CLAP_INVALID_ID`
everywhere, deliberately: with an input gain of exactly one the core hands the
host's own pointers to the WOLA path, which has not been audited for aliasing.

**The engine agrees and the DSP path is live.** `activate()` hardwires
`setNumberOfChannels(2, 2)` — two main, two side — so `hasSideChannel()` is true;
the side channel gets its own OLA FIFO (`sideOLA_`) and its own forward FFT
through `ChannelData::setNewTimeDomainData`
(`channelBuffers.cpp:92-100`). **Seven** of the 57 effects take
`Engine::MainSideChannelData_ReIm` rather than `MainChannelData_ReIm` and so read
it: `colorifer`, `blender`, `vocoder`, `pitch_follower`, `inserter`,
`talking_wind`, `burrito`.

**Three things that are easy to get wrong about it:**

- **`LE_SW_DISABLE_SIDE_CHANNEL` is not this.** Despite the name it is the
  *external audio file* feature — `SampleArea`, the editor's "External audio"
  box, `registerSampleLoadedListener`. It touches GUI, editor and preset files
  only; the side-chain port and the DSP that reads it are unaffected by it. It
  belongs to item 7 (5.0). Nothing about dropping that flag turns the side chain
  on, because the side chain is already on.
- **`LE_SW_ENGINE_INPUT_MODE = 0` does not disable it either.** All it removes is
  the user-facing InputMode parameter (Mono / Mono+SideChain / Stereo /
  Stereo+SideChain) and the parameter↔channel-config synchronisation. With it off
  the CLAP's explicit `setNumberOfChannels(2, 2)` stands unopposed, which for a
  fixed port layout is what you want anyway. This is the one entry in §2.4's
  table whose absence is currently *harmless*.
- **An unpatched side chain is the main input, not silence.** `runEngine`
  (`spectrumWorxCLAP.cpp:691-694`) falls back to `input.data32` when the host has
  not connected port 1. So a Blender with nothing patched blends the signal with
  itself. Defensible, but it is a choice and it is undocumented anywhere a user
  would find it.

**The test pattern that is missing, and it is small.** Nothing has ever put a
*different* signal on port 1, at either level, so all seven effects are pinned
only where side == main — the one case in which a side-chain effect cannot be
distinguished from a bug that ignores the side chain entirely.

1. **`tests/goldens/engineHarness.cpp`** — `render()` builds `inputChannels` and
   passes `inputPointers.data()` twice (`:181`). Give it a second signal and a
   second pointer array: `sideChannels`, generated from a *different* `signal`
   than the main one (the generator already takes one), and pass
   `sidePointers.data()` as the second argument. The comment at `:166` already
   claims the side-chain effects "are driven separately"; nothing in `tests/`
   does, so this makes an existing comment true.
2. **A golden case per side-chain effect**, with main and side deliberately
   dissimilar — a sine against noise is enough, and the corpus already has both
   generators. That is what turns seven degenerate goldens into real ones.
3. **`tests/clap/pluginTests.cpp`** — `ActivePlugin::process` sets
   `audio_inputs_count = 1` (`:218`). A second `clap_audio_buffer` with its own
   two channels, and `audio_inputs_count = 2`, exercises the port-1 branch of
   `runEngine` for the first time. Keep a variant that still passes 1, because
   the fallback-to-main path is also untested and is what most hosts will do.
4. **Assert the Side Chain port's info.** The port test (`:439`) checks
   `count(true) == 2` and then only calls `ports->get(…, 0, …)`. One more call
   with index 1, asserting the id, the stereo type and that `IS_MAIN` is clear.

**One loose end.** Every effect declares `static bool const usesSideChannel`, and
`effects.hpp:61` documents it as part of the effect contract — but a grep of
`src/` and `tests/` finds **no reader**. Dispatch is by parameter type, not by
that constant. Either something generated consumes it and the grep is wrong, or
57 files are maintaining dead metadata; worth five minutes with item 9.

---

## 3. The warnings

A from-scratch Debug configure and build of the whole tree — every target,
including the four plugin formats, the tests and `sw-show-ui`.

**420 warning lines, 254 unique sites. 248 of them are ours.**

The first thing that finding is about is not any particular warning:

> **138 of our 139 translation-unit compilations carry no warning flags at all.**
> The exception is `src/clap-first/swClapEntry.cpp`, compiled twice, which
> inherits `-Wall -Wextra -Wpedantic -Werror` from clap-wrapper.

So the 254 are clang's **on-by-default** set. Sampling six representative files
with `-Wall -Wextra -Wno-unused-parameter` added produced **425 further
warnings** — 365 of them `-Wunknown-pragmas`, from the `#pragma warning(push/
disable/pop)` blocks that are scattered through the tree for MSVC's benefit.

### The inventory

| Flag | Unique sites | Whose |
|---|---:|---|
| `-Wundefined-var-template` | 218 | ours |
| `-Wdeprecated-declarations` | 13 | ours |
| `-Wassume` | 9 | ours |
| `-Wvla-cxx-extension` | 3 | rtaudio, rtmidi |
| `-Wimplicit-const-int-float-conversion` | 3 | 2 ours, 1 vst3sdk |
| `-Wnontrivial-memcall` | 2 | vst3sdk |
| `-W#pragma-messages` | 2 | ours, deliberate |
| `-Wdeprecated-volatile` | 1 | ours |
| `-Wdeprecated-builtins` | 1 | ours |
| `-Wdeprecated-anon-enum-enum-conversion` | 1 | ours |
| `ld: ignoring duplicate libraries` | 1 | ours |

### What each one is, and how to dismiss it

**`-Wundefined-var-template` — 218 sites, 86 % of the total, and one cause.**
`Parameters::Name<Parameter>` declares `static char const string_[]` with no
definition (`uiElements.hpp:62-65`); each parameter's name is supplied by an
explicit specialisation emitted by `EFFECT_PARAMETER_NAME` into a `.cpp`
(`uiElements.hpp:240-252`). Clang cannot see the definition where `name()`
instantiates, so it warns — and it is right to: this is the *exact* shape of the
release-only link error stage 5 found in `ModuleParameters::parameterInfos()`.
Today it links because every specialisation happens to be reachable; nothing
enforces that.

The tree already contains the fix, applied to five parameters and no more —
`baseParametersUIElements.hpp:53-59` declares the specialisations in the header,
under an `#ifdef __GNUC__` whose comment is about 2012 unity builds. It is also
the proof of what the fix has to look like: those five **still warn** from
`plugin2Host.cpp:599`, which instantiates `name<BaseParameters::Bypass>()` and
does not include that header. **So the declaration has to travel with the
parameter**, not sit in a separate UI-elements header a call site may skip —
either `LE_DEFINE_PARAMETER` emits it, or `EFFECT_PARAMETER_NAME` gains a
companion that each effect's own header uses. That is stage 7 work in spirit —
it is the parameter system — and worth doing there rather than as a suppression.

Suppressing it with `-Wno-undefined-var-template` is the wrong answer, because
the warning is the only thing currently checking that a parameter's name exists
at all.

**`-Wdeprecated-declarations` — 13 sites, four unrelated causes.**

- **`sprintf`, 8 sites** — `plugin2Host.cpp:633,699`, `lexicalCast.cpp:48,56,80`,
  `spectrumWorxEditor.cpp:1403`, `presetBrowser.cpp:577`,
  `assertionHandler.cpp:242`. Deprecated by the macOS SDK, not by C++.
  `snprintf` with the real buffer size at each — the sizes are all statically
  known. Mechanical, and it is a genuine hardening: `assertionHandler.cpp`'s is
  writing into a fixed 4096-byte buffer it has already `strcat`ed into.
- **`juce::Font::getStringWidth`, 2 sites** — `gui.cpp:1272,1291`. Deprecated in
  JUCE 8 in favour of `GlyphArrangement`/`TextLayout`. Both sites are computing a
  button's width from its label; `juce::GlyphArrangement::getStringWidth(font,
  text)` is the direct replacement.
- **`std::iterator`, 1 site** — `moduleChainImpl.hpp:117`,
  `chain_const_iterator` derives from it. Deprecated in C++17. Spell out the five
  member typedefs; it is five lines.
- **`std::is_pod`, 2 sites** — `clear.hpp:38`. Deprecated in C++20. The intent is
  "safe to `memset`", which is `std::is_trivially_copyable_v` plus
  `std::is_trivially_default_constructible_v`.

**`-Wdeprecated-builtins` — 1 site.** The other half of the same line:
`__has_trivial_assign(POD)`, a clang builtin that has been deprecated in favour
of `__is_trivially_assignable`. It is there because of a 2011 Boost bug the
comment links to. Both halves of `clear.hpp:38` go together.

**`-Wassume` — 9 sites, and every one of them is currently inert.**
"assumption is ignored because it contains (potential) side-effects": on clang
`LE_ASSUME(x)` expands to `LE_ASSERT_MSG(x, …); __builtin_assume(x)`
(`platformSpecifics.hpp:159-161`), and clang drops the `__builtin_assume` half
whenever the condition contains a call — which all nine do, e.g.
`LE_ASSUME(Host2PluginInteropControler::blockAutomation() == false)`
(`spectrumWorxCore.hpp:309`). So the assert fires and the optimiser hint does
nothing. **Dismiss by admitting what they are**: replace all nine with
`LE_ASSERT`. Stage 0.6 already deleted the UB-adjacent `LE_ASSUME(this)` family
for the same reason; this is the rest of that job. One of the nine is worth a
second look on its own — `spectrumWorxCore.cpp:598` assumes
`!timingInformationChange.timingInfoChanged()`, which the CLAP made false when it
started feeding real tempo. It appears unreachable; if it is not, `LE_ASSUME` on
a false predicate is UB in release.

**`-Wimplicit-const-int-float-conversion` — 2 ours.** `math.hpp:705` asserts
`floatingPointValue < std::numeric_limits<int>::max()`, where the `int` becomes
`2147483648.0f` — one more than it says. `math.cpp:841` divides by
`numeric_limits<uint64_t>::max()` as a `double`, likewise rounded up. Both are
harmless as written and both hide their intent. Write the constant the compiler
actually uses: `< 2147483648.0f` and `* 0x1p-64`, with a comment.

**`-Wdeprecated-volatile` — 1 site.** `processor.cpp:955-959` takes
`float const volatile mixAmount` **on `__APPLE__` only**, with the comment
"broken codegen by Xcode 7.1(.1) Clang". Xcode 7.1 is 2015. Deprecated in C++20.
Delete the `volatile` and the `#ifdef` around it, then **re-run the release
goldens** — that is exactly what they are for, and if the workaround was load
bearing they will say so.

**`-Wdeprecated-anon-enum-enum-conversion` — 1 site.**
`spectrumWorxCore.cpp:77` computes `maxNumberOfInputs - maxNumberOfOutputs`,
which are two *different* unnamed enums (`spectrumWorxCore.hpp:62-70`). Deprecated
in C++20. They are unnamed enums because of a comment reading
"`...mrmlj...Xcode7 linker errors`"; C++17 inline variables solved that.
`static constexpr unsigned` both, and the arithmetic stops being a conversion.

**`-W#pragma-messages` — 2 sites, ours, deliberate.** "LEB: assertion handling
enabled" (`assertionHandler.cpp:69`) and "LE.Math.Vector using OS X 10.4
Accelerate framework" (`vector.cpp:30`). Keep the first; it tells you which
configuration you built. Fix the text of the second or drop it — the tree has
had a portable arm since stage 4 and the message names an OS from 2005.

**`ld: ignoring duplicate libraries: 'src/libsw-gui-resources.a'` — 2 links.**
`sw-dsp` already links `sw-gui-resources` `PUBLIC` (`dsp.cmake:179`), and
`sw-tests` (`tests/CMakeLists.txt:41`) and `sw-show-ui`
(`tools/show-ui/CMakeLists.txt:23`) name it again. Drop the two redundant
entries.

**Third-party — 6 sites, not ours.** rtaudio and rtmidi use C99 VLAs in C++
(`-Wvla-cxx-extension`); the VST3 SDK `memset`s a non-trivially-copyable
`FVariant` and rounds `INT64_MAX` into a `double`. All four come in through
clap-wrapper's CPM fetches. Do not patch them. If they become noisy under a
warning baseline, the lever is `SYSTEM` on the include directories, applied to
those targets only.

### The proposed baseline

Not "turn on `-Werror`". In order:

1. Fix the nine classes above (everything except the 218, which is one change to
   the parameter system, and the six third-party ones).
2. Add `-Wall -Wextra -Wno-unused-parameter` **to our targets only**, via the
   same per-source mechanism `sw_force_include_odr_header` already uses to decide
   what "ours" means — CMake cannot express it any other way, and 7.5 has already
   paid for learning that.
3. Deal with the 365 `-Wunknown-pragmas` in the same pass. They are all MSVC
   `#pragma warning`; either wrap them in `LE_MSVC_SPECIFIC` or add
   `-Wno-unknown-pragmas` and record why.
4. `-Werror` in CI only, once (1)–(3) are at zero. Never on a developer's build,
   where a new compiler version turning up a new warning should not stop work.

---

## 4. The plugin identity ✅ *done*

SpectrumWorx should present as a Surge Synth Team product: `org.surge-synth-team`
for the CLAP id and the bundle identifiers, `SSTx` / "Surge Synth Team" for the
AUv2 manufacturer, and an explicit VST3 factory extension rather than a hash.

> **Done, 01.08.2026.** The `.clap`, `.vst3` and `.component` bundles all carry
> the new identity, the AU is `aufx`/`SpWx` by "Surge Synth Team" (`SSTx`), and
> `sw-tests` is **112/112** in both build trees — the four new cases being
> `tests/clap/identityTests.cpp`, which pins every one of these strings.
>
> One thing is deliberately **not** done: the standalone's macOS
> `CFBundleIdentifier`, which is clap-wrapper's to fix rather than ours to work
> around. See "Three things worth knowing".

### What changed

| | Was | Is |
|---|---|---|
| CLAP id | `com.littleendian.spectrumworx` | `org.surge-synth-team.spectrumworx` |
| CLAP vendor | `Little Endian Ltd` | `Surge Synth Team` |
| CLAP url | `https://github.com/baconpaul/SpectrumWorx` | `https://surge-synth-team.org` |
| `.clap` / `.vst3` / `.component` id | `com.littleendian.spectrumworx.<format>` | `org.surge-synth-team.spectrumworx.<format>` |
| Standalone bundle id | `SpectrumWorx.standalone` | unchanged — **still clap-wrapper's, see below** |
| AU type | `aufx`, derived from the CLAP features | unchanged |
| AU subtype | `SpWx` | unchanged — and free: scxt uses `ScXT`, surge-xt2 `sxt2` |
| AU manufacturer | `LiEn` / `Little Endian Ltd` | `SSTx` / `Surge Synth Team` |
| VST3 factory | not declared | declared, at both factory versions |
| VST3 class id | `sha1_guid_from_name(clap_id)` | the same hash, of the **new** id — see below |

`SSTx` / "Surge Synth Team" is exactly what
`scxt-2/src/scxt-plugin/clap/scxt-clap-entry-impl.cpp:94-96` and
`surge-xt2/src/clap/surge-xt2-clap-entry-impl.cpp:135-138` use, so the AU
aggregates with the rest of the family in Logic's browser. The subtype is
per-product and `SpWx` is SpectrumWorx's from 2010.

### What it took

Three edits and one new file, plus a test that did not exist:

1. **`CMakeLists.txt`** — `SW_CLAP_ID`, and two new variables `SW_VENDOR` and
   `SW_VENDOR_URL` beside it, so the identity is in one place. They reach the
   C++ as compile definitions on `sw-impl` the way `PRODUCT_NAME` already did.
2. **`spectrumWorxCLAP.cpp`** — the descriptor now names those macros instead of
   repeating their contents. The CLAP id was written out twice before, in CMake
   and in C++, which is how a pair like that drifts.
3. **`swClapEntryImpl.cpp`** — `SSTx`/`SW_VENDOR` for the AUv2 factory, with the
   two four-character codes as named constants and a `static_assert` on each
   (`au_subt` is `char[5]` and the wrapper reads all four, so a three-character
   code would ship whatever `strncpy` padded with). Plus the VST3 factory,
   answering **both** `CLAP_PLUGIN_FACTORY_INFO_VST3` and `…_V1` with the same
   struct, as `vst3.h` asks.
4. **`tests/clap/identityTests.cpp`** (new, 4 cases) — pins the CLAP id, vendor
   and url; the AU manufacturer, name and subtype, and that an empty `au_type`
   asks the wrapper to derive it; that the VST3 factory is the same pointer at
   both versions; that an unknown factory id is declined; and that
   `create_plugin` **refuses the old id**. The existing descriptor test checks
   `strlen(id) > 0`, which is the right check for "the factory is wired up" and
   no check at all for this.

### Three things worth knowing

**The standalone's bundle identifier is still `SpectrumWorx.standalone`, and
that is deliberate.** clap-wrapper hardcodes it in its own `Info.plist.in` as
`${MACOSX_BUNDLE_BUNDLE_NAME}.standalone` — a bundle *name* with a suffix rather
than a reverse DNS identifier — and no CMake property overrides that one key.
The only local remedy is to carry a whole copy of their template to change one
line, which is a fork of a third-party file and a maintenance burden every time
theirs gains a key. **Fix it upstream instead**; it matters at notarisation and
nowhere before that, and stage 1 has had it recorded since the walking skeleton.
`src/clap-first/CMakeLists.txt` carries the note so the next person to look does
not re-derive it.

**The VST3 class id is not pinned, deliberately.** clap-wrapper hashes the CLAP
id into it unless the plugin hands over a `componentId`
(`wrapasvst3_entry.cpp:269-276`). Pinning one *now* would only freeze today's
hash of the new id and gain nothing, since no VST3 of this plugin has ever
shipped. The moment one is released, that changes: run the validator, read the
`cid` back and hand it over with `COMPONENT_ID`. `vst3Info()` says so where
whoever does it will be standing.

**The AU's `Info.plist` does not regenerate on an incremental build.**
clap-wrapper generates it by running a build-helper against the built CLAP as a
`POST_BUILD` step on *that helper's* target, so a change to nothing but the
factory strings leaves the helper up to date and the plist stale — it kept
saying `LiEn` through two rebuilds. Under Ninja, deleting
`…_auv2-build-helper-output/auv2_Info.plist` is enough (it is a declared
`BYPRODUCTS`); under the Makefile generator it is not, and the helper binary
itself has to go. A from-scratch build is always right. Worth remembering the
next time an AU behaves like an older version of itself.

### Why now, and what it breaks

### Why now, and what it breaks

**The VST3 class id is `sha1_guid_from_name(clap_id)`** unless the plugin
declares `componentId` through the factory extension
(`wrapasvst3_entry.cpp:269-276`). So the CLAP id is load bearing for VST3
project compatibility: change it after a release and every saved session loses
its plugin. **Nothing has shipped**, so today it costs four string edits and
zero compatibility.

Two consequences to expect and verify in item 1 of §1:

- The AU changes manufacturer, so macOS registers it as a new component. Run
  `auval -v aufx SpWx SSTx` once it is installed, and clear the AU cache if
  Logic gets confused.
- Any DAW that has already seen the old CLAP id will treat the new one as a
  different plugin. That is correct and desirable; do not read it as a bug.
  **`~/Library/Audio/Plug-Ins/CLAP/SpectrumWorx.clap` on this machine is still
  the 28 July build and still answers to `com.littleendian.spectrumworx`** —
  replace it before the DAW pass, or a host will offer both.

**What does *not* change:** the product name (`SpectrumWorx`), the user data
directory (`sst::plugininfra::paths::bestDocumentsFolderPathFor("SpectrumWorx")`
in `gui.cpp`, keyed on the product not the vendor), and the copyright headers.
"Copyright (c) 2010 - 2016. Little Endian Ltd." is attribution and stays; the
identity being changed is the *publisher's*, not the author's.

---

## 5. Where to focus

Beyond the ordered list, six things worth deciding rather than drifting into.

**0. Capture the deadlocks before touching the threading.** They reproduce in
Logic and in Bitwig today; the moment the model changes they stop being
collectable, and a stack from each side of a held lock is worth more to the
redesign than the whole of §2.2. Which host, which situation, both backtraces.
This is the only item here with an expiry date.

**1. Thicken the test host before doing anything else to the host layer.**
§2.1a is a null dereference that the whole green suite cannot see, because the
test host offers one extension. Give `RecordingHost` `clap.state` and
`clap.thread-check`, plus a variant that deliberately withholds thread-check, and
record output events and gesture pairs. Almost every item in §1 items 3 and 4 is
easier to verify afterwards, and this is half a day.

**2. Make the GUI tests assert something.** They currently check an exit code.
Uniform-colour detection plus a dimension check is ten lines in `renderPage()`,
and a `foreach` over the effect list turns one module UI into 57. That combination
is the cheapest large increase in coverage available anywhere in this tree, and
the GUI is where week one's bugs actually were.

**2a. Feed the side chain a different signal.** The same argument, for the DSP:
§2.8 has the four-step recipe, it is well under a day, and it converts seven
effects' goldens from "cannot distinguish a working side chain from one that is
ignored" into real pins. Both harnesses currently pass the main input twice.

**3. CLAP preset discovery is a natural fit and nobody has mentioned it.** The
303 factory banks are already in the binary behind a clean API
(`FactoryPresets::{banks,presets,load}`). `clap_preset_discovery_factory` would
let a host browse them natively — one flat namespace, no files, no installer
step. surge-xt2 has the hook stubbed out
(`surge-xt2-clap-entry-impl.cpp:148-154`) as the reference. This is the one
place where being a 2011 plugin with a big preset library is an *advantage*, and
it is a day's work on top of what 8.2 built.

**4. Decide the sample question with 5.0, not before it.** 1.4 MB of
`assets/samples` is currently neither embedded nor installed, and the loader that
reads it is compiled out. Those two facts are one decision, and splitting them is
how a build ends up shipping content nothing can open.

**5. Settle the licence early, because it is a decision and not a task.**
`doc/manual/EULA.txt` contradicts `LICENSE`; JUCE 8 is AGPLv3-or-commercial; 452
file headers currently say `GPL-3.0-or-later`. If the answer is AGPL it is one
`sed`. If the answer needs a conversation, that conversation should start now
rather than in the week of a release.

And one that is not a task at all: **write down what the first five minutes of
using SpectrumWorx should be**, and check the plugin against it by hand. Week
one's four bugs were all found that way and none of them by a test. Loading a
preset, putting an effect in a slot, turning a knob, saving, closing the editor,
reopening it, saving the session, reloading it. Note that
`doc/manual/SpectrumWorx test procedure.doc` is Little Endian's own version of
exactly this list, sitting unread in the tree since stage 0.5 moved it — worth
converting before writing a new one. That list, run in Reaper, is
worth more right now than any amount of headless coverage — and once it is
written down it is also the acceptance test for items 3, 4 and 5.
