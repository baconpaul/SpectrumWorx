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

- ~~**clap-wrapper's AUv2 "silent" input is uninitialised heap.**~~
  *Fixed upstream and merged, 03.08.2026 — clap-wrapper #498, "Create silence
  buffers as empty buffers"; the submodule is bumped to it.* Kept because the
  shape of it is worth remembering. `ProcessAdapter::setupProcessing` allocated
  `_silent_input` with `new float[numMaxSamples]` and never zeroed it, and
  `process()` points every channel of any bus whose `PullInput` failed at that
  buffer. SpectrumWorx declares two input busses and `auval` connects only the
  first, so **the side chain was fed uninitialised memory every block** —
  measured: 5 of 5 `auval` runs aborted on a NaN in `rectangular2polar`, 10 of 10
  pass now. The AUv3 path in the same repository already memset its equivalent,
  so it was an omission on one path rather than a design.

  Two things it cost, both worth more than the fix: the failure was **~50 %
  intermittent**, so a single green run "proving" it was worthless, and the
  garbage was **huge but finite** rather than NaN — see the entry under "Host
  interface" — so it walked through every finiteness check in the engine before
  becoming a NaN three layers away.

- **`RequiredStringStorage<T>` is the contract for every `lexical_cast` buffer
  and nothing had ever checked it.** (02.08.2026, §3 `-Wdeprecated-declarations`)
  The three `lexical_cast(value, char *)` overloads take a bare pointer, so when
  their `sprintf`s became `snprintf`s the only bound available was the size the
  interface tells callers to allocate — `2 + digits * 3010 / 10000`. Two values
  of it look short on paper:

  | | constant | worst case |
  |---|---:|---|
  | `std::int32_t` | 11 | `-2147483648` is 11 characters **plus** the null |
  | `double`, `%.9f` | 17 | `1000000.000000000` is 17 characters plus the null |

  Neither is reachable from anything the plugin prints — parameter values, LFO
  periods, module indices — and the 192 cases and the 303 factory presets all
  pass with the new `LE_ASSERT` on the return live. So this is not a bug report:
  it is that the guard is an assert in a debug build, which is the *first* thing
  that has ever checked the constant, and the release build still truncates
  rather than overruns. Either widen the constant by one for the sign and give
  the `double` arm a real bound, or give the overloads a size parameter and
  delete the constant. There are 13 call sites and they all have a `std::array`
  or a `_countof` in hand already.

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

- **Rendering real spectra trips the negative-amplitude verification.**
  (02.08.2026, from `presetRenderTests.cpp`) `goldenTests.cpp` already records
  this for one effect — a running sum across thousands of bins drifts a hair
  below zero and the next module reads it as an amplitude — and playing the
  factory banks shows it is not one effect: **at least eight of the 303 presets**
  abort a checked build on it, and the iteration was stopped rather than
  finished. Benign in the output, which is why both files are release-build
  artifacts and why the release run renders all 303 finite. It is a weakness in
  the vector primitives, and a skip list would need a dozen names and would grow.

- ~~**Browsing the factory banks puts a dialog in front of the user on one preset
  in three.**~~ *Closed 02.08.2026.* All 104 were `MissingParameter` and nothing
  else — an effect that grew a parameter after the preset was written, the value
  defaulting, which is what the format is for. A user can neither act on that nor
  avoid it, so it no longer reaches them: see
  `PresetLoadReport::worthTellingTheUser()`. It is still counted and still traced,
  and `presetReportTests.cpp` pins the total at 722 so that a parameter going
  missing for a *bad* reason — a rename, a changed streaming name — reddens
  rather than hiding behind the suppression.

- **The Exaggerator's behaviour next to an empty bin is a cliff.**
  (02.08.2026, from `presetRenderTests.cpp`) Its intensity maps to an exponent
  over [-1, 4] and it raises every normalised bin to it, so with a negative
  intensity the gain applied to a bin grows without bound as that bin approaches
  zero. The NaN this produced is fixed — `pow( 0, negative )` is `+inf`, one
  infinity zeroed the normaliser and the whole spectrum followed, in four shipped
  presets — but the fix is "an empty bin stays empty", which is a discontinuity
  rather than a rounding of one: a bin at 1e-30 is still boosted enormously and a
  bin at exactly zero is not boosted at all. A floor on the input would be the
  honest shape, and choosing one is a DSP decision with an audible answer.
  The unexplained `/ 2` in its normaliser is worth the same look.

- **An LFO's default sync type depends on whether a host has reported a tempo.**
  (02.08.2026, from making the N/T/D buttons work in the standalone)
  `LFOImpl::SyncTypes::default_()` is `hasTempoInformation() ? Quarter : Free`,
  and it is the last reader of that flag now that the interface has stopped
  asking. A parameter's *default* is supposed to be a property of the parameter:
  this one is a property of the host's transport at the moment somebody asks, on
  a process-global flag that `Timer::reset()` deliberately never clears. So the
  same preset, loaded into the same build, can get a different sync type
  depending on what happened earlier in the process — which is the shape of the
  order-dependent `[preset-corpus]` digests recorded below, and plausibly a
  direct cause rather than a coincidence.

  Making it a constant `Quarter` is the obvious fix and is deliberately not done
  here: it would move `parameterTable.txt`, and possibly `presetCorpus.txt`, and
  a committed digest moving is a decision rather than a chore. Worth doing with
  the fixtures regenerated in the same commit and the reason written on it.

- **The LFO panel does not follow the host's tempo.**
  (02.08.2026, from `correct_the_threading.md` §6.8)
  `SpectrumWorxEditor::updateForNewTimingInfo()` is correct and unreachable: its
  one caller was `SpectrumWorx::updatePosition()` in the 2016 host class, which
  stage 8 deleted. The CLAP's equivalent is `updateLFOTiming()`, on the audio
  thread, and reaching a widget from there is what this whole redesign forbids —
  so the answer is a `ToUI` message, and the function is where it lands. Visible
  as an LFO panel showing the old period after a tempo change.

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

- ~~**Restoring a session puts up one modal dialog per parameter the file does
  not mention.**~~ *Closed 02.08.2026.* The default reporter counts rather than
  alerting, `stateLoad` says nothing at all, and a parameter the file does not
  mention is no longer the user's business in the first place — see
  `PresetLoadReport::worthTellingTheUser()` and `presetReportTests.cpp`. The
  figure this entry quoted, 806, was never measured: the counted total across the
  303 banks is **722**, pinned as a fixture.

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

- **An unconnected side-chain port is indistinguishable from a connected one.**
  (03.08.2026, from item 1) `runEngine` falls back to the main input only when
  `audio_inputs[1].data32` is *null* (`spectrumWorxCLAP.cpp:876`), and no real
  host gives us null — every wrapper hands over a buffer it owns. So what an
  unpatched side chain contains is whatever the host put there, and the plugin
  cannot tell "silence" from "not connected" from "never written". This is how
  the auval NaN below reached the engine, and the fallback §2.8 documents as the
  intended behaviour — an unpatched side chain being the main input — is
  therefore effectively dead code. CLAP's own mechanism for this is
  `clap_audio_buffer::constant_mask`, which nothing here reads.

- **The engine's guards are finiteness guards, and garbage is usually finite.**
  (03.08.2026, from item 1) Every `LE_MATH_VERIFY_VALUES` on the input path
  tests `Invalid` (NaN/infinity) or denormals. Uninitialised memory read as
  float is overwhelmingly *huge and finite* — the measured value was 2.9e33 —
  so it passes `time2DFT`'s checks on the time domain, on the window and on both
  FFT outputs, and only becomes NaN when squared inside `vDSP_zvabs`. The assert
  that fires is therefore three layers away from where the bad data entered,
  which is why this cost a day. A magnitude bound on the incoming block would
  have named it immediately; whether the engine should carry one in a release
  build is a real question and not obviously yes.

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

- **`LFO::Timer`'s tempo is three process-global statics, and a test binary is
  where that stops being benign.** (01.08.2026, item 4; symptom closed
  03.08.2026) It read: `[preset-corpus]` fails about one run in three when
  `sw-tests` is run bare, and `ctest` cannot see it. 153 of the 303 rows move —
  the ones with a tempo-synced LFO. The cause is process-global state:
  `LFOImpl::Timer` keeps `barDuration_`, `measureNumerator_` and
  `hasTempoInformation_` as **statics**, and `Timer::reset()` deliberately does
  not clear the last of them — a 2012 workaround for Ableton Live popping up
  "preset uses tempo-synced LFOs but the host provides no tempo" while browsing
  (`lfoImpl.cpp:766-784`). That dialog is gone as of 02.08.2026 — a host with no
  transport gets 120 BPM 4/4, which is an answer rather than a fault — but the
  workaround it produced is still what makes `hasTempoInformation_` sticky, and
  the statics are still shared. So once `pluginTests.cpp`'s `[clap][lfo]`
  transport cases have told the plugin a tempo, every later preset load in that
  process converts `PeriodScale` differently, and the corpus digests move.

  ~~Reproduce it deterministically with
  `./sw-tests --order decl "[lfo],[preset-corpus]"` — 153 failures, every time.~~
  It hid because `catch_discover_tests` gives each case its own process, so
  `ctest` was always green; only running the binary directly, which is the
  quicker thing to do while working, exposed it.

  > **The symptom is gone as of 02.08.2026, by accident, and the cause is not.**
  > The threading redesign's stage 7 split `sw-tests` into `sw-dsp-tests` and
  > `sw-plugin-tests` so that the engine's cases could link without JUCE.
  > `[preset-corpus]` went to the first and `pluginTests.cpp`'s `[clap][lfo]`
  > transport cases to the second, so the two are no longer in one process and
  > neither binary can pollute the other. Measured 03.08.2026: `./sw-dsp-tests
  > --order decl` is 108 passed / 3 skipped and `./sw-plugin-tests --order decl`
  > is 72 passed, both bare, both green.
  >
  >   Nothing was fixed. `LFOImpl::Timer`'s three statics are still statics and
  > `hasTempoInformation_` is still sticky; what changed is that no test now runs
  > on the far side of them. A case added to `sw-dsp-tests` that establishes a
  > tempo would bring the whole thing straight back, with no warning, which is
  > why this entry stays.
  >                                                   (03.08.2026.)

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

- ✅ ~~**The GUI render tests assert an exit code.**~~ (01.08.2026, from §2.3)
  `renderPage()` writes a PNG and returns 0; a page that paints solid black
  passes. Blank-and-uniform-colour detection is about ten lines, and it is what
  would have caught the empty settings panel that 6.4 found by looking at an
  image. Nine tests currently asserting almost nothing.
  *Closed 03.08.2026. `renderPage()` measures what fraction of the canvas is not
  its commonest colour and fails under a thousandth of it; the pages measure
  12–80 %, so the floor is a long way below every one of them and the failure it
  is for is "nothing was drawn". `tests/gui/overlayPanelTests.cpp` is the other
  half — the whole-canvas number cannot see a 191 × 363 hole in a 563 × 376
  editor, so the overlay rectangle is measured on its own.*

- ✅ ~~**One effect of 57 and one bank of 18 are ever drawn.**~~ (01.08.2026, from
  §2.3) `SW_SHOW_UI_EFFECT`, `SW_SHOW_UI_PRESET` and `SW_SHOW_UI_PRESET_SWEEP`
  exist and are manual-only. A CMake `foreach` over the effect list is four lines
  for 57× the GUI breadth.
  *Half closed, 03.08.2026: 57 `show-ui-renders-module-<Effect>` cases, with the
  list read out of `effectsList.hpp` rather than copied into CMake. The banks are
  still one of eighteen. What the sweep found on its first run is the next
  bullet.*

- **A slot filled from the editor needs its rack resynced by hand, and two
  harnesses do it and one did not.** (03.08.2026, from §5.2)
  `addUserAddedModule` ends in `refreshModuleRackAsync()`, so a caller with no
  message loop has to call `resyncModuleRack()` itself; `pluginTests.cpp` did and
  `tools/show-ui`'s editor-module page did not. For a month that page rendered an
  editor with a highlighted empty slot and no module in it — the page whose whole
  purpose is proving a module's widgets can be built. It was invisible until the
  sweep above made all 57 effects produce *byte-identical* PNGs. Fixed there, but
  the shape of it is not: "build the thing, then pump the async step by hand" is
  an unwritten rule that three harnesses now follow separately, and the next one
  will not know either.

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

- ✅ ~~**A host that provides `clap.thread-check` and answers has never been
  tested.**~~ (01.08.2026, item 0) `StatefulHost` deliberately omits it, which is
  what reaches the deferral. The other arm of every `canUseThreadCheck()` branch
  — the one where the plugin is told it *is* on the main thread and acts
  immediately — has no coverage at all.
  *Closed 03.08.2026 by `tests/clap/testHost.hpp`: one configurable `TestHost`
  replaces the three hand-rolled ones, answers the thread check out of its own
  bookkeeping, and `hostInteropTests.cpp` drives both arms. What it left behind
  is the next two bullets.*

- **clap-helpers' flush validation calls its own `[main-thread]` entry point
  from the audio thread.** (03.08.2026, from §5.1) `clapParamsFlush` guards
  itself correctly with `ensureFlushThread` — `active ? audio : main`, matching
  ext/params.h:303 — and then validates each event through
  `checkValidFlushEvent` → `getParamInfoForParamId`, which opens with
  `checkMainThread()` and then calls `clapParamsInfo`, whose C entry point opens
  with `ensureMainThread`. So a correct host flushing a correct plugin is
  reported for misbehaving, twice per event, at `CheckingLevel::Maximal`.
  Upstream's, and it is only visible to a host that answers `clap.thread-check`,
  which is why nothing had seen it. `TestHost` filters the two `clap.log`
  messages by exact wording; the third — `getParamInfoForParamId`'s own
  `checkMainThread()` — goes straight to `std::cerr` and cannot be intercepted at
  all, so it is one line of noise per flush on the test output. Worth a
  clap-helpers issue; worth nothing local.

- **`ActivePlugin` is the only harness that flushes on the thread CLAP says to.**
  (03.08.2026, from §5.1) `params.flush()` is `[active ? audio-thread :
  main-thread]`, and every `params.flush(&*plugin, …)` call site in
  `pluginTests.cpp` — several dozen — makes it from the main thread against an
  active plugin. Harmless there, because those cases use hosts that offer no
  thread check and so nothing asks; but it means the flush path has only ever
  been driven from the wrong thread, and `paramsFlush()` calls `drainCommands()`
  and `handleEvent()`, which are the two things `process()` does to the engine.
  Converting them to `plugin.flush(…)` is mechanical and would put the whole
  parameter-event path on the audio thread under tsan and rtsan, where it belongs.

- **`SpectrumWorxCLAP::paramsFlush` opens no `Threading::ScopedAudioCallback`.**
  (03.08.2026, from §5.1) Against an active plugin it *is* an audio-thread
  callback by contract, and it mutates the engine — so `Threading::isAudioThread()`
  answering `false` inside it is the plugin's own account of who owns the engine
  being wrong on one path. Not a live bug, because nothing asserts on it today;
  it is the sort of thing `engineOwnershipTests.cpp` exists to pin and does not.
  Adding the scope also brings the path under rtsan, which would immediately
  report the slot-selector allocation already recorded above — so it is a
  deliberate pair of changes rather than a one-liner.

## Licence and shipping

- ✅ ~~**`doc/manual/EULA.txt` is a commercial end-user agreement and the
  repository is GPL-3.0.**~~ (01.08.2026, from item 10)
  *Wrong, and checked on 03.08.2026: the file in the tree is a plain-text copy of
  the GPL-3.0 licence — 218 lines, the same terms as `LICENSE`, no proprietary
  wording in it. The commercial agreement was replaced before the port began and
  three documents went on describing the file by its 2016 name. The decision
  itself is made and written down in [`LICENSING.md`](../../LICENSING.md): source
  GPL-3.0-or-later, released binary AGPL-3.0-or-later because JUCE 8 is
  AGPLv3-or-commercial, and the 452 file headers are right as they stand.*

- **The duplicate licence file and the installer path that names it.**
  (03.08.2026, from item 10) `doc/manual/EULA.txt` is now a byte-for-byte
  duplicate of `LICENSE` under a filename that means the opposite of what it
  contains, and `src/legacy-build.cmake:386` still points
  `CPACK_RESOURCE_FILE_LICENSE` at `../installer/ProgramFolder/Licences/EULA.txt`
  — a path outside this repository. Neither is reachable from a live target, so
  neither is urgent; both are item 10's to clear when there is a real installer,
  and the AGPL statement is what it has to show.

- **The standalone's `CFBundleIdentifier` is clap-wrapper's
  `SpectrumWorx.standalone`.** (01.08.2026, from §4) Notarisation is the step
  that cares. The fix belongs upstream — a `BUNDLE_IDENTIFIER` the standalone
  wrapper honours the way the plugin wrappers already do — so it wants a
  clap-wrapper PR rather than a local workaround, and an upstream PR is not
  something a stage can be sized around.
