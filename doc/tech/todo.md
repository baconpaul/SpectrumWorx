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
| Tests | **299/299** as of 05.08.2026, in both build trees. Two binaries, `sw-dsp-tests` and `sw-plugin-tests`, plus 66 `sw-show-ui` renders. Goldens run in Release only. |
| Validators | `auval` 10 runs of 10. `vst3-validator` 47/47. `clap-cpp-validator` 21/21, one warning (`scan-time`, below). |
| CI | `.github/workflows/build-plugin.yml`, five jobs over three platforms. **Written on 05.08.2026 and never run** — item 2. |
| Warnings | **Two**, both deliberate `#pragma message` build banners. Our own sources compile under `-Wall -Wextra -Werror`, on Apple by default and elsewhere with `-DSW_WERROR=ON`. MSVC has no baseline yet — item 2. |
| Sanitizers | rtsan and tsan clean over both test binaries, against the model as it stood on 02.08.2026. `reset()` and `paramsFlush()` entered the realtime region on 03.08.2026 and **have not been run under rtsan since** — item 1. |

---

## The ordered list

| # | What | Size |
|---|---|---|
| 1 | **Drive it in a DAW** and settle whether the deadlocks are gone | 1–2 days |
| 2 | **Get the CI matrix green** — it is written and has never run | 1–3 days |
| 3 | **Ship** — README, manual, notarisation | 1–2 weeks |

### 1 — Drive it in a DAW

**The only thing that can confirm the threading redesign.** SpectrumWorx
deadlocked in Logic and in Bitwig in certain situations; every part of the cycle
that was attributed to has since been deleted, the processing lock included. That
is an argument, not an observation, and nothing has been back into either host.

**The by-hand list**, and the order matters — the first two are the prime
suspects:

1. Loading a preset **while audio runs**.
2. Changing the FFT size from the host's generic panel **while audio runs**.
3. **Loading an external audio file and hearing it.** Never worked in this port
   until 05.08.2026 — `activate()` asked the engine for no side channels, so the
   guard on the sample path was `0 >= 2` and a loaded file was silently ignored
   in every format. Fixed and covered by `sampleFeedTests.cpp`, and *no human
   has heard it*, which is what this row is for.
4. Putting an effect in a slot from the host's panel rather than the editor.
5. Saving and reloading a session.
6. Opening and closing the editor with two instances on two tracks.
7. Reaper, in addition to Logic and Bitwig.

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

  **And `runEngine()`'s sample branch has never executed under anything**, rtsan
  included: it was guarded on side buffers that `activate()` never asked for
  until 05.08.2026, so twenty lines of per-block work — `sampleChunk()`, a
  wrapping read and a copy into the side buffers — enter the realtime region for
  the first time with this pass. `sampleFeedTests.cpp` is what drives them.

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

### 2 — Get the CI matrix green

**The workflow is written and no leg of it has ever run.** Everything below is
therefore a claim, and the whole point of this item is to find out which of them
are true. `.github/workflows/build-plugin.yml`:

| Job | What | Where |
|---|---|---|
| `gates` | `scripts/check_format.sh` and `scripts/check_boost_allowlist.sh` | ubuntu |
| `test` | `ctest`, Debug **and** Release, `SW_BUILD_PLUGIN_BUNDLES=OFF` | ×3 |
| `build_plugin` | the four bundles; the installer too off a pull request | ×3 |
| `build_plugin_docker` | the Linux binary against Ubuntu 20's glibc | ubuntu |
| `publish-*` | Nightly on `main`, a release on a `v*` tag | ubuntu |

**Both configurations, because neither alone is 299/299.** The goldens `SKIP`
under `!NDEBUG` (`goldenTests.cpp:287-298`), so Release is the only one that
renders DSP and Debug is the only one that runs the ~1200 asserts. Release also
carries warnings Debug does not — two of the twenty-two the baseline first found
were variables kept only to feed an assert.

**What is still open.**

- **Ethereal's eight golden fixtures fail on Windows, and it is not the FFT
  backend.** Peaks of 80.9 and 13.6 against 0.5 and 0.46 — not drift, a
  different render. `-DSW_FORCE_PFFFT=ON` is what settled the cause: it puts the
  pffft branch on Apple, so the backend can be changed without changing the
  platform, and across that boundary **463 of 464 fixtures hold and all eight
  Ethereal ones pass**, where across the platform boundary 455 hold. So the
  variable is Windows, not Accelerate-versus-pffft.

  Ethereal is "compare and replace": its whole output is `side < main *
  threshold` per bin, and it is the only effect that turns a comparison
  straight into its output, so it magnifies whatever the real difference is. It
  is not a chaotic-sensitivity story — the same magnification is present in the
  pffft-on-Apple run and produces nothing. `mode_.unpack()` is a plain switch
  and `dB2NormalisedLinear` is `std::pow(10, dB/20)`; neither is a suspect.
  Nothing else has been ruled out.
- **`Talking_Wind/sweep/2048/8` is over the rms bound by a hair** — 0.0001198
  against 0.0001 — and does it against *both* pffft builds, so it is a real
  backend difference and the tolerance is calibrated a shade too tightly for it.
  One fixture, one number, and it needs a decision rather than an investigation.
- **MSVC has no warning baseline at all**, deliberately — see `tech_debt.md`.
  `SW_WERROR` is a no-op there by construction (`sw-our-sources.cmake`), so
  Windows compiles warning-blind, and `/W4 /WX` is still a decision rather than
  a setting. The first run of it is the only cheap one.
- **The Ubuntu 20 / GCC 11 container has not got far enough to say anything**,
  and it is the oldest compiler this C++20 tree has been near.
- **Seven secrets and a tag have to exist** before a push to `main` publishes
  anything: `MAC_CERTS_P12`, `CERT_PWD`, `MAC_SIGNING_CERT_NAME`,
  `MAC_INSTALLING_CERT_NAME`, `MAC_SIGNING_ID`, `MAC_SIGNING_1UPW` and
  `MAC_SIGNING_TEAM` — and the `Nightly` tag, which the workflow reuses and
  deliberately does not create.

**What it has already found.** Eleven fixes so far, and two of them are the
argument for the whole item.

> **`Math::log2( float )` returned 1/ln2 times the answer on MSVC.** It divided
> an already-base-two result by ln2, and had done since the 2016 import — but
> the block is guarded on `__APPLE__ || !LE_HAS_NT2`, so in 2016 every Windows
> build took `nt2::log2` instead and the wrong arm was never compiled. Stage 4
> removed NT2, which made it the only arm; the `__GNUC__` branch beside it then
> covered for it on the two platforms anybody built. The live callers are
> `interval12TET2Semitone()` — every pitch effect — musical-scale octave
> detection and the LFO's skew factor. **The first Windows build in this port
> is what found it**, in a test written to ask exactly that question.

> **Seventeen tests had stopped existing**, found by counting what the matrix
> would run rather than by running it. The 05.08.2026 clang-format sweep
> reflowed `effectsList.hpp`, and `tools/show-ui/CMakeLists.txt` parses that
> table to register one UI-render test per effect, so 57 became 40 and the suite
> went quiet about it. The header holds its layout now and the parse asserts its
> count against the header's own `LE_SW_NUMBER_OF_EFFECTS`. **A test that stops
> being registered reports nothing.**

The rest were build failures rather than bugs: `__has_feature` on GCC, a
flexible array member in an empty struct, a zero-length dispatch table, `M_PI`,
`std::fputc`, JUCE reaching for libcurl, a message box linked into the engine,
and `_CONSOLE`. Every one of them was invisible from a Mac.

### 3 — Ship

**`README.md` is the 2016 one.** Ten lines, says "The code does not compile, the
build does not work", and links to `source/…` paths that stage 0 deleted. It is
the first thing anyone sees, and it is the cheapest item on this page.

The licence is settled — [`LICENSING.md`](../../LICENSING.md): source
GPL-3.0-or-later, released binary AGPL-3.0-or-later because JUCE 8 is
AGPLv3-or-commercial. The 452 file headers are right as they stand and no `sed`
is needed. `assets/installer/License.txt` is what both installers show: that
statement, then the GPL-3.0 text it refers to.

**The installers build.** `spectrumworx-installer` makes a `.pkg` in a `.dmg` on
macOS, a `.zip` and an Inno Setup `.exe` on Windows, and a `.zip` on Linux; the
icons all come from `assets/LOGO.png` through `scripts/make_icons.sh`. Only the
macOS one has been run, and only unsigned. What is left here:

- **Notarisation has never been attempted.** The path exists — `make_installer.sh`
  signs, submits and staples when `MAC_SIGNING_CERT` and the rest are set — and
  an unsigned build takes the same path with the signing steps skipped, so what
  is untested is the credentials and Apple's answer, not the plumbing.
- **The standalone's macOS bundle identifier is not ours.** clap-wrapper
  hardcodes `"<name>.standalone"` in its own `Info.plist.in` and no CMake
  property overrides that key; `src/clap-first/CMakeLists.txt` says so at
  length. It matters at notarisation and nowhere before.
- **The manual**, and the three loose ends `tech_debt.md` records under "Licence
  and shipping".

---

## Smaller work, not in the order

Each of these is under a day and none of them blocks anything.

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

Roughly 8,000 lines across 51 files are in the tree and in no target. Every one
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
- **Five cmake files that record a build nobody runs** — `legacy-build.cmake`
  (768 lines), `core/sources.cmake` (342), `le/utility/CMakeLists.txt` (220) and
  two `configuration.cmake` (189). Nothing includes any of them;
  `src/CMakeLists.txt` says so in its second line. They were kept as the record
  of the 2016 build, and the deletions of 05.08.2026 cost them that: **65 of the
  names in them are now files that do not exist**, so as a record of anything
  they are already wrong. Either delete them — git has the 2016 build — or
  accept that they are prose and stop expecting them to resolve.

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
item 1's first step and everything else's acceptance test.
