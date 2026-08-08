# SpectrumWorx — review of src vs doc/tech, and a roadmap for the next Opus work

## Status as of 08.08.2026

Tier 0 and Tier 1 are done, on branch `respond-to-fable`, one commit per finding
with a reproduction in each. Tests went 333 → 357, green in `build/` and
`build-release/`; the goldens and the 303-preset corpus digests did not move.

| | Finding | |
|---|---|---|
| ✅ | T0.1 preset LFO waveform | `3bffb1cd` |
| ✅ | T0.2 `isValidParamId` | `667ae7ba` |
| ✅ | T0.3 `lexical_cast` overrun, and the interface behind it | `e91fecc3` + |
| ✅ | T0.4 preset file size | `7b09e809` |
| ✅ | T0.5 sample decode bounds | `473da6ee`, `baf18233` |
| ✅ | T1.1 side effects in `LE_ASSERT_MSG` | `76148ce5`, gate in `bcec76e9` |
| ✅ | T1.2 unchecked ring pushes | `5f4da9f3` |
| ☐ | T2.1 – T2.6 threading and lifetime | |
| ☐ | T3.1 – T3.3 GUI use-after-free | |
| ☐ | T4 host protocol | |
| ☐ | doc drift | partly — see below |

**Two of the review's claims did not survive contact**, both marked below:

- The **AIFF denormal sample rate** (T0.5) does not exist. JUCE's reader refuses
  any exponent byte other than `0x40` and truncates the result to an `int`, so
  the smallest rate it can report is about 2 Hz. The bound was kept as a guard on
  a header-supplied number, not as a fix.
- The **`clap_id` in T0.2's worked example** has its bytes the wrong way round.
  `0x000000FF` is a padding byte; the dangerous value is `0x00FF0000`. The
  vulnerability is real and worse than described — it crashes the checked build
  too.

A third is worth recording as a **near miss**: the first attempt at a hostile
channel-count test passed identically with the fix reverted, because a plain
WAV's 32-bit chunk length caps the allocation. It was thrown away rather than
committed. The channel bound is still right — RF64 carries a 64-bit length — but
what it saves is not observable from a test, and the cases say so.

## Context

Two weeks of Opus-driven modernization/porting of a 2016 spectral-effects plugin
to a clap-first design (CLAP + VST3/AUv2/standalone via clap-wrapper). Paul asked
for a full read of `src/` against `doc/tech/`, hunting bugs and memory/threading
problems, plus a prioritized "what next" list to hand to his next Opus sessions.

This was a read-only review. Nothing in the tree was changed. Findings come from
one hand-trace of the threading core plus three parallel review agents
(threading/lifetime, GUI ownership, host-interface/state); every finding marked
**VERIFIED** below I re-checked against the source myself. The plugin genuinely
works — testers ran it in DAWs on three platforms — so nothing here is "it's
broken"; it is "here is where robustness is still thin, ranked."

The single most important structural fact the review turned up:
**`LE_ASSERT_MSG` does not evaluate its argument in release builds**
(`assert.hpp:54-58`, `NDEBUG` arm → `static_cast<void>(0)`). Several of the worst
findings are debug-only guards standing in front of release-build hazards, and
the two ring pushes wrapped in that macro are simply *gone* in every shipped
build. That one macro is the thread running through C1, C2, C3, T1, H1, H2.

---

## Tier 0 — memory-unsafe, reachable from data a host/file supplies

These are the ones to fix first. Each is an out-of-bounds access or an
indirect call driven by untrusted input, live in release builds.

### ✅ T0.1 — Preset/session LFO waveform indexes a function-pointer table unchecked, on the audio thread — VERIFIED
`presets.cpp:684-689` loads every LFO sub-parameter (`wfrm`, `sync`, `ph`,
`lbnd`, `ubnd`) through `adjustValueFromPreset` (identity for all but
`PeriodScale`) into `Parameter::setValue`, whose range check is an assert only
(`parameter.hpp:206`). `wfrm` is `EnumeratedParameter<11>`; `lexical_cast` will
produce 0–255. Once the chain is published,
`ModuleParameters::updateEffectParametersFromLFOs` calls
`LFOImpl::getWaveformAmplitudeForPosition` every block for each enabled LFO, which
does `lfoFunctions[waveForm()](…)` on an **11-entry** table with no bound
(`lfoImpl.cpp:343-357`, `waveForm()` returns the raw stored value). A
double-clicked `.swp` or a restored session with `wfrm="200"` → indirect call
through arbitrary `.data.rel.ro` **on the audio thread**, first block after load.
Secondary: `ph`/`lbnd`/`ubnd` out of range feed `LE_ASSUME(position>=0 && <=1)`
in the waveform functions → false `__builtin_assume` → UB.
**Fix:** clamp/reject in `doLoad` (or give `adjustValueFromPreset` a real range
gate for the enumerated/bounded sub-parameters). Add tests writing out-of-range
`wfrm/ph/lbnd/ubnd/sync`.

**Done — `3bffb1cd`.** Gated in `LFODataLoader::doLoad`, which is the check
`ParametersLoader::operator()` already made for every *other* parameter in a
preset. An out-of-range value is treated as an absent one and the parameter goes
to its default rather than being clamped: there is no nearest waveform, and both
cases say the same thing about the file. A third case pins that a legal LFO block
survives intact, because a gate that threw every LFO attribute away would satisfy
the two hostile ones and flatten the shipping banks.

### ✅ T0.2 — `isValidParamId()` validates only the 8-bit discriminator; the other 24 bits index jump tables guarded by `__builtin_assume` — VERIFIED
`spectrumWorxCLAP.cpp:455-467` returns `type() <= LFOParameter` and checks
nothing else. `Global::index`, `LFO::lfoParameterIndex`, `LFO::moduleParameterIndex`
are full bytes decoded from the `clap_id` and dispatched through
`invokeFunctorOnIndexedParameter` → `typeList.hpp` `cases[index]`, whose only
guard is `LE_ASSUME` (release: `__builtin_assume`, no check). Reachable from all
four param entry points that take a raw `clap_id`: `paramsValue`,
`paramsValueToText`, `paramsTextToValue` (`[main-thread]`) and `handleEvent`
(`[audio-thread]`). `liveRanges()` even *retries with `nullptr`* on an empty
slot, disabling the one partial guard. `paramsValue(0x000000FF,…)` → `cases[255]`
on a 6-entry table → OOB read + indirect call. Manifests from a stale/bogus
automation id in a project or a fuzzing/validating host.
**Fix:** validate the sub-indices in `isValidParamId` (it is the one choke point
before all four entry points; everything downstream assumes it happened).

**Done — `667ae7ba`.** Every field, padding bytes included, and no unreachable
default on the type switch — telling the optimiser that 252 of 256 discriminators
cannot occur is the thing being fixed. Two corrections to the finding: the worked
example has its bytes reversed (`0x00FF0000`, not `0x000000FF`, which is a padding
byte), and it is worse than described — global index 6 of 6 crashes the *checked*
build too, inside the dispatch rather than at any guard. A second case requires
every id `paramsInfo` advertises to still be accepted, since over-rejecting is the
silent failure from here: a host's automation lane just stops answering.

### ✅ T0.3 — `lexical_cast(double, decimalPlaces, char*)` reads and writes past the caller's buffer on truncation — VERIFIED
`lexicalCast.cpp:82-104`: `snprintf` is bounded to `RequiredStringStorage<double>`
= 17, but the trailing-zero trim uses `charactersWanted` (the length snprintf
*wanted*, unbounded) as the cursor origin. `phaseString`
(`spectrumWorxEditor.cpp:2085-2092`) passes the LFO `Phase` value into a 32-byte
stack array; with `ph="1e30"` (reachable via T0.1) `%.1f` wants ~35 chars →
`pEnd = buffer+35`, trim reads `buffer[34..]`, `*pEnd='\0'` writes there, then
`strcpy(&buffer[33],"%")`. **Stack buffer overflow.** Refutes the tech_debt entry
claiming release "truncates rather than overruns" — the overrun is in the trim,
not the snprintf. Same latent shape at `presets.hpp:419-427` (`makeString` builds
a `std::string` from the *wanted* length → over-read into the saved file).
**Fix:** clamp the cursor to the actually-written length; use `std::to_chars`.

**Done — `e91fecc3`.** The rendering goes into a scratch buffer wide enough for
`%f` of any double, so the trim runs over something that cannot have been
truncated, and the caller sees a result only once it is known to fit. Measured in
Release before the fix: a returned length of **312** against a string of 16, and
thirteen breaches of the guard bytes behind a 17-byte buffer. A value too wide for
the constant prints as `%g` — fewer significant digits, but the right number,
inside the buffer, and something `strtod` reads back; truncated `%f` would have
been none of those. The closing assertion compared the readback as a `float`,
which is `inf - inf` for anything over `FLT_MAX` and so a NaN that made the whole
comparison vacuous; it compares in double now. The `tech_debt.md` entry is
corrected to what is genuinely left, which is that the size is a constant and not
a parameter.

### ✅ T0.4 — `readPresetFile` truncates file size to 32 bits, then `presetSize + 1` can wrap to 0 — VERIFIED
`presetStorage.cpp:45-62`: `auto const presetSize = static_cast<unsigned int>(fileSize)`
then `new char[presetSize + 1]` and `stream.read(ptr, presetSize)`. A 4 GiB−1
file → `new char[0]` + a 4 GiB read = heap overflow; any file >4 GiB is silently
read as `size mod 2³²` and parsed as complete. Browser file path only (stateLoad
uses `readWholeStream`). **Fix:** keep `uintmax_t`, reject sizes over a sane cap.

**Done — `7b09e809`.** Checked before the narrowing, against a 16 MiB cap — the
largest of the 303 shipped presets is 2,240 bytes and the format has no element
that grows with anything but a module count capped at five. `readWholeStream`, the
session half that the review filed under Tier 4, is held to the same number: its
only exit was the stream saying it was done, and `stateLoad` is `noexcept`, so
growing until the allocation threw was `std::terminate`. Confirmed by reversion.

### ✅ T0.5 — Sample decode: unbounded channel count / resample ratio can `std::terminate` a `noexcept` load — VERIFIED mechanism (agent; AIFF arm SUSPECTED)
`sample.cpp:184-210` bounds frames (100 M) but not `numChannels` (a 1000-channel
header × 100 M frames = a 400 GB `juce::AudioBuffer` → `bad_alloc`) and bounds
`reader->sampleRate` only `>0` (AIFF's 80-bit rate can be ~1e-300 → ratio→0 →
`targetFrames` overflow → UB). The throw propagates out through `Sample::load` →
`setNewSample` → `GUI::loadPreset` → `stateLoad`, which is **`noexcept`** →
`std::terminate`: the host dies opening a project. **Fix:** bound `numChannels`
and `sampleRate`, compute `targetFrames` in double and reject before the cast,
and wrap the load body so `stateLoad` cannot terminate.

**Done — `473da6ee` (bounds) and `baf18233` (the `noexcept` boundary).** Channels
are bounded to the two that are actually read, which is a bound rather than a cap
and so has no number to tune wrong. The reachable arm turned out to be a
*legitimate* file rather than a hostile one: the length guard ran before
resampling, so an 8 kHz recording against a 768 kHz engine — ninety-six times its
own length — got past it. Measured: with that bound off, an 8 KB file loads
successfully by allocating 3.1 GB. **The AIFF arm is refuted** — see the status
note at the top. `stateSave` and `stateLoad` are function-try-blocks now, verified
by reversion: without the handler the case does not fail, it aborts the run.

---

## Tier 1 — release-build correctness holes (silent leaks / desync)

### ✅ T1.1 — `retire()` and `publishModuleMove()` push through `LE_ASSERT_MSG`, which is a no-op in release — VERIFIED
`spectrumWorxCLAP.cpp:1223`, `publish.cpp:77`. In every shipping build the pushes
never happen. Consequences while the plugin is active:
- **`retire()` leaks everything the audio thread hands back** — each displaced
  module, each chain displaced by a preset load (whole rack per browse), each
  swapped-out decoded sample. Unbounded growth, invisible to the checked-build
  suite.
- **`publishModuleMove()` never reaches the engine** — dragging a module reorders
  the UI and the saved state but not the audio. Permanent silent desync.
**Fix:** `if (!push(...)) { handle }` at both sites. Then a repo rule: no side
effects inside `LE_ASSERT*`. (The review's grep found exactly these two in
`src/`; make it a CI check.)

**Done — `76148ce5`, with the CI gate in `bcec76e9`.** Both pushes hoisted out of
the assertion, following the `if (push(...)) return;` idiom `publishSlot` and
`publishChain` beside them already used. The gate scans 799 assertions in about
two seconds and names both originals, at their exact lines, when run against the
parent commit. It reads the sources rather than the compile lines, so it also runs
under the generators the other two gates skip.

### ✅ T1.2 — Unchecked ring pushes elsewhere desync the two Program copies silently
`handleEvent`'s `toUI_.push` echo (`:798`), `chainChanged()` (`:1234`),
`editParameter` (`:1360`), `queueUnexportedLFOParameter`
(`spectrumWorxEditor.cpp:2438`), and `publishSample` records `sampleFile_`/
`decodedSampleRate_` *before* its push can fail (`:1884-1898`, so state names a
sample the engine never got). `programMain_` (which answers `paramsValue`/
`stateSave`) drifts from the engine with no diagnostic and no recovery. A full
`toUI_` ring (1024) also drops `Retire` — see T2.3.
**Fix:** check every push; on echo-drop set a sticky "resync programMain_ from
engine" flag; move `publishSample`'s bookkeeping after the push (publishSlot/
publishChain already undo correctly).

**Done — `5f4da9f3`.** Every push goes through one place and every drop is
counted; the three `publish*` functions answer whether the engine got the change
rather than answering nothing. `publishSample`'s bookkeeping moved after the
handover — it recorded `sampleFile_` first, so a dropped sample load left the
session naming a file the engine never received. The editor's last bare
`toEngine().push()` now goes through the interface, which `editorHost.hpp` already
argued for.

Deliberately a counter and **not** an assertion, which is a departure from the
`LE_ASSERT_MSG(false, ...)` at the two publish.cpp sites: an assertion answers
differently in a checked build and a shipped one — the family of defect this whole
branch removed — and it makes the behaviour untestable, because a case that fills
a ring on purpose aborts instead of measuring. The counter is not a repair, and
the cases assert the resulting divergence as well as the count. `tech_debt.md`
carries the lossless-echo design that would make it unreachable, and why that is a
decision rather than a patch.

---

## Tier 2 — threading / lifetime races (UB; not yet exercised by tests)

### ☐ T2.1 — Stale-sized chain installs after a spectral restart (heap corruption) — VERIFIED (hand + agent)
Preset changes FFT size while active → new chain built against the **old**
storage factors, queued via `publishChain`. `presetChangeEnd` requests restart;
if the host restarts before the next `process()` drains the ring (Logic, parked
transport, is the documented no-`process()` host), `deactivate()` resizes only
the **live** chain, `activate()` drains nothing, and the first `process()` splices
in modules sized for the old FFT with no resize (`spectrumWorxCLAP.cpp:1159`,
contract at `spectrumWorxCore.hpp:186` unenforced). Larger FFT ⇒ per-bin channel
state writes past its heap block. `engineOwnershipTests.cpp:199` pins only the
no-queued-chain halves.
**Fix:** drain `toEngine_` in `deactivate()` after `suspend()` (main thread owns
the engine there), before `applyPendingSpectralSetup()`. Closes T2.4 too.

### ☐ T2.2 — Host writing a slot selector FREES on the audio thread — VERIFIED mechanism
`handleEvent` → `AutomatedModuleChain::setParameter`
(`automatedModuleChain.hpp:129,137`): `remove()`/`insertAtAndReplace()` drop the
chain's reference; the displaced module's last reference is the local iterator,
released **inside `process()`** → `delete` + `HeapSharedStorage` free on the audio
thread, bypassing the Retire protocol. tech_debt records only the *allocation*.
Strips can't save it (they hold `programMain_`'s modules). Failed `initialise()`
destroys the fresh module on the audio thread too.
**Fix:** take a reference before unlinking and hand the module to `retire()` —
the three lines `installModuleInSlot` already has (`spectrumWorxCore.cpp:530`).

### ☐ T2.3 — Preset load writes the engine's six global parameters from the main thread — VERIFIED
`GlobalParameterUpdater` (`presetLoading.cpp:47-58`) writes the engine's
`InputGain`/`OutputGain`/`MixPercentage` via the one `setGlobalParameter` arm with
no `currentThreadMayMutateEngineState()` assert (`spectrumWorxCore.hpp:350-363`),
racing `process()`'s per-block reads. Normal browser load with audio running.
Violates §2 rules 1–2. **Fix:** route non-spectral globals over
`ToEngine::SetBaseParameter` when `engineIsRunning()`, or at minimum add the
missing assert (would have caught it).

### ☐ T2.4 — `toEngine_` never drained at `deactivate()`/destruction — VERIFIED
`~SpectrumWorxCLAP` drains only `toUI_` (`:222`). Queued `SetSlot`/`SwapChain`/
`SwapSample` payloads leak at destruction, and stale commands replay on top of
newer direct-applied state after reactivation. T2.1's fix closes this.

### ☐ T2.5 — Cross-thread flags are plain `bool`s — VERIFIED
`spectralSetupPending_` (`spectrumWorxCore.hpp:494`), `restartRequested_`
(`spectrumWorxCLAP.hpp:540`) are written and read across threads;
`restartRequested_` as a test-and-set gating `request_restart()`, so a lost
update can **drop the restart** (engine one FFT size, parameter another).
`blockAutomation_` (`host2Plugin.hpp`) is a non-atomic bool whose assert reads it
cross-thread. **Fix:** `std::atomic<bool>` + `exchange`.

### ☐ T2.6 — Double-`activate()` reallocates the working set under a live audio thread — agent-traced
No re-entry guard (`spectrumWorxCLAP.cpp:244`); with `MisbehaviourHandler::Ignore`
a misbehaving host re-enters and `initialise()`→`resize()` reallocates
`sharedStorage_` while `process()` reads it. One line: `if (engineRunning_) return true;`.

---

## Tier 3 — GUI use-after-free (message thread)

### ☐ T3.1 — Popup-menu callbacks UAF on editor close — VERIFIED mechanism
`PopupMenu::showAt` (`gui.cpp:581-599`) captures raw `this` and writes
`menuActive_=false` in the async callback. `dismissAllActiveMenus()` only
**queues** dismissal (JUCE `triggerAsyncUpdate`), so the callback runs a
message-loop turn after `~SpectrumWorxEditor`; for `PopupMenuWithSelection` it
also walks the destroyed `items_` vector — all before the inner `SafePointer`
guards. Owners that die under an open menu: module combo (ejected/resynced),
LFO type, sample-area menu, settings combos, editor close. **Fix:** guard at the
outermost lambda with a SafePointer/alive-token, not two layers in.

### ☐ T3.2 — `detachFrom()` guards on the wrong pointers (the 608f0773 bug class, second instance) — VERIFIED code
`spectrumWorxEditor.cpp:1280-1281` keys on `pActiveControl_`/`pSelectedModule_`,
but deactivation is deferred and clears those first, leaving
`sharedModuleControls_`/`lfoDisplay_` alive with raw back-pointers
(`ModuleControlBase::pModuleUI_`, `LFODisplay::pModuleControl_`) into a strip
`detachFrom` is about to free. Trigger: `drainEngineEvents()` resyncs
**synchronously** from `onMainThread()` (`:1339`). Then paint (`ModuleKnob::paint`
→ `moduleUI().pModule_`) or mouse delivery to the disabled control derefs the
freed strip. **Fix:** guard on the pointers that actually dangle. Test: select →
deactivate → resync before pumping → pump+paint under ASan.

### ☐ T3.3 — smaller GUI hazards
`SharedModuleControls` derefs `editor().selectedModule()` behind assert-only
guards (`auxiliaryComponents.cpp:96,137,309,323`) → null deref in release inside
T3.2's window and during editor member destruction;
`getIndexForModule()` never terminates in release for a module that left the
chain (`moduleChainImpl.cpp:132-142`), reachable via a focus change on a ghost
strip; `PresetBrowser::refreshAndSelectPreset` uses `findPreset()`'s `end()`
unchecked → OOB `juce::Array` read on a name mismatch (NFD/Windows munging);
`updateActiveControlValue()` derefs a possibly-empty optional under `catch(...)`
(`:859-873`); shim `guiCreate`+`guiDestroy` with no parent leaves a live editor
`pEditor_` still points at, and a second create/close then clears `pEditor_` for
the *new* editor (rack/automation updates stop). `clapJuceShim_` declared before
the rings/`pEditor_` — reorder so an editor surviving to `~SpectrumWorxCLAP` is
torn down before the state it reads.

---

## Tier 4 — host-protocol correctness (visible, not unsafe)

- ✅ **VST3 hides every module/LFO parameter forever** — VERIFIED. The design
  leaned on `CLAP_PARAM_IS_HIDDEN` being a live RESCAN_INFO flag
  (`spectrumWorxCLAP.cpp:507-515`), but the shipped clap-wrapper maps it once at
  construction and `RESCAN_INFO` re-reads only the name
  (`wrapasvst3.cpp:1253-1269`); flags re-read only under `RESCAN_ALL`, which the
  plugin can't send while active. So a VST3 host's automation list shows 6
  globals + 5 selectors and nothing else for the instance's life. CLAP and AUv2
  are correct.
  **Done — the flag is simply not used.** The review called this a decision
  rather than a commit and Paul made it: nothing is ever hidden, so all 388 rows
  are there whether or not a slot currently owns them. A flag whose only value is
  that it changes is not worth having in a format that cannot see it change, and
  the failure mode it was buying — an automation lane a user cannot find at all —
  is far worse than a long list. An unused parameter still answers: it reads
  `N/A` and refuses writes.
  The tests that used the flag as their oracle for "no effect owns this" now ask
  the id or the display instead, which is what a user sees; one of them asserts
  outright that nothing is hidden, and another that a slot being filled does not
  move any flag at all — because a flag that moves is one a VST3 host was never
  told about.
- ✅ **`activate()`'s sample-rate re-read is dead in the case it was written for**
  — VERIFIED. `Sample::load` stores the *requested* rate, and the ordinary
  construct→stateLoad→activate order loads with `sampleRate_==0`, so
  `decodedSampleRate_` stays 0 and the `!=0` re-read guard never fires
  (`spectrumWorxCLAP.cpp:285-294`, `sample.cpp:128`). Sample plays at the wrong
  rate — the exact 2016 bug the note claims fixed.
  **Done.** What decides it is whether a sample is loaded at all and then whether
  what it was decoded for is what the engine now runs at; zero answers that the
  same way any other mismatching rate does. Pinned by a case that restores state
  into an *inactive* plugin and then activates — the order every host uses and
  the one nothing tested — which reports `0 == 48000` against the old guard.
- ✅ **A session naming a missing sample keeps the previous one and re-saves it**;
  a session with no `<p n="Sample">` doesn't clear the sample (the one global
  that doesn't reset) (`spectrumWorxCLAP.cpp:1850`, `presetLoading.cpp:130`).
  Also `stateLoad` can pop a modal dialog during unattended restore.
  **Done.** Both cases clear the sample, so nothing of the previous session
  survives into this one or into the next save — the review's "re-saves it" is
  the sharper half, and both new cases check the re-saved bytes for the old name.
  A file that will not load is reported as a `PresetProblem` rather than shown,
  which is the channel this project already built for "something is wrong with a
  preset, and the caller decides whether to interrupt".
  The dialog is *half* fixed, deliberately: `setNewSample` returns its error
  instead of raising a box, so the only caller that raises one is the editor's
  file menu. `GUI::loadPreset`'s own summary still can, when a window happens to
  be open during a restore. That is asserted against (`GUI::UnattendedLoad`) and
  recorded in `tech_debt.md`, per Paul's "make that an assert for now".
- **Unbounded module count** in a preset/state (`presets.cpp:562-596`, only a
  debug assert); `ModuleChainBase::size()` truncates to `uint8_t` at 256.
- ✅ **Locale-dependent float I/O** (`%.9g`/`strtod`, no `"C"` imbue) — a
  comma-decimal host writes unreadable presets and reads every factory float as
  its integer part.
  **Done — `99d74bc1`.** Both halves, and the reading half is the one that loses
  a user's work: reproduced under `de_DE.UTF-8`, `0.75` read back as **0** and a
  1234.5678 round trip came back as 1.0, silently, because stopping at the point
  is not an error. Writing goes through a stream imbued with
  `std::locale::classic()`; reading keeps `strtod` and hands it a `"C"` locale
  (`strtod_l`/`_strtod_l`), because `num_get`'s character set is specified
  without `i` or `n` in it and this plugin prints `inf` for a gate minimum, so
  `>>` was not an option. `std::to_chars` was the first answer and is not
  available: its floating point half is a libc++ dylib symbol introduced in
  macOS 13.3 and this ships to 10.15 — the integer overloads do use it. Text is
  byte-identical, verified across the magnitudes and both infinities before the
  change, and the corpus digests did not move.
  *The test harness had the same bug*: `presetCorpus.txt`'s digests are hashed
  from `%.6g`, so all 303 committed rows were a statement about the locale of the
  machine that generated them.
  Three cases: the conversions, the whole corpus read under such a host, and a
  preset saved under one and read back outside it.
- ☐ The rest of what this bullet had swept together, none of it locale: · a
  `CLAP_PARAM_REQUIRES_PROCESS` used for a "meta" meaning the flag doesn't have ·
  a failed `updateEngineSetup()` rolling back only the engine copy, leaving
  `programMain_`/state disagreeing · a Save-As uniquifier that can spin at ≥100
  same-named presets. (`readWholeStream` uncapped was closed by `7b09e809`.)

---

## Doc drift (worth a pass so the docs stay the source of truth)

threading_model.md: §1 "each strip holds an `IntrusivePtr<Module>` into the
engine" — strips hold **programMain_'s** modules now, so the "can't reach zero on
the audio thread" argument is void (and T2.2 breaks it anyway); §3 table lists 5
ToEngine kinds, there are 6 (`SetUnexportedLFOParameter` missing); §5 "installs
its chain in `activate()`" — nothing does (T2.1); §5 slot-selector exception is
"an allocation" — it's also a free (T2.2); §5 "two things ask for the resync" —
a third does, synchronously, which is the precondition for T3.2; §7 marks ✅ rows
that aren't (ToEngineQueue teardown, spectral flags, module chain), and omits
`SkinLifetime::liveEditors_`, `PresetLoadReport`, and the raw editor
back-pointers; §7 `menuActive_` ✅ despite T3.1; §8 "six assertion sites" — nine,
and the T2.3 arm has none.

parameter_system.md §5–6 cite `src/le/plugins/vst/` and `au/` at six line numbers
— **those backends are deleted**; all of §6 describes machinery that's gone. The
"286-row table" note contradicts §3's 388 and the actual file. Several `:line`
citations stale (constants.hpp:28→21, spectrumWorxCore.hpp:163→267,
automatedModuleChain, plugin2Host).

streaming_format.md §5 says `stateSave` writes `program_` — it writes
**`programMain_`** (`:1662`), which is the entire point of the split; and
"stateLoad… that is the whole of it" omits the rescan + possible dialog/sample
load.

tech_debt.md `RequiredStringStorage` entry's "release truncates rather than
overruns" is false (T0.3, and the `makeString` over-read).
✅ **Done — `e91fecc3`, completed in the commit after it.** The overrun was fixed
first; then the interface itself: every binary-to-string overload takes a
`std::span<char>` rather than a bare pointer, so the bound is the caller's real
buffer and the constant is no longer a contract anybody can fail to honour. All
13 call sites hand over the buffer whole. The entry is rewritten to the one
property left, which is that a value too wide for a *display* prints compactly.

✅ Two more tech_debt.md entries moved with this branch. *An out-of-range LFO
sub-parameter index asserts instead of being dropped* is **closed** by `667ae7ba`
— that id is refused at the choke point now — and is deleted rather than
annotated, per the file's own rule. *`UIEdits` drops on full* is amended: the drop
is counted as of `5f4da9f3`, so it is no longer silent; not dropping it is what is
still owed. A new entry records that a counted drop is not a repaired one.

effect_contract.md §3 inventory (57 effects, order, groups, cmake) — **all
correct**, only ±1 line drift. streaming grammar spot-checks correct.

---

## Verified sound (so you know these were checked, not skipped)

SPSCQueue + ValueMailbox memory ordering; intrusive refcount atomicity
(acq_rel); IntrusivePtr copy/move/self-assign; no lock/alloc/IO on the audio
path except the recorded slot-selector exception (+ T2.2's frees); sample-swap
single-ownership (modulo T1); stack buffers FFT-bounded; publishSlot/publishChain
refusal handling; MissingParameter handling (nothing stale survives except the
sample); all five rescan routes; format-version detection and legacy-name repair;
round-trip with spectral-pending; the effect inventory and streaming grammar.

---

## Suggested order for the Opus sessions

~~1–3 are done~~ — see the status table at the top. What is left, renumbered so
the next session can start at the top:

1. **T2.5** — atomics for the two or three flags. Moved up from 6: it is
   mechanical, and doing it first means the tsan run that validates the rest of
   Tier 2 is not reporting the flags instead of the races.
2. **T2.1 + T2.4** — drain `toEngine_` in `deactivate()`; closes the corruption
   window and the shutdown leak together. Add the FFT-change-while-running test.
3. **T2.6** — the `activate()` re-entry guard. One line.
4. **T2.2** — retire the displaced module on the slot-selector path.
5. **T2.3** — route preset globals through the protocol (or add the assert).
6. **T3.1 + T3.2** — the two GUI UAF classes; add the deferred-teardown ASan test
   the suite is missing.
7. **T3.3** — the smaller GUI guards.
8. **T4 + doc drift** — the VST3 `IS_HIDDEN` question is a *decision* and not a
   commit: the review concludes there is no plugin-side fix, but declining to use
   the flag dynamically is one, with a real cost (a VST3 automation list of 388
   rows against 11). It also rests on a claim about the pinned clap-wrapper that
   nobody has re-checked. Then the sample-rate re-read, stale-sample-on-load, and
   the doc-sync pass last so it describes the fixed tree.

The original order, for the record:

1. **T1.1** — `LE_ASSERT_MSG` → checked push at the two sites, plus a CI grep
   rule. Two lines; the difference between a correct and a leaking/desyncing
   release build. (One session, mostly the CI rule and a leak test.)
2. **T0.1 + T0.2** — the two audio-thread-reachable OOB indirect calls from
   file/host data. One guard each; both want new hostile-input tests. Highest
   safety payoff.
3. **T0.3 + T0.4 + T0.5** — the buffer/size/decode bounds. T0.3 and T0.5 both
   want the fix even though a full repro needs T0.1/hostile files.
4. **T2.1 + T2.4** — drain `toEngine_` in `deactivate()`; closes the corruption
   window and the shutdown leak together. Add the FFT-change-while-running test.
5. **T2.2** — retire the displaced module on the slot-selector path.
6. **T2.5** — atomics for the two/three flags (quick, unblocks a real tsan run
   of the preset-load-during-process case).
7. **T2.3** — route preset globals through the protocol (or add the assert).
8. **T3.1 + T3.2** — the two GUI UAF classes; add the deferred-teardown ASan
   test the suite is missing.
9. **T2.6, T1.2, T3.3** — the smaller races/guards.
10. **T4 + doc drift** — VST3 IS_HIDDEN (upstream issue), sample-rate re-read,
    stale-sample-on-load, then a doc-sync pass. Do the doc pass last so it
    describes the fixed tree.

A standing rtsan run (`threading_model.md §8` recipe) over a case that loads a
preset against a live process loop would independently catch T2.1–T2.4; it's the
one instrument the suite hasn't pointed at this surface yet.
