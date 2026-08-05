# SpectrumWorx — What is left to do

The work queue. Everything here is open; when an item closes it comes out of this
file rather than being marked done, and whatever it left behind goes to
[`tech_debt.md`](tech_debt.md).

Sizes are estimates and have been wrong in both directions. The rule that has
held: **the estimate is usually right and the value is usually somewhere else**
— four of the last six items were worth more for what they found than for what
they were scoped to do.

---

## Where this stands

| | |
|---|---|
| Builds | CLAP, VST3, AUv2, standalone — macOS arm64. Linux built on 04.08.2026 under GCC 15, as a log rather than here; its 469 warnings are fixed and **the fixes have not been compiled by a GCC**. Windows arrives as logs. |
| Runs | Standalone, with audio, with the real editor, with presets. It deadlocked in Logic and in Bitwig on the 2016 threading model; **that model has been replaced and nobody has reloaded it in either host** — item 1. |
| Tests | **270/270** as of 05.08.2026, in both build trees. Two binaries, `sw-dsp-tests` and `sw-plugin-tests`. Goldens run in Release only. |
| Validators | `auval` 10 runs of 10. `vst3-validator` 47/47. `clap-cpp-validator` 21/21, one warning (`scan-time`, below). |
| CI | **None.** There is no `.github/`. |
| Warnings | **Two**, both deliberate `#pragma message` build banners. Our own sources compile under `-Wall -Wextra -Werror`, on Apple by default and elsewhere with `-DSW_WERROR=ON`. MSVC has no baseline yet — item 2. |
| Sanitizers | rtsan and tsan clean over both test binaries, against the model as it stood on 02.08.2026. `reset()` and `paramsFlush()` entered the realtime region on 03.08.2026 and **have not been run under rtsan since** — item 1. |

---

## The ordered list

| # | What | Size |
|---|---|---|
| 1 | **Drive it in a DAW** and settle whether the deadlocks are gone | 1–2 days |
| 2 | **CI**, three OSes × four formats, with the gates that already exist | 3–5 days |
| 3 | **Ship** — README, manual, installers, notarisation | 1–2 weeks |

### 1 — Drive it in a DAW

**The only thing that can confirm the threading redesign.** SpectrumWorx
deadlocked in Logic and in Bitwig in certain situations; every part of the cycle
that was attributed to has since been deleted, the processing lock included. That
is an argument, not an observation, and nothing has been back into either host.

**The by-hand list**, and the order matters — the first two are the prime
suspects:

1. Loading a preset **while audio runs**.
2. Changing the FFT size from the host's generic panel **while audio runs**.
3. Putting an effect in a slot from the host's panel rather than the editor.
4. Saving and reloading a session.
5. Opening and closing the editor with two instances on two tracks.
6. Reaper, in addition to Logic and Bitwig.

**If either host still hangs, take both backtraces before anything else** — one
from each side. That ask expired once already: the redesign landed without them,
so if the deadlock survives there is no evidence and it starts from a live
reproduction anyway.

**Two questions this row owns beyond the hang.**

- **Does a real host honour `request_restart`?** It is how every spectral-setup
  change lands (`threading_model.md` §5) and the test hosts implement it as a
  no-op observer. A host that ignores it leaves the FFT size parameter reading
  one thing and the engine running another — visible, harmless, and not what
  anyone asked for. Fallback if one does: apply the setup in the command handler
  under the same concession a host-written slot selector already has.
- **Run the tests under rtsan again first.** `reset()` and `paramsFlush()` are
  inside the realtime region for the first time as of 03.08.2026, so a
  sanitizer build is expected to report at least the slot-selector allocation
  `tech_debt.md` records, and possibly more. `threading_model.md` §8 has the
  configure line. This is cheap and it is upstream of the DAW pass.

**Convert `doc/manual/SpectrumWorx test procedure.doc` first.** It is Little
Endian's own version of exactly this list, sitting unread in the tree since stage
0.5 moved it. Week one's four bugs were all found by driving the plugin by hand
and none of them by a test; a written first-five-minutes checklist is also the
acceptance test for anything that touches the editor.

**What the validators already settled, and the one thing they did not.** All
three now pass, and between them they found four bugs — none of which was
reachable from the suite as it then stood. Each is pinned by a case now, so the
suite would catch a regression; what it could not do was find them. The
survivor:

> **`scan-time` against a 100 ms limit: over, and unmeasured.** 301 ms, then
> 18 ms, then 274 ms across three runs of the same binary — dominated by whether
> the bundle and its dependencies are in the page cache. Whatever we do at scan
> time is worth reducing, and the measurement needs a cold-cache protocol before
> it can say so in either direction.

`vst3-validator` is not obvious enough to find twice: clap-wrapper ships a
`vst3_validator` target that builds Steinberg's own validator out of the
already-fetched SDK, so `cmake --build <dir> --target vst3_validator` is all it
takes and the binary lands in `<dir>/validator-build/bin/Debug/validator`.

### 2 — CI

Nothing exists. The gates mostly do — they are just not wired:
`scripts/check_boost_allowlist.sh` (run by hand),
`tests/checkODRHeaderScope.cmake` and `tests/checkNoJuceInDSP.cmake` (ctest cases
already), and `clang-format` (see below — the tree is not clean).

Model it on OB-Xf's `build-plugin.yml` with `sst-githubactions/prepare-for-juce`.
Matrix: macos-universal, windows-msvc-x64, windows-arm64, linux-x64, linux-arm64.
Take `add_clapfirst_installer` from two-filters' `basic_installer_clapfirst.cmake`.

**The one thing to get right on day one: run `ctest` in Debug *and* Release.**
The goldens `SKIP` under `!NDEBUG` (`goldenTests.cpp:287-298`), so Release is the
only configuration that renders DSP and Debug is the only one that runs the
~1200 asserts. Neither alone is 264/264. The nine effects' property tests run in
both and narrow the gap without closing it. Release also carries warnings Debug
does not — two of the twenty-two the baseline first found were variables kept
only to feed an assert.

**Pass `-DSW_WERROR=ON` on every leg.** The warning baseline is on our own
sources everywhere (`cmake/sw-our-sources.cmake`); what the option adds is that a
warning stops the build. It is on by default on Apple only, because that is where
the development happens and nowhere else can a new compiler's new warning be
somebody's problem at the wrong moment.

**Two of the five legs have never compiled our sources with it.** Windows is one
of them and the bigger one: MSVC gets no baseline at all, deliberately — see
`tech_debt.md` — so `/W4 /WX` is a decision for whoever wires the matrix, and the
first run of it is the only cheap one. Linux is the other, and its first GCC 15
build (04.08.2026, against `00383f6`) is the argument for the whole item: 469
warnings from four causes, three of them ours, one of them a `-Wnonnull` on a
null-`this` offset computation the source had been calling UB since 2016. All
four are fixed; none of them was visible from a Mac.

### 3 — Ship

**`README.md` is the 2016 one.** Ten lines, says "The code does not compile, the
build does not work", and links to `source/…` paths that stage 0 deleted. It is
the first thing anyone sees, and it is the cheapest item on this page.

The licence is settled — [`LICENSING.md`](../../LICENSING.md): source
GPL-3.0-or-later, released binary AGPL-3.0-or-later because JUCE 8 is
AGPLv3-or-commercial. The 452 file headers are right as they stand and no `sed`
is needed. What is left here is packaging: installers, the manual, notarisation,
and the three loose ends `tech_debt.md` records under "Licence and shipping".

---

## Smaller work, not in the order

Each of these is under a day and none of them blocks anything.

### The side chain, above the engine

The engine and the plugin edge are both done —
`tests/effects/sideChainTests.cpp` drives fifteen effects with a signal of their
own and 60 fixtures pin the result, and `pluginTests.cpp` now declares a second
`clap_audio_buffer` so `runEngine`'s port-1 branch runs. That found four effects
that hear nothing at their defaults and a `usesSideChannel` constant that is
both unread and wrong; both are in [`tech_debt.md`](tech_debt.md).

What is left is the sample. **The harness can feed a *file* now.** 25 golden
fixtures hash identically on every platform because they render pure silence —
`Convolver`, `Frecho`, `Frevcho` at default parameters — and the old explanation
was the flag that compiled the sample loader out. The loader is back and the
factory samples are in the binary, so a fixture can load one by name and those 25
can pin something. Convolver's three are already understood: it renders silence
because `Triggered` is its default and nothing has pressed Grab IR.

### Test holes worth the effort

| Hole | Why it matters |
|---|---|
| **Sequential preset loads into one engine** | `presetCorpusTests.cpp:110` deliberately uses one engine per preset. "Load preset B on top of preset A" is what a user does and no case does it. |
| **A malformed, truncated or missing preset *file*** | The state side is covered — four `[clap][state]` cases drive bad streams — and the file side is not. The corpus proves 303 happy paths; `unknownEffect` and `missing` are asserted zero and never driven above zero. |
| **A loaded sample never reaches the DSP** | `sampleTests.cpp` proves all seventeen decode; nothing proves `runEngine()` then feeds one to the engine in place of the port. The reach problem is solved — `stateTests.cpp` loads a sample through `plugin_data` — so what is left is the block itself. |
| **`lfoImpl.cpp` has no direct test** | Only LFO 0 of module 0 targeting Gain is ever exercised. Waveform shapes, sync types, `PeriodScale` snapping, `LowerBound > UpperBound`, an LFO on an enumerated target, several at once — none. A value-table golden fits the existing pattern. Related: nothing in the suite drives a meter other than 4/4, which `tech_debt.md` records as an unmeasured half of a landed fix. |
| **1 of 18 preset banks is ever drawn** | The effect sweep went from 1 of 57 to 57 of 57 and immediately found a page that had been rendering no module at all. The banks are the same shape of cheap breadth and have not had it. |

### The preset browser

Three drifts, all visible to a user, all in `presetBrowser.cpp`:

- The header strip prints `currentDirectory_.getFullPathName()` unconditionally
  (`:907-912`), so `Root` and `Factory` show a stale or empty path.
- It does not remember where it was: `~PresetBrowser` persists
  `currentDirectory_` but not `location_`, so it always reopens at `Root`.
- Four file paths — `file()`, `selectedFile()`, the rename path and
  `browseArrow_`'s folder chooser — are gated only by button enablement, and the
  chooser will happily walk into the on-disk `assets/presets` and present a
  factory bank as writable.

### Dead code that needs a decision rather than a sweep

Roughly 7,400 lines across 35 files are in the tree and in no target. Worth
removing not for tidiness but because each one makes the next audit harder.

**Free deletions, no decision needed** (~1,700 lines):
`core/modules/{moduleGUI.cpp,moduleGUI.hpp,moduleDSP.hpp}` — superseded by
`moduleDSPAndGUI.cpp`, and their own comments say so; `src/debugConsole.cpp`;
`le/build/{precompiledHeaders.{cpp,hpp},juceIncludeWrapper.hpp}` (no target uses
a PCH); `le/utility/{pimpl.hpp,pimplPrivate.hpp,entryPoint.hpp,
filesystemImpl.inl}`; `le/plugins/{entryPoint.hpp,plugin.hpp}` — stage 0.3 kept
`le/plugins/` "until `le/plugins/clap/` works", and it does.

Two joined the list on 04.08.2026, when `LE_SW_SDK_BUILD` went: the SDK's public
module interface `le/spectrumworx/engine/moduleBase.hpp` (202 lines), whose only
remaining mention is in the record-not-build `core/sources.cmake`, and
`le/spectrumworx/effects/configuration/effectTypeNames.hpp` (38), which had one
include and it was inside the macro.

**Platform arms with no platform** (~1,100 lines): `le/utility/`'s Android, JNI,
Matlab and MSVC-universal-build files. `filesystemWindows.cpp` and
`filesystemApple.cpp` are the two to think about rather than delete — either they
come back for the Windows build, or `sst-plugininfra` already covers them.

**Actual decisions:**

- **`src/le/spectrumworx/effects/_unfinished/`** — 16 effects, 3,778 lines.
  `old/initial_scan.md` says read before deleting. A branch or an `attic/` gets
  it out of `git ls-files 'src/**'` without losing it.
- **Four finished effects that were never shipped** — `vocoder`, `synth`,
  `talk_box`, `dissonancizer`, 1,325 lines. **Not port leftovers**: the 2016
  `effectsList.cmake` already had three of them commented out. `effectsList.hpp`
  fixes the count at 57 and the order is ABI, so appending them is legal and
  reordering is not.
- **`src/nt2_static_fft/`** — 5,519 lines, the largest orphan, kept as the stage
  4 reference. Stage 4 is done and `tests/math/fftTests.cpp` is the grading
  harness now. Does the reference still earn its place?
- **Hand-written weak `strnlen`/`wcsnlen`** — `gui.cpp:1236-1264`, "OSX 10.6 does
  not provide std::strnlen", with the `!__LP64__` guard **commented out**. They
  are compiled into every macOS build today, in 2026.

### Formatting

**58 files fail `clang-format --dry-run -Werror`** (21.1.5), counted 04.08.2026,
and only **1** of them is outside `src/le/`. So it is one reformat commit and it
is almost entirely the 2016 sources rather than anything the port has written.
Stage 0.6 established "format-stable" and recorded its reformat in
`.git-blame-ignore-revs`; do the same, and pin the clang-format version in CI
while you are there, since nothing in `.clang-format` does.

One thing that reformat must not eat, and now cannot: a value-string table is one
`{ Value, "string" }` pair per line so that the pairs line up against the
enumerators they name, and clang-format's default is to reflow all of them into a
paragraph. `.clang-format` names `ENUMERATED_PARAMETER_STRINGS` and
`EFFECT_ENUMERATED_PARAMETER_STRINGS` under `WhitespaceSensitiveMacros`, which
holds every one of the twenty-two still.

---

## One thing that is not a task

**Write down what the first five minutes of using SpectrumWorx should be**, and
check the plugin against it by hand: loading a preset, putting an effect in a
slot, turning a knob, saving, closing the editor, reopening it, saving the
session, reloading it. Week one's four bugs were all found that way and none of
them by a test, and the validators' four were found the same way by machines.

`doc/manual/SpectrumWorx test procedure.doc` is Little Endian's own version of
that list and has been sitting unread since stage 0.5 moved it. Converting it is
item 1's first step and everything else's acceptance test.
