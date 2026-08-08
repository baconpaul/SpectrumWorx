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
| Tests | **Green on 07.08.2026** — 119 plugin cases and 143 dsp cases, Debug and Release. Two binaries, `sw-dsp-tests` and `sw-plugin-tests`, plus 66 `sw-show-ui` renders. Goldens run in Release only. |
| Validators | `clap-cpp-validator` **22 of 22 with zero failures** and `vst3-validator` **47/47**, both re-run on 07.08.2026 against `text_to_value`. `auval` was 5 runs of 5 on 06.08.2026 and **has not been re-run since**: it reads `~/Library/Audio/Plug-Ins/Components`, so running it means installing over whatever is there. The one `scan-time` warning comes and goes with the page cache (below). All by hand on this machine; CI runs none of them. |
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

### Let a user type a value into a knob

The plugin can read its own display back now: `DisplayValueTransformer` has an
`inverse` beside its `transform`, `Parameters::parse` mirrors
`Parameters::print` tag for tag, and `SpectrumWorxCLAP::paramsTextToValue`
answers. `parameter_system.md` §8 is the mechanism, and
`tests/clap/parameterTextTests.cpp` holds every parameter of every one of the 57
effects to the round trip.

So a **host's** generic panel can be typed into. The plugin's own editor still
cannot: right-click a knob and type a value, which is what the whole inverse was
wanted for. That is `EditorKnob` and the module parameter widgets, and it needs
nothing new underneath — `Plugin2HostPassiveInteropImpl::getParameterFromDisplay`
is the call, and it takes the `Program` the caller owns.

Worth doing beside it: the knob's own value display already goes through
`Parameters::print`, so typing and showing would finally be the same pair of
functions rather than two spellings of the same table.

### Consider the cpputils ring buffer

`src/core/threading/spscQueue.hpp` is ours, hand written, and carries the
engine's two channels. `sst-cpputils` is already a dependency and ships a ring
buffer; if it fits, the queue that everything else in this design rests on stops
being code we maintain and start being code somebody else tests. Worth an hour
to compare the two interfaces before deciding — `threading_model.md` §3
describes what the channels actually need, which is less than a general queue
offers.

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

### The `LE_` macro layer: a Windows log, and one decision

The strip ran on 07.08.2026 and is done on macOS: 184 `LE_` names down to 83,
around 8,700 lines deleted over two branches, goldens bit-identical throughout.
What survives is what C++20 genuinely cannot express — the assert family,
`LE_RESTRICT`, `LE_ASSUME`, the alloca buffers, and the effect and resource
X-macro tables. Two things are left over.

**A Windows log is still owed.** The first came back on 07.08.2026 and stopped
in `sw-dsp` on the secure-CRT shim's removal, which is fixed. Nothing after that
target compiled, so most of the strip remains unseen by an MSVC and these two are
still unverified:

- The assertion handler writes to `OutputDebugStringA` itself now, rather than
  through the deleted tracer.
- `float &LE_RESTRICT` replaced `LE_GNU_SPECIFIC` at six sites in `vector.hpp`
  and `phase_vocoder/shared.cpp`, where a 2012 comment says MSVC could not take
  a restricted reference; three of those also changed MSVC from pass-by-value to
  pass-by-const-reference.

**`LE_MATH_NATIVE_POINTER_SIZE_INTERFACE` is the one decision left**, 37 sites
in `vector.cpp`. It is not a dead arm. `vector.cpp` publishes each primitive
three ways — Span, `(begin, end)`, and `(pointer, count)` — and exactly one of
the latter two can hold the implementation while the other forwards, or they
recurse. This macro picks which. It is defined on Apple, where the vDSP and vvv
calls are naturally `(pointer, count)`, and undefined elsewhere, where the
`(begin, end)` forms hold the loops.

So both arms have a live reason, which is why the NT2 strip did not settle it.
The question worth answering is whether the three-way interface is wanted at all
now that there is exactly one vectorised backend: collapsing to Span plus one
forwarding pair would delete the macro and about a third of the file's surface.
That is a refactor of the vector math, so it belongs beside the entry below
rather than in a macro sweep.

### Dead code that needs a decision rather than a sweep

Roughly 5,800 lines across 45 files are in the tree and in no target. Every one
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
- **`le/math/vector.cpp`'s three interfaces.** The NT2 arm is gone as of
  07.08.2026 and the file is 1,126 lines rather than 2,002, but it still
  publishes every primitive three times — Span, `(begin, end)` and
  `(pointer, count)` — with `LE_MATH_NATIVE_POINTER_SIZE_INTERFACE` choosing
  which pair holds the implementation and which forwards. That made sense with
  two vector backends. With one, collapsing to Span plus a single forwarding
  pair would take about a third of the file's surface and the last live
  configuration macro in the math layer with it. It is the vector math the whole
  engine runs on, so it is a decision and not a sweep.

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
