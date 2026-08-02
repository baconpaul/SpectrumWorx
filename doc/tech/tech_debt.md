# SpectrumWorx — Tech debt

A running list, appended to as work happens. Companion to
[`implementation_sequence.md`](implementation_sequence.md), which is the plan,
[`week_two.md`](week_two.md), which is the re-plan, and
[`streaming_format.md`](streaming_format.md), which is what goes into a file.

**What belongs here and what does not.** Those two documents track *work*: things
someone will sit down and do, in an order, with a size next to them. This tracks
what is left behind by work that is otherwise finished — the half-fix, the
correct-but-unsatisfying answer, the finding that has no owner because it is not
big enough to be a stage and not small enough to be a `\todo`. A thing that is
squarely inside a numbered item is that item's; it does not need a bullet here.

The test for a bullet: **if the plan were executed exactly as written, would this
still be true at the end?** If yes, it belongs here.

Every entry carries the date it was written and, where there is one, the item or
section it fell out of — because half of these are true only until someone
touches the file, and a bullet with no provenance is unverifiable a month later.
New entries go at the top of their area.

---

## Build and platform

- **MP3 decoding is a different decoder on macOS than on Windows and Linux, and
  which one answers is decided by registration order.** (01.08.2026, item 7)
  `registerBasicFormats()` gives `CoreAudioFormat` on macOS,
  `WindowsMediaAudioFormat` on Windows and **nothing** on Linux, so `sw-dsp`
  defines `JUCE_USE_MP3AUDIOFORMAT=1` — JUCE's own decoder, behind a flag because
  it carries a patent disclaimer for patents that expired in 2017. Every one of
  the seventeen factory samples is an MP3, so without it a Linux build ships
  content it cannot open.

  What that flag actually did is worth reading off
  `juce_AudioFormatManager.cpp:63-87` rather than off the flag's name.
  `createReaderFor` walks `knownFormats` **in registration order** and takes the
  first that accepts the stream, and `MP3AudioFormat` is registered *before*
  `WindowsMediaAudioFormat` and *after* `CoreAudioFormat`. So:

  | | answers for MP3 |
  |---|---|
  | macOS | `CoreAudioFormat` |
  | Windows | `MP3AudioFormat` — **not** `WindowsMediaAudioFormat`, which the flag displaced |
  | Linux | `MP3AudioFormat` |

  Two decoders, then, not one and not three — and setting the flag quietly
  changed Windows from the platform decoder to JUCE's. That is probably the
  better outcome (it is the same code as Linux, so two platforms agree by
  construction) but it was not the intent and nothing says so.
  - The cheap fix is to stop shipping MP3: the samples are 1.4 MB as MP3 and the
    only reason for the format is that 2016 chose it. FLAC is registered on every
    platform, ahead of all of these, and would delete this entry.
  - The `sampleTests.cpp` cases would catch a decoder that fails outright. They
    would not catch one that is a few samples out of alignment with another
    platform's — encoder delay is exactly where MP3 decoders disagree — which is
    what would actually happen.

- **`clang-format` is clean where the port has been and not where it has not.**
  (01.08.2026) 66 of 465 files deviate; **63 of them are under `src/le/`**, the
  2016 tree, and the other three are `core/modules/factory.cpp`,
  `gui/editor/moduleMenuHolder.cpp` and `gui/modules/moduleUI.hpp`. So this is
  not "the tree is dirty" — it is a boundary between ported and unported code
  that happens to be visible to a formatter. The three outliers are worth fixing
  now; reformatting `src/le/` wholesale would put a 60-file diff between every
  future change and its history, and is a decision rather than a chore.

- **`build-asan/` exists and is configured from an older CMake.** (01.08.2026,
  from `week_two.md` §1 item 3) It registers three of the nine GUI tests, so a
  sanitiser run over it is quietly a third of the coverage a normal run has.
  Item 3 wants ASan; nothing owns the directory being stale, and a stale build
  directory is exactly the kind of thing that gets trusted.

## Threading

- ~~**A block that cannot have the processing lock is still dropped.**~~ *Closed
  02.08.2026 by `correct_the_threading.md` stage 6.* There is no lock, so there
  is no block to drop: `SpectrumWorxCore::process()` returns `void` again and
  `runEngine()`'s silence branch is gone, which is the measure of success
  `week_two.md` item 3 set for itself.

- ~~**`currentThreadOwnsTheProcessLock()` returns a hardcoded `true`.**~~ *Closed
  02.08.2026.* Stage 0 made it real; stage 6 deleted the lock it was about. The
  six assertion sites now read `currentThreadMayMutateEngineState()`, which is
  "the audio thread owns the engine while one exists, and the main thread owns it
  while one does not". The invalid `reinterpret_cast` to a `CRITICAL_SECTION`
  went with it, so the Windows half is closed too.

- **A host writing a slot selector allocates on the audio thread.** (02.08.2026,
  from `correct_the_threading.md` §8) What is left of the concession stage 6 was
  granted. Every other route — the interface, a preset, a session — builds its
  modules on the main thread and hands the engine a pointer to link. A host's
  parameter event arrives inside `process()`, and deferring it means a round trip
  to the main thread and back before the slot changes, which is a latency a
  generic panel would notice.

- **`LFOImpl::Timer`'s tempo is one value for every instance in the process.**
  (02.08.2026, from §8) `std::atomic` since stage 6, so it is no longer a data
  race — but two tracks at two tempi still see one tempo. Making it per-instance
  means threading a timer through `snapPeriodScale()`, `clampFreePeriod()` and
  the two period-scale bounds, all of them static and all called from the
  parameter layer and the editor; that is the LFO parameter interface's redesign
  rather than the threading model's.

- **`UIEdits` drops on full, and that is wrong for gestures.** (01.08.2026, from
  §2.2) The ring is otherwise correct. Dropping a `Kind::Value` is right — the
  next one supersedes it. Dropping a `GestureBegin` whose `GestureEnd` survives
  leaves the host holding an unbalanced gesture, which some hosts never recover
  from. §2.2 calls it "worth a follow-up, not a blocker" and no item took it.

## Host interface

- **Restoring a session puts up one modal dialog per parameter the file does not
  mention.** (02.08.2026, item 4) The default `PresetProblemReporter` is
  `GUI::warningMessageBox`, and `MissingParameter` is raised once per parameter
  an effect grew after the file was written — 806 times across the 303 factory
  banks. That was already true of opening a preset; it is now also true of a host
  restoring a project, which happens without anyone asking for it and possibly
  before there is a window to put a dialog in front of. The machinery to fix it
  exists and is used by the tests: `setPresetProblemReporter` takes a function
  pointer, and `SWTest::ScopedProblemCounter` counts instead of alerting. What is
  missing is the shipping plugin's own choice of reporter — a status line, a
  once-per-load summary, anything that is not a dialog per parameter.

- ~~**The CLAP state does not hold which external audio file is loaded.**~~
  *Fixed 02.08.2026, item 4.* State is the preset serialisation now, and
  `<p n="Sample">` has been in that since 2011, so `setNewSample()` marks the
  session dirty and a reopened project has its sample.

- **`paramsTextToValue` is `return false` and `paramsValueToText` ignores the
  value it is given.** (01.08.2026, from §2.3) The second prints the parameter's
  *current* value whatever it was asked about, which is visible in every
  automation-lane tooltip in every host. Both are documented at length in the
  source with a shared `\todo`, and both are one job: give
  `AutomatedParameterPrinter` an arm that takes a value *and* the live parameter.
  No stage owns it. It is the most user-visible thing on this page.

- **Host automation of the six global parameters does not move the editor's
  knobs.** (01.08.2026, from §2.2) `updateGlobalParameterWidget<>` and
  `updateForGlobalParameterChange()` have no callers — their only caller was the
  deleted 2016 plugin class. A live UX bug with no owner, and one where the
  naive fix (call them from the parameter path) recreates a documented
  audio-thread violation.

## DSP and effects

- **A phase-vocoder pitch shift's accuracy depends on the FFT size, and not
  monotonically.** (01.08.2026, item 8) Measured, with Pitch Magnet asked to
  move a 220 Hz partial to 880 Hz and the output's dominant frequency read back:

  | FFT size | lands at | error |
  |---|---|---|
  | 1024 | 707.9 Hz | **−377 cents** (the target is the *second* loudest thing present) |
  | 2048 | 880.1 Hz | +0.2 cents |
  | 4096 | 922.8 Hz | **+82 cents** |

  110 Hz and 330 Hz targets are within 0.2 cents at both 2048 and 4096, so it is
  the large upward shift that degrades. "More bins are better" is not the shape
  of this and nobody has looked at why. `exImploderImpl.cpp` carries a 2012
  `\todo` from Domagoj Saric saying the pitch shift there "is not the correct way
  to do it (although Dobson does it that way)", which may or may not be the same
  finding. The property test pins 2048 and says so; that is a test choosing a
  setting where the measurement is unambiguous, not a fix.

- **Three effect parameters have ranges most of which do nothing useful.**
  (01.08.2026, item 8) All three were found by writing property tests and all
  three read as bugs to a user:
  - **Slew Limiter's rise starts from `FLT_EPSILON`**, which the implementation
    floors the previous amplitude to so that a bin can leave silence at all.
    That is 138 dB below unity, so a rise-limited bin has to climb 138 dB before
    it is audible: **at 3 dB/s that is 46 seconds**, and anything below about
    28 dB/s — the bottom tenth of the 0–300 dB/s range — takes more than five.
    That part of the knob is a mute with extra steps.
  - **The Exploder's "Limit" does not limit.** Reaching it *resets* the
    accumulator to whatever the input is doing, so the level is a sawtooth
    rather than a ramp to a ceiling. Defensible as an effect; the parameter is
    named for the other behaviour.
  - **The Octaver's cutoff defaults to 350 Hz**, and it is a low pass over the
    effect's *output*. So an Octaver dropped into a slot removes most of what it
    just added: the up-octave of anything above F3 (175 Hz) is cut, which is
    most of what anyone plays. This is the one that most reads as "the effect is
    broken".

  None of these is a regression — all three are 2016 behaviour, now pinned by
  tests. Changing any of them changes what a 2011 preset sounds like, which is
  why none of them is in a plan.

- **The goldens skip in a checked build because `Smoother` asserts.**
  (01.08.2026, from `goldenTests.cpp:287`) `Math::symmetricMovingAverage` carries
  a running sum across thousands of bins and over pink noise the accumulated
  rounding drifts a hair below zero, so `Smoother` hands `amph2DFT()` a negative
  "amplitude". Benign in the output, real as a numerical weakness. The
  consequence is structural: **Release is the only configuration that renders
  DSP**, so a debug-only regression in the engine has nothing to catch it. Item
  8's property tests are the first DSP assertions a checked build makes, and
  they cover nine effects of 57.

- **Fourteen side-chain effects are golden-pinned only where side == main.**
  (01.08.2026, from §2.8) `engineHarness.cpp` passes `inputPointers.data()` as
  both main and side — the one case in which a side-chain effect cannot be told
  apart from a bug that ignores the side chain entirely. §2.8 has a five-step
  recipe and no item owns it. Related and stranger: `convolver.hpp` declares
  `usesSideChannel = false` while `convolverImpl.hpp` takes
  `MainSideChannelData_AmPh`, and the reason nobody has noticed is that
  **`usesSideChannel` has no reader anywhere in `src/` or `tests/`** — 57 files
  maintaining metadata that nothing consumes.

- **An unconnected side-chain port is fed the main input, not silence.**
  (01.08.2026, from §2.8) So a Blender with nothing patched blends the signal
  with itself. Defensible, deliberate, and documented nowhere a user would look.

## Tests

- **`[preset-corpus]` fails about one run in three when `sw-tests` is run bare,
  and `ctest` cannot see it.** (01.08.2026, item 4) 153 of the 303 rows move —
  the ones with a tempo-synced LFO. The cause is process-global state:
  `LFOImpl::Timer` keeps `barDuration_`, `measureNumerator_` and
  `hasTempoInformation_` as **statics**, and `Timer::reset()` deliberately does
  not clear the last of them — a 2012 workaround for Ableton Live popping up
  "preset uses tempo-synced LFOs but the host provides no tempo" while browsing
  (`lfoImpl.cpp:766-784`). So once `pluginTests.cpp`'s three `[clap][lfo]`
  transport cases have told the plugin a tempo, every later preset load in that
  process converts `PeriodScale` differently, and the corpus digests move.

  Reproduce it deterministically with
  `./sw-tests --order decl "[lfo],[preset-corpus]"` — 153 failures, every time.
  It hides because `catch_discover_tests` gives each case its own process, so
  `ctest` is always green; only running the binary directly, which is the
  quicker thing to do while working, exposes it.

  Two things are wrong and only one of them is the test's. A plugin whose
  tempo-to-period conversion depends on whether *any* instance in the process
  ever saw a transport is a design smell independent of tests — it is benign in a
  DAW, where there is one tempo, and it is not benign in a test binary or in an
  offline renderer. The 2012 note explains why the flag is sticky but not why the
  state is global. Fixing it properly means the tempo living on the engine;
  fixing it cheaply means a way for the corpus test to establish a known tempo
  state, which needs a reset the class does not expose.

- **`LFOImpl::Timer::setPosition( float )` asserts two things that are both
  false, and is dead.** (01.08.2026, item 4) `lfoImpl.cpp:753-755` reads
  `LE_ASSUME( barDuration_ == 4 )` and
  `LE_ASSUME( measureNumerator_ == 60.0f / 120 * 4 )` — the two values swapped
  between them. The initialisers three hundred lines up are
  `barDuration_( 60.0f / 120 * 4 )`, which is 2, and `measureNumerator_( 4 )`, so
  both assumptions are false as written. Nothing has noticed because nothing
  calls it: its only caller is `Engine::Processor::setPosition`, and that has no
  callers at all in the CLAP path. Harmless while dead; `LE_ASSUME` is
  `__builtin_assume` in a shipping build, so reviving the caller without fixing
  the pair would hand the optimiser two false facts about live values.

- **The GUI render tests assert an exit code.** (01.08.2026, from §2.3)
  `renderPage()` writes a PNG and returns 0; a page that paints solid black
  passes. Blank-and-uniform-colour detection is about ten lines, and it is what
  would have caught the empty settings panel that 6.4 found by looking at an
  image. Nine tests currently asserting almost nothing.

- **One effect of 57 and one bank of 18 are ever drawn.** (01.08.2026, from
  §2.3) `SW_SHOW_UI_EFFECT`, `SW_SHOW_UI_PRESET` and `SW_SHOW_UI_PRESET_SWEEP`
  exist and are manual-only. A CMake `foreach` over the effect list is four lines
  for 57× the GUI breadth.

- **`ctest -LE slow` skips nothing.** (01.08.2026, from §2.3) No test in the repo
  sets `LABELS`; the one labelled case went with `check_gui_flag_parity.py`.
  Either re-establish the label or stop recommending the flag — several documents
  do.

- **Nothing has ever loaded a sample and then processed a block.** (01.08.2026,
  item 7) `sampleTests.cpp` proves all seventeen factory samples decode to two
  equal channels at the requested rate; nothing proves `runEngine()` then feeds
  them to the engine in place of the port. The obstacle *was* reach —
  `setNewSample` is an `EditorHost` virtual and `tests/clap/` drives the C API —
  and that is now gone: `stateTests.cpp` loads a sample into a plugin the factory
  created, through `plugin_data` and the `EditorHost` interface. What is left is
  only the block itself, which is the smaller half.

- **A host that provides `clap.thread-check` and answers has never been
  tested.** (01.08.2026, item 0) `StatefulHost` deliberately omits it, which is
  what reaches the deferral. The other arm of every `canUseThreadCheck()` branch
  — the one where the plugin is told it *is* on the main thread and acts
  immediately — has no coverage at all.

## Licence and shipping

- **`doc/manual/EULA.txt` is a commercial end-user agreement and the repository
  is GPL-3.0.** (01.08.2026, from item 10) Every file header says
  `SPDX-License-Identifier: GPL-3.0-or-later`; JUCE 8 is AGPLv3-or-commercial.
  Item 10 lists it, but it is the only entry there that is a *decision* rather
  than a task, and decisions do not get cheaper by being scheduled last.

- **The standalone's `CFBundleIdentifier` is clap-wrapper's
  `SpectrumWorx.standalone`.** (01.08.2026, from §4) Notarisation is the step
  that cares. The fix belongs upstream — a `BUNDLE_IDENTIFIER` the standalone
  wrapper honours the way the plugin wrappers already do — so it wants a
  clap-wrapper PR rather than a local workaround, and an upstream PR is not
  something a stage can be sized around.
