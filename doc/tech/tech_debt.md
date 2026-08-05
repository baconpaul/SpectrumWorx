# SpectrumWorx — Tech debt

A running list, appended to as work happens. Companion to
[`todo.md`](todo.md), which is the work queue.

**What belongs here and what does not.** `todo.md` tracks *work*: things somebody
will sit down and do, in an order, with a size next to them. This tracks what is
left behind by work that is otherwise finished — the half-fix, the
correct-but-unsatisfying answer, the finding that has no owner because it is not
big enough to be an item and not small enough to be a `\todo`. A thing that is
squarely inside a numbered item is that item's; it does not need a bullet here.

The test for a bullet: **if `todo.md` were executed exactly as written, would
this still be true at the end?** If yes, it belongs here.

**A remediated entry comes out.** Not struck through, not marked done — removed,
because the point of this file is what is still true. If the reasoning behind a
closed entry was worth keeping, it goes into whichever "how it works" document
owns the mechanism.

Every entry carries the date it was written and, where there is one, the item or
section it fell out of — because half of these are true only until someone
touches the file, and a bullet with no provenance is unverifiable a month later.
New entries go at the top of their area.

---

## Build and platform

- **Window presum is an option now, and turning it on does not work.**
  (04.08.2026, from the stage 7 macro pass) `SW_ENGINE_WINDOW_PRESUM`
  (`src/dsp.cmake`) is the surviving one of the seven macros no live build could
  define, kept because the technique is real and the engine's half of it is live:
  `channelData.cpp`'s analysis fold and `channelBuffers.cpp`'s synthesis unfold
  both run every block, with a factor of one. What the option adds is the
  plumbing that lets the factor be anything else, and a `WindowSizeFactor` global
  parameter to set it with.

  It had never been compiled. Turning it on for the first time found:

  - **A parameter whose constructor had never been through a compiler.**
    `WindowSizeFactor`'s mem-initialiser named `PowerOfTwoParameter` unqualified,
    which names nothing from `LE::SW::Engine`. Fixed, because a build that does
    not compile cannot be measured.
  - **7 cases fail, and they are the right ones.** The parameter table grows a
    row, so `parameterTable.txt`, `presetCorpus.txt` and `streamingNames.txt` all
    move. That is the decision this option really is: a global parameter added is
    every automation lane a host has saved against the old numbering.
  - **One case aborts: "Short integer overflow."** The window is
    `fftSize * windowSizeFactor` in a `std::uint16_t`, so at the maximum FFT size
    the factor cannot exceed one — and a host writing the parameter's maximum is
    exactly what `[clap][host]`'s range case does. Whoever revives this starts
    here.

- **The include-what-you-use sweep did not happen, and the obvious tool is wrong
  about this tree.** (04.08.2026, from stage 7) The mechanical half of that item
  did land — `LE_IMPL_NAMESPACE_BEGIN` is gone, and with it the 47 files that
  used a macro without declaring where it came from — but no include was removed
  on the strength of an analysis. clangd's include-cleaner cannot see through
  this codebase's macros: it reports `symmetric/parameter.hpp` as unused in
  `pitchShifter.hpp`, where removing it produces nine errors, because
  `LE_DEFINE_PARAMETER(SemiTones, SymmetricFloat, …)` names the type inside a
  macro argument. Every effect header is that shape.

  The other half of the reason is that this platform cannot answer the question
  that matters. A header macOS gets transitively and Windows does not is
  invisible from here, and Windows arrives as a build log. This wants the CI
  matrix (`todo.md`) in front of it.

- **Four GCC 15 fixes have not been compiled by a GCC.** (04.08.2026) A Linux
  build of `00383f6` reported 469 warnings from four causes, and all four are
  fixed here: `<ciso646>` deleted from the force-included header (294 `-Wcpp`),
  `valueOffsetGetter()` rewritten to take the difference between two addresses of
  a real object instead of dereferencing null (142 `-Wnonnull`, and the source
  had been calling it UB since 2016), `SpectrumWorxCore::Module` moved above its
  first unqualified use (29 `-Wchanges-meaning`), and the VST3 SDK's
  `std::wstring_convert` suppressed on `base-sdk-vst3` (4, not ours). Every one
  was verified on clang, which is a different compiler saying nothing about a
  warning it never had. The next Linux log is the check.

- **The warning baseline stops at the MSVC line.** (04.08.2026)
  `-Wall -Wextra -Wno-unused-parameter -Wno-unknown-pragmas` is on our own
  sources on every compiler that takes those spellings, and `-Werror` with them
  on Apple and wherever CI passes `-DSW_WERROR=ON`. MSVC gets nothing: `/W4` on a
  codebase nobody here can compile is a few hundred warnings delivered to
  somebody else's afternoon. It belongs with the CI matrix, where the first run
  is free.

  `-Wno-unknown-pragmas` covers 288 `#pragma warning(...)` lines — 3772 of the
  3902 warnings the baseline first produced, all of them MSVC diagnostic control
  and inert off MSVC by design. It costs the detection of a misspelled pragma,
  which is worth knowing about because the tree contained one: a
  `#pragma warning(push)` where a `pop` was meant (`assertionHandler.cpp`).
  `-Wunknown-pragmas` had not caught it and could not — both spellings are
  equally unknown to clang. It was found by reading.

- **A source that misses the force-included ODR header now builds.**
  (04.08.2026) Measured: all 148 of our translation units compile with
  `-include leConfigurationAndODRHeader.h` removed. That is new, and it is worse
  rather than better — until `LE_IMPL_NAMESPACE_BEGIN` was written out, a file
  that missed the header failed to compile, confusingly but loudly. What the
  header still decides is `NDEBUG`, which decides whether the ~1200 asserts exist
  and whether `ModuleNode` has a virtual, which decides the layout of every
  module object. So the failure mode went from a wall of errors to a silent ABI
  disagreement. `tests/checkODRHeaderScope.cmake` is the only thing standing
  there, and it only runs under the Ninja and Makefile generators.

- **`RequiredStringStorage<T>` is the contract for every `lexical_cast` buffer
  and nothing had ever checked it.** (02.08.2026, from the warnings pass)
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
  which one answers is decided by registration order.** (01.08.2026)
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

- **`build-asan/` exists and is configured from an older CMake.** (01.08.2026)
  It registers three of the nine GUI tests, so a sanitiser run over it is quietly
  a third of the coverage a normal run has. Superseded in principle by
  `SW_SANITIZER` — one cache variable rather than a pair of blessed build
  directories, `threading_model.md` §8 — but nothing owns the directory itself
  being stale, and a stale build directory is exactly the kind of thing that gets
  trusted.

## Threading

- **An LFO's Waveform and SyncTypes are still written straight into the engine
  from the message thread.** (04.08.2026, found rereading the source against
  `threading_model.md`) Every other edit crosses as a `SetBaseParameter`
  command, but these two are past `ParameterCounts::lfoExportedParameters`, so
  they have no `ParameterID` and no route through the queue:
  `SpectrumWorxEditor::…::updateParameterAndNotifyHost` (`spectrumWorxEditor.hpp:676-680`)
  branches on the index and calls `lfo().parameters().set<LFOParameter>()`
  directly. The audio thread reads that LFO every block. It is the last
  unsynchronised write from the interface into engine state, and the redesign
  recorded it as something a later stage would take — no stage did.

  Neither value is a smooth control (a waveform choice and a sync mode, both
  changed by a menu click), so nothing has been heard, and both are single
  `float`s written with a plain store. Closing it means either a `ToEngine` case
  that carries an LFO sub-parameter by index rather than by `ParameterID`, or
  exporting the two — which would move `parameterTable.txt` and is a decision
  about what a host should see rather than a threading fix.

- **A host writing a slot selector allocates on the audio thread.** (02.08.2026)
  The one exception to "modules are built on the main thread"
  (`threading_model.md` §5). Every other route — the interface, a preset, a
  session — builds its modules on the main thread and hands the engine a pointer
  to link. A host's parameter event arrives inside `process()`, and deferring it
  means a round trip to the main thread and back before the slot changes, which
  is a latency a generic panel would notice.

- **Rendering real spectra trips the negative-amplitude verification.**
  (02.08.2026, from `presetRenderTests.cpp`) `goldenTests.cpp` already records
  this for one effect — a running sum across thousands of bins drifts a hair
  below zero and the next module reads it as an amplitude — and playing the
  factory banks shows it is not one effect: **at least eight of the 303 presets**
  abort a checked build on it, and the iteration was stopped rather than
  finished. Benign in the output, which is why both files are release-build
  artifacts and why the release run renders all 303 finite. It is a weakness in
  the vector primitives, and a skip list would need a dozen names and would grow.

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

- **The LFO panel does not follow the host's tempo.** (02.08.2026)
  `SpectrumWorxEditor::updateForNewTimingInfo()` is correct and unreachable: its
  one caller was `SpectrumWorx::updatePosition()` in the 2016 host class, which
  is deleted. The CLAP's equivalent is `updateLFOTiming()`, on the audio thread,
  and reaching a widget from there is what the whole threading model forbids —
  so the answer is a `ToUI` message, and the function is where it lands. Visible
  as an LFO panel showing the old period after a tempo change.

- **`LFOImpl::Timer`'s tempo is one value for every instance in the process.**
  (02.08.2026) `std::atomic`, so it is no longer a data race — but two tracks at
  two tempi still see one tempo. Making it per-instance means threading a timer
  through `snapPeriodScale()`, `clampFreePeriod()` and the two period-scale
  bounds, all of them static and all called from the parameter layer and the
  editor; that is the LFO parameter interface's redesign rather than the
  threading model's.

  **A test binary is where this stops being benign**, and it has been live once
  already. `[preset-corpus]` used to fail about one run in three when the whole
  suite ran bare in one process — 153 of the 303 rows move, the ones with a
  tempo-synced LFO — because once a `[clap][lfo]` transport case had told the
  plugin a tempo, every later preset load in that process converted
  `PeriodScale` differently. `ctest` could never see it: `catch_discover_tests`
  gives each case its own process. What ended the symptom was the split into
  `sw-dsp-tests` and `sw-plugin-tests`, which put the two sets in different
  binaries — **nothing was fixed**. A case added to `sw-dsp-tests` that
  establishes a tempo brings the whole thing back with no warning.

  `Timer::reset()` deliberately does not clear `hasTempoInformation_` — a 2012
  workaround for Ableton Live raising "preset uses tempo-synced LFOs but the host
  provides no tempo" while browsing (`lfoImpl.cpp:766-784`). That dialog is gone
  (a host with no transport gets 120 BPM 4/4, which is an answer rather than a
  fault) but the stickiness it produced is not. The 2012 note explains why the
  flag is sticky and not why the state is global.

- **`UIEdits` drops on full, and that is wrong for gestures.** (01.08.2026) The
  ring is otherwise correct. Dropping a `Kind::Value` is right — the next one
  supersedes it. Dropping a `GestureBegin` whose `GestureEnd` survives leaves the
  host holding an unbalanced gesture, which some hosts never recover from.
  Recorded when the ring was audited as "worth a follow-up, not a blocker", and
  nothing took it.

## Host interface

- **`paramsTextToValue` is `return false` and `paramsValueToText` ignores the
  value it is given.** (01.08.2026) The second prints the parameter's *current*
  value whatever it was asked about, which is visible in every automation-lane
  tooltip in every host. Both are documented at length in the source with a
  shared `\todo`, and both are one job: give `AutomatedParameterPrinter` an arm
  that takes a value *and* the live parameter. Nothing owns it. It is the most
  user-visible thing on this page.

- **Host automation of the six global parameters does not move the editor's
  knobs.** (01.08.2026) `updateGlobalParameterWidget<>` and
  `updateForGlobalParameterChange()` have no callers — their only caller was the
  deleted 2016 plugin class. A live UX bug with no owner, and one where the
  naive fix (call them from the parameter path) recreates the audio-thread
  violation `threading_model.md` §1 exists to forbid. The answer is a `ToUI`
  message, the same as the LFO panel above.

- **An unconnected side-chain port is indistinguishable from a connected one.**
  (03.08.2026) `runEngine` falls back to the main input only when
  `audio_inputs[1].data32` is *null* (`spectrumWorxCLAP.cpp:876`), and no real
  host gives us null — every wrapper hands over a buffer it owns. So what an
  unpatched side chain contains is whatever the host put there, and the plugin
  cannot tell "silence" from "not connected" from "never written".

  Two consequences. The documented intended behaviour — **an unpatched side chain
  is the main input, so a Blender with nothing patched blends the signal with
  itself** — is therefore effectively dead code, undocumented anywhere a user
  would look and unreachable anywhere it would matter. And it is how uninitialised
  memory reached the FFT from an AU host: clap-wrapper handed every channel of an
  unconnected AUv2 bus a buffer it had allocated and never zeroed, `auval` aborted
  5 runs of 5 on a NaN in `rectangular2polar`, and the plugin had no way to know
  the port was not really connected. Fixed upstream (clap-wrapper #498) rather
  than here. CLAP's own mechanism is `clap_audio_buffer::constant_mask`, which
  nothing here reads.

  Still open, and not closed by the tests added on 05.08.2026 — those drive all
  three arms `runEngine` can distinguish (no second port, a second port with no
  `data32`, a second port with audio) and confirm the first two are the same
  fallback. What they cannot do is give the plugin a way to tell a *connected*
  port carrying silence from an unpatched one carrying whatever the host left
  behind, which is the entry.

- **The engine's guards are finiteness guards, and garbage is usually finite.**
  (03.08.2026) Every `LE_MATH_VERIFY_VALUES` on the input path tests `Invalid`
  (NaN/infinity) or denormals. Uninitialised memory read as float is
  overwhelmingly *huge and finite* — the measured value was 2.9e33 — so it passes
  `time2DFT`'s checks on the time domain, on the window and on both FFT outputs,
  and only becomes NaN when squared inside `vDSP_zvabs`. The assert that fires is
  therefore three layers away from where the bad data entered, which is why the
  entry above cost a day. A magnitude bound on the incoming block would have named
  it immediately; whether the engine should carry one in a release build is a real
  question and not obviously yes.

## Parameters and LFOs

- **Should a genuine tempo change move a host-visible parameter at all?**
  (03.08.2026) `LFOImpl::updateForNewTimingInformation()` rescales a **Free**
  LFO's period by the bar-duration ratio, so that the period stays constant in
  seconds when the tempo changes. `LFOImpl::Timer::establishedChange()` stops the
  *first* announcement of a tempo counting as a change — the engine assumes 120
  BPM until told, and a host announcing 140 used to be indistinguishable from a
  user retempoing the project — but that only settles the assumption. Mid-session,
  a real tempo change still rescales every Free LFO's period, and the number the
  host sees — and automates, and has saved in its project — moves with it.

  Holding the sounding period constant and holding the automation value constant
  are incompatible; the honest resolutions are bigger than a flag. Either export
  the tempo-independent quantity at the CLAP edge and convert in `CLAPEdge`, or
  store the period in seconds internally and convert to bars where it is used.
  The first does not touch the file format, which makes it the cheaper of the two.

- **The measure-numerator half of `establishedChange()` is reasoned, not
  measured.** (03.08.2026) It reports no change for the meter as well as for the
  bar duration, on the same argument — there was nothing to change *from*. The
  synced arm of `updateForNewTimingInformation()` resnaps the period when the
  numerator changes, so the same class of bug exists there. But **nothing in the
  suite drives a meter other than 4/4**, so that arm is unexercised in both
  directions. A case at 3/4 would settle it.

- **An out-of-range LFO sub-parameter index asserts instead of being dropped.**
  (03.08.2026) `ParameterCounts::lfoExportedParameters` is 5, so a
  host sees Enabled, PeriodScale, Phase, LowerBound and UpperBound; SyncTypes and
  Waveform are internal. Writing index 5 anyway reaches
  `Automation::Detail::autoAdjustedLFOParameter` and trips
  `LE_ASSUME(lfoParameterIndex < lfoExportedParameters)`
  (`automatedModule.cpp:140`) — in a release build, an index past the end. No
  conforming host can reach it, since the id is not exported; neither could a
  conforming host send an out-of-range parameter *value*, and that one is now
  clamped at the edge rather than trusted. This is the same guard and the same
  argument. Found by accident, writing the id by hand while chasing the entry
  above.

## DSP and effects

- **A phase-vocoder pitch shift's accuracy depends on the FFT size, and not
  monotonically.** (01.08.2026) Measured, with Pitch Magnet asked to
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
  (01.08.2026) All three were found by writing property tests and all
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
  DSP**, so a debug-only regression in the engine has nothing to catch it. The
  effect property tests are what a checked build has instead, and as of
  05.08.2026 they reach all 57: nine in `amplifyingEffectsTests.cpp`, four in
  `silentDefaultsTests.cpp`, and every one of the 57 in `sideChainTests.cpp` —
  which has to name Smoother as its single exception for exactly this reason.

- **`usesSideChannel` is metadata nothing consumes, and it is wrong.**
  (01.08.2026, measured 05.08.2026) Every effect declares
  `static bool const usesSideChannel` and `effects.hpp:61` documents it as part
  of the effect contract, but a grep of `src/` and `tests/` finds **no reader**
  — dispatch is by parameter type, so what decides is whether the effect's
  `process()` overload takes a `MainSideChannelData`.

  Driving the side chain with a signal of its own settles how wrong: the
  constant says **seven** effects, the engine's behaviour says **fifteen**. It
  names Colorifer, Blender, Burrito, Inserter, Talking Wind and both Pitch
  Followers, and misses Slicer, Convolver, Denoiser, Ethereal, Vaxateer,
  Shapeless, Merger and Sumo Pitch. `convolver.hpp` declares `false` while
  `convolverImpl.hpp` takes `MainSideChannelData_AmPh`, which is the same fault
  seen from the other end. It should go; `sideChainTests.cpp` holds the measured
  set and does not read it.

- **Four side-chain effects are indistinguishable from deaf at their defaults.**
  (05.08.2026, from `sideChainTests.cpp`) Which is why the side-chain fixtures
  configure them, and it is worth reading as a *user*-facing observation rather
  than a test one — dropping any of these four into a slot and patching a signal
  into the side chain port does nothing until a second parameter is moved:
  - **Slicer**, **Denoiser** and **Convolver** each have an enumerated mode whose
    default is its *first* enumerator, and in all three cases that enumerator is
    the one that ignores the side channel (`Hold`, `Main`, `Triggered`).
  - **Convolver**'s is the strongest form of it: `Triggered` means the impulse
    response is grabbed on a button press, so until then the effect renders
    **silence**. Eight of the 25 identically-hashed golden fixtures are
    Convolver's, and they are not a quiet render — they are an unarmed one. See
    "25 golden fixtures render silence" under Tests.
  - **Burrito** chooses its replacement positions only when its frame counter
    wraps `Period`, which defaults to 250 ms, so nothing at all happens for the
    first quarter second whatever is on the port.

  None of this is a regression and all of it is 2016 behaviour. It is recorded
  because "the side chain does nothing" is a plausible bug report against four of
  the fifteen, and the answer is a parameter rather than a fix.

## Tests

- **25 golden fixtures render silence, and none of it is about external audio.**
  (05.08.2026, from `silentDefaultsTests.cpp`) A silent render hashes identically
  on every platform and has zeroes in every numeric column, so those rows agree
  with any build ever made — 25 of 464 fixtures that cannot fail. `todo.md` put
  it down to the sample loader having been compiled out and proposed feeding a
  fixture a factory sample; **both halves are wrong**. `Sample` is built on
  `juce::File` and the goldens live in `sw-dsp-tests`, which links no JUCE at all
  by design, so such a fixture would not link. And the four causes are ordinary
  parameter defaults:
  - **Convolver** (8 rows) defaults to `Triggered`, so there is no impulse
    response until Grab IR is pressed. Unarmed rather than quiet.
  - **Frecho and Frevcho** (16) default to 100 m, and the delay is the round trip
    at 343 m/s: 583 ms against a 371 ms fixture. These sixteen are a statement
    about the *fixture length* — a two second matrix would never have had them.
  - **Freqnamics** (1) gates at −60 dB, and one impulse spread over 2048 bins is
    quieter than that per bin. The same render at 512 bins is not, which is why
    only one of its eight rows is silent.

  All four are 2016 behaviour and moving any of them changes what a preset sounds
  like, so none is a fix. The count is now held by a case and can only fall.

- **A new LFO's default sync type depends on what some other instance was told.**
  (05.08.2026, from `lfoTests.cpp`) `LFOImpl::SyncTypes::default_()` is
  `Timer::hasTempoInformation() ? Quarter : Free`, and that flag is one of the
  three **process-wide statics** on `Timer`. So the default is `Free` until
  anything anywhere in the process reports a tempo and `Quarter` for ever after:
  `reset()` deliberately does not clear it — a 2012 workaround for preset
  browsing in Live — and only the no-transport overload of
  `updatePositionAndTimingInformation` does.

  Which makes it order-dependent in a way that reaches the *preset format*:
  `adjustValueForPreset` writes a Free LFO's period in milliseconds and a synced
  one in bars, so the same session saved before and after the host's first
  transport report writes different files. Found because the first version of
  `lfoTests.cpp` left the flag set and turned 303 preset digests red in
  `presetCorpusTests.cpp`, a file that never mentions an LFO. The test now scopes
  it (`ScopedHostTiming`); the fix is the same per-instance timer the note on
  `Timer`'s statics already asks for.

- **`LFOImpl::Timer::setPosition( float )` asserts two things that are both
  false, and is dead.** (01.08.2026) `lfoImpl.cpp:753-755` reads
  `LE_ASSUME( barDuration_ == 4 )` and
  `LE_ASSUME( measureNumerator_ == 60.0f / 120 * 4 )` — the two values swapped
  between them. The initialisers three hundred lines up are
  `barDuration_( 60.0f / 120 * 4 )`, which is 2, and `measureNumerator_( 4 )`, so
  both assumptions are false as written. Nothing has noticed because nothing
  calls it: its only caller is `Engine::Processor::setPosition`, and that has no
  callers at all in the CLAP path. Harmless while dead; `LE_ASSUME` is
  `__builtin_assume` in a shipping build, so reviving the caller without fixing
  the pair would hand the optimiser two false facts about live values.

- **A slot filled from the editor needs its rack resynced by hand, and two
  harnesses do it and one did not.** (03.08.2026)
  `addUserAddedModule` ends in `refreshModuleRackAsync()`, so a caller with no
  message loop has to call `resyncModuleRack()` itself; `pluginTests.cpp` did and
  `tools/show-ui`'s editor-module page did not. For a month that page rendered an
  editor with a highlighted empty slot and no module in it — the page whose whole
  purpose is proving a module's widgets can be built. It was invisible until the
  sweep that drew all 57 effects made them produce *byte-identical* PNGs. Fixed
  there, but the shape of it is not: "build the thing, then pump the async step by
  hand" is an unwritten rule that three harnesses now follow separately, and the
  next one will not know either.

- **`ctest -LE slow` skips nothing.** (01.08.2026) No test in the repo sets
  `LABELS`; the one labelled case went with `check_gui_flag_parity.py`. Either
  re-establish the label or stop recommending the flag — several documents do.

- **clap-helpers' flush validation calls its own `[main-thread]` entry point
  from the audio thread.** (03.08.2026) `clapParamsFlush` guards
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
  (03.08.2026) `params.flush()` is `[active ? audio-thread :
  main-thread]`, and every `params.flush(&*plugin, …)` call site in
  `pluginTests.cpp` — several dozen — makes it from the main thread against an
  active plugin. Harmless there, because those cases use hosts that offer no
  thread check and so nothing asks; but it means the flush path has only ever
  been driven from the wrong thread, and `paramsFlush()` calls `drainCommands()`
  and `handleEvent()`, which are the two things `process()` does to the engine.
  Converting them to `plugin.flush(…)` is mechanical and would put the whole
  parameter-event path on the audio thread under tsan and rtsan, where it belongs.

- **A `checkMainThread()` failure is invisible to `TestHost`, and one case
  overclaims because of it.** (03.08.2026) clap-helpers'
  `checkMainThread`/`checkAudioThread` write to `std::cerr` directly
  (plugin.hxx:2219, 2233) rather than through `hostMisbehaving`, so nothing that
  routes through `clap.log` can see them — which is the same limitation the flush
  entry above records, seen from the other end. The consequence is that
  `hostInteropTests.cpp`'s "Driven the way a DAW drives it, **nobody
  misbehaves**" asserts only over the reports that *can* be intercepted: that
  case emits one such line to stderr and passes. Either capture stderr in
  `TestHost` for the duration of a case and assert on it too, or rename the case
  to what it actually checks. The first is worth more and is not hard.

## Licence and shipping

The decision itself is settled and written down in
[`LICENSING.md`](../../LICENSING.md): source GPL-3.0-or-later, released binary
AGPL-3.0-or-later because JUCE 8 is AGPLv3-or-commercial. The 452 file headers
are right as they stand. What is below is packaging.

- **The duplicate licence file and the installer path that names it.**
  (03.08.2026) `doc/manual/EULA.txt` is a byte-for-byte duplicate of `LICENSE`
  under a filename that means the opposite of what it contains — 2016's
  commercial agreement was replaced before the port began and only the name
  survived. `src/legacy-build.cmake:386` still points
  `CPACK_RESOURCE_FILE_LICENSE` at `../installer/ProgramFolder/Licences/EULA.txt`
  — a path outside this repository. Neither is reachable from a live target, so
  neither is urgent; both are for whoever builds a real installer, and the AGPL
  statement is what it has to show.

- **The standalone's `CFBundleIdentifier` is clap-wrapper's
  `SpectrumWorx.standalone`.** (01.08.2026) A bundle *name* with a suffix rather
  than a reverse-DNS identifier, hardcoded in clap-wrapper's own `Info.plist.in`,
  and no CMake property overrides that one key. Notarisation is the step that
  cares. The only local remedy is to carry a whole copy of their template to
  change one line, so the fix belongs upstream — a `BUNDLE_IDENTIFIER` the
  standalone wrapper honours the way the plugin wrappers already do.
  `src/clap-first/CMakeLists.txt` carries the note so the next person to look does
  not re-derive it.
