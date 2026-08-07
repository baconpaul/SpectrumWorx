# SpectrumWorx — What is left to do

The work queue. Everything here is open; when an item closes it comes out of this
file rather than being marked done, and whatever it left behind goes to
[`tech_debt.md`](tech_debt.md).

Sizes are estimates and have been wrong in both directions. The rule that has
held: **the estimate is usually right and the value is usually somewhere else**
— five of the last seven items were worth more for what they found than for what
they were scoped to do. The CI matrix is the clearest case yet: it was scoped as
plumbing and it found a wrong `log2` that had shipped since 2016, seventeen
tests that had silently stopped being registered, and the reason three effects
cannot be held to a number off the machine that minted their fixtures.

---

## Where this stands

| | |
|---|---|
| Builds | CLAP, VST3, AUv2, standalone, on every push: macOS universal, Windows x64 under MSVC 19.51, Linux x64 under GCC 12.4, and again in an Ubuntu 20 / GCC 11 container for the glibc a released binary needs. |
| Runs | **In DAWs, on macOS, Windows and Linux, driven by testers rather than by us** (07.08.2026). The deadlocks that motivated the threading redesign are gone, and the plugin works. That closes the question the redesign was an argument about: it is an observation now. |
| Tests | **Green on 06.08.2026** — 109 plugin cases and 142 dsp cases, Debug and Release. Two binaries, `sw-dsp-tests` and `sw-plugin-tests`, plus 66 `sw-show-ui` renders. Goldens run in Release only. |
| Validators | **All three clean on 06.08.2026** — `auval` 5 runs of 5, `vst3-validator` 47/47, and `clap-cpp-validator` **22 of 22 with zero failures**, `state-reproducibility-flush` included. The one `scan-time` warning comes and goes with the page cache (below). All three by hand on this machine; CI runs none of them. |
| CI | `.github/workflows/build-plugin.yml`. **Green on 06.08.2026** — ten jobs (gates, five test legs, four builds) over three platforms, run `31112026299`. Windows Debug is the sixth test leg and is excluded; `tech_debt.md` says why. |
| Warnings | **Two**, both deliberate `#pragma message` build banners. Our own sources compile under `-Wall -Wextra -Werror` on Apple Clang, GCC 12.4 and GCC 11 — CI passes `-DSW_WERROR=ON` to every leg. MSVC gets nothing and compiles warning-blind — `tech_debt.md`. |
| Sanitizers | **tsan clean over both binaries as of 06.08.2026** — zero reports across 101 plugin cases and 106 dsp cases, including the case that reproduced the preset-swap race. The earlier clean run, on 02.08.2026, is what that race shows the limit of: nothing then drove a host reading parameters against a running engine, so tsan had nothing to see. `reset()` and `paramsFlush()` entered the realtime region on 03.08.2026 and **have not been run under rtsan since** — below. |

---

## The ordered list

| # | What | Size |
|---|---|---|
| 1 | **Ship** — the manual, and somebody installing the result | 1–2 weeks |

Two items came off this list on 07.08.2026. *Give the main thread its own
`Program`* landed whole; what it built is a property of the design now and
[`threading_model.md`](threading_model.md) states it — rule 2, the diagram, and
the recomputation table. *Drive it in a DAW* was the acceptance test for that
work and testers have now run it on all three platforms.

### 1 — Ship

The licence is settled — [`LICENSING.md`](../../LICENSING.md): source
GPL-3.0-or-later, released binary AGPL-3.0-or-later because JUCE 8 is
AGPLv3-or-commercial. The 452 file headers are right as they stand and no `sed`
is needed. `assets/installer/License.txt` is what both installers show: that
statement, then the GPL-3.0 text it refers to.

**The installers build, on all three platforms, on every push.**
`spectrumworx-installer` makes a `.pkg` in a `.dmg` on macOS, a `.zip` and an
Inno Setup `.exe` on Windows, and a `.zip` on Linux; the icons all come from
`assets/LOGO.png` through `scripts/make_icons.sh`. A pull request builds the
bundles and no installer; a push to `main` builds the installer and signs it,
with the seven `MAC_*` secrets and the `Nightly` tag all in place. **Signing and
notarisation both succeed** as of 07.08.2026 — a push to `main` produces a
signed, notarised, stapled build without hand-holding. What is left here:

- **Nobody has installed the result.** The `.pkg` has never been run on a
  machine that did not build it. That is the acceptance test for the installer,
  and it is now the only part of the shipping path with no observation behind
  it — the plugin's own acceptance test, driving it in a DAW, has been done.
- **The manual**, and the four loose ends `tech_debt.md` records under "Licence
  and shipping" — one of which, the standalone's bundle identifier, is much
  cheaper to settle before a release than after one.

---

## Smaller work, not in the order

None of these blocks anything, and most are under a day.

### `text_to_value` returns false

A host that lets a user *type* a value gets nothing from us:
`SpectrumWorxCLAP::paramsTextToValue` is `return false`
(`spectrumWorxCLAP.cpp:684`), and the `\todo` above it says why. The 2016 code
never needed the inverse — neither VST 2.4 nor AU asks a plugin to parse a typed
value — so `Parameters::DisplayValueTransformer` has `transform` and no
counterpart, and the effect printers go the same one way.

Returning false is the *honest* answer rather than a lazy one, and the previous
implementation shows why: it ran `strtod` and treated display units as storage
units, which clap-validator caught taking input gain `0.001` → `"-60dB"` →
`-60.0` → `"nandB"`, a NaN written straight into the engine.

The work, in order:

1. Give `DisplayValueTransformer` an `inverse` alongside `transform` — four
   specialisations, two dB and two percentage — and teach the effect-specific
   printers the same.
2. **Test it as a round trip**, which is the only way this stays honest:
   for every parameter, `value_to_text` then `text_to_value` must return where it
   started, within the display's own precision. The suite already walks the whole
   parameter list calling `value_to_text` (`pluginTests.cpp:472`), so the walk
   exists and wants the second half.
3. Add it to the test host's parameter API and re-run clap-validator, which is
   what found the NaN.
4. Then the GUI can offer it: right-click a knob and type a value.

### CLAP parameter IDs

Two things about the encoding in `src/core/parameterID.hpp`, which packs a
`type` byte over three index bytes in a union:

- **The first parameter's ID is `0x00000000`.** It falls out of the encoding —
  `GlobalParameter` is the zeroth `Type` and its index is zero — and it is a
  perfectly legal `clap_id`, since only `CLAP_INVALID_ID` (`0xFFFFFFFF`) is
  reserved. It still reads like an uninitialised value in every log and every
  debugger, which is a bad property for the one number a host stores forever.
- **Some IDs are one apart.** An LFO's parameters vary in the low byte, so
  consecutive ones are consecutive integers, while global parameters land on
  `0x10000` boundaries. The density is not wrong, it is just uneven, and it
  leaves no room in the places that are dense.

**The constraint that governs both:** a parameter ID is what a host writes into
a saved session to name an automation lane. Changing the encoding renumbers
every lane anyone has saved. That makes this a thing to settle *before* a
release and not after — the same argument as the standalone's bundle identifier
in `tech_debt.md`, and the same argument that deleted window presum rather than
leaving it switchable.

### Consider the cpputils ring buffer

`src/core/threading/spscQueue.hpp` is ours, hand written, and carries the
engine's two channels. `sst-cpputils` is already a dependency and ships a ring
buffer; if it fits, the queue that everything else in this design rests on stops
being code we maintain and start being code somebody else tests. Worth an hour
to compare the two interfaces before deciding — `threading_model.md` §3
describes what the channels actually need, which is less than a general queue
offers.

### Save-As offers a garbage name

The preset browser's Save-As prefills its name field from
`editor().currentProgramName()` and then appends a `" (NN)"` counter until the
name is unused (`presetBrowser.cpp:563-580`). What a user sees when the box
opens is not a sensible default. Blank it: an empty field with the old name as
placeholder text is what every other application does, and the dedup loop can
run when the name is committed rather than when the box appears.

### Run the tests under rtsan again

`reset()` and `paramsFlush()` went into the realtime region on 03.08.2026 and
have not been under a realtime sanitizer since; neither has `runEngine()`'s
sample branch, which was guarded on side buffers `activate()` never asked for
until 05.08.2026, so twenty lines of per-block work — `sampleChunk()`, a wrapping
read, a copy into the side buffers — have never executed under one at all.
`sampleFeedTests.cpp` drives them. Expect at least the slot-selector allocation
`tech_debt.md` records. `threading_model.md` §8 has the configure line.

This outlived the DAW pass it was filed under: a tester exercising the plugin
confirms it works, which is not the same as confirming it allocates nothing on
the audio thread.

### `scan-time` is over the limit and unmeasured

`clap-cpp-validator` wants 100 ms and got 301, then 18, then 274 across three
runs of the same binary — dominated by whether the bundle and its dependencies
are in the page cache. Whatever happens at scan time is worth reducing, and the
measurement needs a cold-cache protocol before it can say so in either
direction.

### The preset browser

Three drifts, all visible to a user, all in
`src/gui/preset_browser/presetBrowser.cpp`:

- The header strip prints `currentDirectory_.getFullPathName()` unconditionally
  (`:923-929`), so `Root` and `Factory` show a stale or empty path.
- It does not remember where it was: `~PresetBrowser` persists
  `currentDirectory_` but not `location_`, so it always reopens at `Root`.
- Four file paths — `file()`, `selectedFile()`, the rename path and
  `browseArrow_`'s folder chooser — are gated only by button enablement, and the
  chooser will happily walk into the on-disk `assets/presets` and present a
  factory bank as writable.

### Finish stripping the `LE_` macro layer

Begun 07.08.2026 and stopped part-way, deliberately: what is left is the half
that changes generated code, and it wants a Windows log in the loop rather than
a run of green macOS builds.

**Done** — the `LE_ASSUME` double definition (it was defined twice with different
meanings and include order picked the winner, so an assumption broken in twelve
headers was silent undefined behaviour); the sixteen macros defined with no call
site; the five orphan cmake files; the doxygen template; `le/utility/filesystem`
and its `#pragma comment(lib)` naming a library this project has never built; the
dead feature switches `LE_NO_PARAMETER_STRINGS`, `LE_SIMPLE_TUNEWORX`,
`LE_NO_PRESETS`, `LE_NO_LFOs`, `LE_NO_RTTI`/`LE_NO_EXCEPTIONS`; window presum and
its parameter, with the fold collapsed to the single frame it always ran at; the
tracer and the FPU exception guards; the 89 per-translation-unit optimise and
fast-math pragmas; and the macros C++20 can spell for itself. Around 6,400 lines.

**Left**, in the order to do it:

- **`LE_COLD` (166 sites), `LE_HOT` (38), `LE_NOVTABLE` (31).** The plan that
  scoped these called them no-ops, and on MSVC and GCC they are. **On Clang they
  are not**: `LE_COLD` is `__attribute__((minsize))` and `LE_HOT` is
  `__attribute__((hot))`, so deleting them re-optimises 200-odd functions in the
  compiler that renders the goldens. The goldens are the check and the risk is
  real rather than notional — Clang contracts FMAs by default, so a function
  moving from `minsize` to `-O3` can round differently. If they move, that is an
  answer about the macros, not a bug to chase.
- **`LE_DISABLE_LOOP_UNROLLING` (19) and `LE_DISABLE_LOOP_VECTORIZATION` (6).**
  Same shape, sharper: these are live `clang loop` pragmas, and removing them
  lets the vectoriser into loops it has never been in. Do them alone, after the
  above, so a golden change has one candidate cause.
- **`LE_ALIGN`** — one live use, eleven more inside `vector.cpp`'s NT2 arms, so
  it cannot go until that strip below does. `alignas` replaces it.
- **The ODR force-include.** `leConfigurationAndODRHeader.h` still carries a
  ~78-line fake secure-CRT shim dated 2009 that is *reachable in release MSVC
  builds*, a `__GLIBCXX__ < 20110325` typedef and a 2011 iOS `__MMX__` hack. Once
  those go, the only genuinely ODR-critical thing left in it is `LE_HAS_SSE1`; if
  that moves to `target_compile_definitions` then the force-include and
  `tests/checkODRHeaderScope.cmake` can both retire.
- **A Windows log.** Most of what the first three items delete is MSVC-only code,
  and nothing here has an MSVC. Three changes already made are unverified on it:
  the assertion handler now writes to `OutputDebugStringA` itself rather than
  through the deleted tracer, `float &LE_RESTRICT` replaced `LE_GNU_SPECIFIC` at
  six sites in `vector.hpp` and `phase_vocoder/shared.cpp` where a 2012 comment
  says MSVC could not take a restricted reference, and three of those changed
  MSVC from pass-by-value to pass-by-const-reference.

### Dead code that needs a decision rather than a sweep

Roughly 6,500 lines across 46 files are in the tree and in no target. Every one
of them needs somebody to decide rather than somebody to sweep, which is why
each is a paragraph and not a line on a list.

- **`src/le/spectrumworx/effects/_unfinished/`** — 16 effects, 33 files, 3,908
  lines. `old/initial_scan.md` says read before deleting. A branch or an
  `attic/` gets it out of `git ls-files 'src/**'` without losing it.
- **Four finished effects that were never shipped** — `vocoder`, `synth`,
  `talk_box`, `dissonancizer`, 12 files, 1,859 lines. **Not port leftovers**:
  the 2016 `effectsList.cmake` already had three of them commented out.
  `effectsList.hpp` fixes the count at 57 and the order is ABI, so appending
  them is legal and reordering is not.

  **Nothing compiles these, so nothing checks them.** They are in no target and
  `allEffectImpls.hpp` does not name them, so an edit here is checked by reading
  and by nothing else — which is a live hazard, not a hypothetical one: their
  Matlab scaffolding was removed by hand and no compiler has seen the result.
  Whoever revives one starts by getting it into a target.
- **`le/math/vector.cpp`'s dead NT2 arm** — ~750 lines across 18
  `#ifdef LE_MATH_USE_NT2` sites in a 2,034-line **live** file, each with a live
  `#else` beside it. Left when `src/nt2_static_fft/` went on 05.08.2026, and it
  is not a dangling reference the way the deleted files' callers were: the arm
  needs `boost/simd/…`, which **is not vendored either**, so it has been
  un-compilable for as long as this port has existed rather than merely
  unreachable. Stripping it is a refactor of the vector math the whole engine
  runs on, which is why it is a decision and not a sweep — and it is what would
  let `boost/simd` and `boost/dispatch` off `scripts/check_boost_allowlist.sh`.

---

## One thing that is not a task

**Write down what the first five minutes of using SpectrumWorx should be**, and
check the plugin against it by hand: loading a preset, putting an effect in a
slot, turning a knob, saving, closing the editor, reopening it, saving the
session, reloading it. Week one's four bugs were all found that way and none of
them by a test, and the validators' four were found the same way by machines.

`doc/manual/SpectrumWorx test procedure.doc` is Little Endian's own version of
that list and has been sitting unread since stage 0.5 moved it. Converting it is
worth more now rather than less: there are testers to hand a checklist to.

Two things belong on it that nothing else will catch:

- **Change the FFT size from the host's own generic panel while audio runs**, and
  watch whether the change takes. That is the only way to see whether a real host
  honours `request_restart` — `threading_model.md` §5 explains why every spectral
  setup change rides on it, and the test hosts answer it as a no-op.
- **Load an external audio file and hear it.** It never worked in this port until
  05.08.2026, and no human has heard it since it was fixed.
