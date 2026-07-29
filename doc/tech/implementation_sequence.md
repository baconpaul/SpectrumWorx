# SpectrumWorx — Implementation Sequence (CLAP-first)

Companion to [`initial_scan.md`](initial_scan.md), which is the analysis. This
is the plan: a stage-by-stage path from the 2016 tree to a modern three-OS
plugin, each stage ending in something you can build, test and commit.

**The decision this document assumes** (scan §1.6): SpectrumWorx becomes a
**native CLAP**, and [clap-wrapper](https://github.com/free-audio/clap-wrapper)
emits the VST3, AUv2 and standalone builds from it. JUCE 8 is used **only for
the GUI**, consumed in modules-only mode. `juce::AudioProcessor` never enters
the picture.

The reference implementation of this idiom in the Surge family is
**shortcircuit XT** (`~/dev/music/sst/scxt-2`), and
[six-sines](https://github.com/baconpaul/six-sines) is the small worked example
that clap-wrapper's own docs point at. OB-Xf remains the reference for
repo layout, CI and installers — just not for the host layer.

---

## Why this shape

Three facts from the scan drive everything below.

1. **SpectrumWorx's parameters are dynamic.** 50 fixed slots whose name, range,
   unit and meaning change when you load a different effect into a module. CLAP
   models this natively (`clap_host_params::rescan`); the existing framework
   already models it (`useDynamicParameterLists()`, `Program const*` threaded
   through every getter). These two designs agree. `AudioProcessorParameter`
   disagrees with both.
2. **`SW::ParameterID` is already a `clap_id`.** `core/parameterID.hpp:27-64` is
   a packed `uint32` of `{type, moduleIndex, moduleParameterIndex,
   lfoParameterIndex}`. It goes into `clap_param_info.id` with zero translation.
3. **`Plugin2HostInteropControler` is already a CLAP adapter interface.** Eleven
   pure virtuals, every one of which maps onto a CLAP host extension (scan
   §1.6.1). The work is writing one backend, not rewriting the plugin.

What CLAP-first does *not* buy you: the GUI work is identical either way. 11.3 k
lines of `juce_gui_basics` still need porting to JUCE 8, and the owned-window
model still needs collapsing. That is stage 6 and it is the largest single
stage regardless of format.

---

## Target end state

A checklist to grade against. When all of these are true, the project is done.

**Build**
- [ ] Top-level `CMakeLists.txt`; `source/CMakeLists.txt` gone
- [ ] C++20, no compiler-specific flag zoo, no `$LEB_3rdParty_root`
- [ ] Every dependency a git submodule under `libs/` or a vendored single file
      under `cmake/`
- [ ] No generated file written back into the source tree
- [ ] Configures and builds on CMake ≥ 3.28 with Ninja, MSVC, Xcode

**Formats and platforms**
- [ ] CLAP, VST3, AUv2, Standalone — all produced by `make_clapfirst_plugins`
- [ ] macOS universal (arm64 + x86_64), Windows x64 + arm64, Linux x64 + arm64
- [ ] `clap-validator validate` clean
- [ ] Loads and passes `auval` / VST3 validator / a manual pass in Reaper,
      Bitwig, Logic, Ableton, FL

**Code**
- [ ] Zero `#include <boost/…>` anywhere under `src/`
- [ ] Zero NT2; zero VST2; zero licence manager; zero FMOD/Unity
- [ ] No Carbon, no `FSRef`, no DirectShow, no `SetWindowsHookEx`
- [ ] No separate desktop windows — one editor view
- [ ] All source UTF-8, GPL/AGPL headers, `.clang-format` clean
- [ ] Golden DSP tests for all 57 effects, green on all three OSes

**Ship**
- [ ] `basic_installer` producing signed/notarised dmg, Inno Setup exe, tar+deb
- [ ] Nightly and release workflows
- [ ] `THIRD_PARTY.md`, README, converted manual

---

## Target repo layout

```
CMakeLists.txt              project(SpectrumWorx) VERSION … LANGUAGES C CXX
cmake/
  CmakeRC.cmake             vendored single file (resource embedding)
libs/
  CMakeLists.txt            all add_subdirectory()s live here
  JUCE/                     submodule, tag 8.0.12
  simde/  fmt/  pffft/  catch2/
  clap/{clap,clap-helpers,clap-wrapper}/
  sst/{sst-basic-blocks,sst-clap-helpers,sst-cmake,sst-cpputils,sst-plugininfra}/
src/
  CMakeLists.txt            the sw-impl static library
  clap-first/
    CMakeLists.txt          make_clapfirst_plugins(...)
    sw-clap-entry.cpp       ~40 lines
  spectrumWorx.{hpp,cpp}    core plugin (was source/)
  spectrumWorxCLAP.{hpp,cpp}
  core/                     host_interop, module chain, parameterID
  gui/
  external_audio/
  le/                       was source/externals/le
    spectrumworx/{engine,effects}
    parameters/ math/ analysis/ utility/
    plugins/{plugin.hpp, clap/}
  nt2_static_fft/           retained NT2 static_fft (reference for stage 4)
assets/
  CMakeLists.txt            cmrc_add_resource_library(sw-skin) — stage 6.3
  skin/                     59 PNGs + 2 fonts, was installer/…/Resources
  presets/                  15 factory banks
  samples/
tests/
  CMakeLists.txt            sw-tests (Catch2)
  golden/                   committed DSP fixtures
tools/
  show-ui/                  sw-show-ui, the stage 6 GUI harness
scripts/
doc/
  tech/                     these documents
  manual/
.github/workflows/
```

---

## Dependencies

```sh
git submodule add https://github.com/juce-framework/JUCE                    libs/JUCE
git submodule add https://github.com/free-audio/clap                        libs/clap/clap
git submodule add https://github.com/free-audio/clap-helpers                libs/clap/clap-helpers
git submodule add https://github.com/free-audio/clap-wrapper                libs/clap/clap-wrapper
git submodule add https://github.com/surge-synthesizer/sst-clap-helpers     libs/sst/sst-clap-helpers
git submodule add https://github.com/surge-synthesizer/sst-basic-blocks     libs/sst/sst-basic-blocks
git submodule add https://github.com/surge-synthesizer/sst-plugininfra      libs/sst/sst-plugininfra
git submodule add https://github.com/surge-synthesizer/sst-cpputils         libs/sst/sst-cpputils
git submodule add https://github.com/surge-synthesizer/sst-cmake            libs/sst/sst-cmake
git submodule add https://github.com/surge-synthesizer/pffft                libs/pffft
git submodule add https://github.com/simd-everywhere/simde                  libs/simde
git submodule add https://github.com/fmtlib/fmt                             libs/fmt
git submodule add https://github.com/catchorg/Catch2                        libs/catch2
```

Then pin JUCE: `git -C libs/JUCE checkout 8.0.12`.

| Submodule | Replaces | Notes |
|---|---|---|
| JUCE 8.0.12 | JUCE 2.1.2 patched fork (29 MB, 276 patched files) | modules only; `juce_gui_basics`, `juce_graphics`, `juce_events`, `juce_core`, `juce_audio_formats` |
| clap / clap-helpers | — | `clap::helpers::Plugin<>` gives you the boilerplate |
| clap-wrapper | `le/plugins/{vst,au}`, CPack/WiX/PackageMaker | VST3 + AUv2 + standalone from the one CLAP |
| sst-clap-helpers | `gui.mm`, `OwnedWindowBase`, the Win32 hook | `make_clapfirst_plugins` glue + `ClapJuceShim` |
| sst-basic-blocks + simde | NT2 / Boost.SIMD (10,134 files, 53 MB) | SIMD helpers, SSE-on-NEON |
| pffft | `nt2/signal/static_fft.hpp` (non-Apple) | Apple keeps Accelerate/vDSP |
| sst-plugininfra | `.paths` file, `boost::mmap`, RapidXML | paths, tinyxml2, version info |
| sst-cpputils | `boost::lockfree`, misc | ring buffers |
| sst-cmake | CPack, WiX, PackageMaker, `buildOptions.cmake` | `include(basic_installer)` |
| fmt | ad-hoc `sprintf` | |
| Catch2 | nothing — there are no tests today | |
| `cmake/CmakeRC.cmake` (vendored) | `resourcesPath()`, disk-loaded skin PNGs | see the note below |

**On resource embedding.** `add_clap_juce_shim` sets `JUCE_MODULES_ONLY ON`, and
in that mode JUCE's top-level `CMakeLists.txt` returns immediately after
`add_subdirectory(modules)`, *before* `include(extras/Build/CMake/JUCEUtils.cmake)` — so
`juce_add_binary_data` does not exist on this route. Use CMakeRC
(`cmrc_add_resource_library`), which is how shortcircuit embeds its assets. It
is one vendored `.cmake` file, no submodule.

**Boost is scaffolding, not a dependency.** It appears in stage 3 via CPM (which
clap-wrapper already pulls in), scoped to Boost.Fusion / MPL / Preprocessor
only, and is deleted in stage 7. It never goes under `libs/`.

---

## The clap-first build, concretely

Three pieces. This is the whole mechanism.

**1. `libs/CMakeLists.txt`** — bring in the dependencies and the shim:

```cmake
add_subdirectory(sst/sst-cmake)
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" PARENT_SCOPE)   # for basic_installer

add_subdirectory(clap/clap         EXCLUDE_FROM_ALL)
add_subdirectory(clap/clap-helpers EXCLUDE_FROM_ALL)

set(CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES TRUE CACHE BOOL "")
set(CLAP_WRAPPER_DONT_ADD_TARGETS      TRUE CACHE BOOL "")
set(CLAP_WRAPPER_BUILD_AUV2            TRUE CACHE BOOL "")
add_subdirectory(clap/clap-wrapper EXCLUDE_FROM_ALL)

add_subdirectory(sst/sst-clap-helpers)
add_clap_juce_shim(JUCE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/JUCE)

add_subdirectory(sst/sst-basic-blocks)
add_subdirectory(sst/sst-plugininfra)
add_subdirectory(sst/sst-cpputils)
add_subdirectory(fmt EXCLUDE_FROM_ALL)
```

**2. `src/CMakeLists.txt`** — the plugin is a **static library** exporting three
symbols. This is where ~100 % of development happens:

```cmake
add_library(sw-impl STATIC
    spectrumWorxCLAP.cpp
    sw-clap-entry-impl.cpp        # get_factory / clap_init / clap_deinit
    …)
target_link_libraries(sw-impl PRIVATE
    sw-dsp                        # the engine + effects (stage 3)
    clap-helpers clap-wrapper-extensions
    clap_juce_shim clap_juce_shim_headers
    sst-plugininfra fmt sw-resources)
target_link_libraries(sw-impl PUBLIC clap)
```

**3. `src/clap-first/CMakeLists.txt`** — one call assembles every format:

```cmake
make_clapfirst_plugins(
    TARGET_NAME       spectrumworx_clapfirst
    IMPL_TARGET       sw-impl
    OUTPUT_NAME       "SpectrumWorx"
    ENTRY_SOURCE      sw-clap-entry.cpp
    BUNDLE_IDENTIFIER "com.littleendian.spectrumworx"
    BUNDLE_VERSION    ${PROJECT_VERSION}
    PLUGIN_FORMATS    CLAP VST3 AUV2
    WINDOWS_FOLDER_VST3 TRUE
    ASSET_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/sw_assets
    STANDALONE_CONFIGURATIONS
        standalone "SpectrumWorx" "com.littleendian.spectrumworx"
)
```

`sw-clap-entry.cpp` is ~40 lines: a `clap_plugin_entry` struct pointing at three
`extern` functions in `sw-impl`. clap-wrapper recompiles that one file per
format and links your static library behind it. See
`scxt-2/src/clients/clap-first/scxt-clap-entry.cpp` for the exact shape.

---

## Stages

Nine stages. Each has a goal, the work, and an explicit **done when** — a
condition you can check, not a feeling.

---

### Stage 0 — Purge and amputate  ✅ *complete*

**~1 week. Nothing compiles at the start of this stage and nothing compiles at
the end. That is the point:** every mechanical, whole-tree change is free while
the build is already broken and the repo has no forks. Doing any of it later
costs a rebuild and an unreviewable diff.

**✅ 0.1 — Rewrite history. Do this before anything else is committed.**

```sh
git filter-repo --invert-paths \
  --path source/externals/le/plugins/vst/2.4/aeffect.h \
  --path source/externals/le/plugins/vst/2.4/aeffectx.h \
  --path-glob 'source/externals/le/license_key/*'
```

Removes the Steinberg VST2 SDK and the committed 4096-bit RSA private key from
every commit (scan §3.3, §8.1). Force-push. Every stage after this assumes the
rewritten history — so it genuinely has to be first.

> **Done.** All 6 commits rewritten; `aeffect.h`, `aeffectx.h` and all 7
> `license_key/` files are absent from every ref. Note that filter-repo removes
> the `origin` remote as a safety measure — re-add it and `push --force` (both
> `main` and `--tags`).

**✅ 0.2 — Tag before you delete.** `git tag attic/2016-import` so nothing is
irrecoverable. Note that filter-repo rewrites tags too, so a tag made before 0.1
will point at the rewritten commit and will *not* preserve the purged files —
which is correct, but means the tag is a safety net for the deletions below, not
for the purge. Three things in the delete list are worth *reading* first:
`effects/_unfinished` (17 half-built effects, several interesting),
`nt2/signal/static_fft.hpp` (appears to be Little Endian's own contribution to
NT2 and may be faster than pffft), and `le/audioio/device/patchedRtAudio`.

**✅ 0.3 — Delete.** Per scan §8.2 — nt2, audioio, licenser, license_key,
`3rd_party/` entirely, `externals/boost/{hash,mmap,filesystem}`,
`le/build/*.cmake`, the iOS/Android toolchains, the WiX/PackageMaker trees.
Roughly 80 % of the files and 90 % of the bytes.

> **Done — 12,918 tracked files → 875.** Three deliberate retentions against the
> list above:
>
> - **`le/plugins/{vst,au,fmod,unity}` kept for now.** They are the reference
>   for the CLAP backend in stage 5 — `vst/2.4/plugin.inl` and `au/plugin.hpp`
>   between them show how the framework's `ParameterIndex` and `ParameterID`
>   selectors are wired, which is exactly what 5.1–5.4 have to reproduce. Delete
>   them once `le/plugins/clap/` works.
> - **`effects/_unfinished` kept** — 17 half-built effects, several worth
>   finishing rather than discarding.
> - **`nt2/signal/static_fft.hpp` and its six `details/` headers kept**, moved to
>   `source/externals/nt2_static_fft/` with the `nt2/signal/…` include path
>   preserved so `le/math/dft/fft.cpp` still resolves. Its header reads
>   "Copyright 2012 - 2015 Domagoj Saric, Little Endian Ltd." over the LASMEA/LRI
>   base under BSL-1.0, so it is substantially LE's own work and may well beat
>   pffft. Its 286-line unit test came along too — currently the only test in the
>   repository. It does **not** compile standalone (it still wants
>   `boost/simd/*`); it is retained as source material for stage 4, not as a
>   build target.

**✅ 0.4 — Excise the licence manager** (scan §3.2) — it is `#if`-gated with a
working `#else`, so this is mostly deleting branches. Includes removing the
Registration tab from the Settings window and re-laying-out the remaining three
tabs.

> **Done.** `LE_SW_AUTHORISATION_REQUIRED` and the `LE_LICENCE_SPECIFIC` macro
> are gone; `rg` for any of the licensing identifiers returns nothing.
> The bulk was one contiguous ~1,000-line block in `spectrumWorx.cpp`
> (licence parsing, RSA verification, the demo-crippling thread, the
> "targetAuthorizationDataAntiHack" games). Beyond the plan:
>
> - The `#else` branch also had to go, not just the `#if`. It was not a no-op —
>   it populated the Registration tab with a hardcoded "Registered to: Everyone
>   / Free" and the About tab with a licence-type string. Those are artefacts of
>   a commercial product, so the whole tab went and the Settings window is now
>   three tabs (`createTabButton` reindexed, tab-count assertions 4 → 3).
> - `SW_IS_RETAIL` and `SW_ENABLE_UPGRADE` existed only to serve licensing;
>   removed from `versionConfiguration.hpp.in` along with the `retailBuild` /
>   `versionUpgradeEnabled` CMake variables that fed them.
> - `SpectrumWorx::blockAutomation()` had an unauthorised-and-headless early
>   return; it now just forwards to `SpectrumWorxCore`.
> - Unused after this: `GUI::licencesPath()`, `demoLimiter.hpp`, and eight
>   `ResourceBitmaps` enum entries (`SettingsReg*`, `Authorize*`, `BuyNow*`).
>   The corresponding PNGs in `assets/skin/` are now orphaned and should be
>   dropped when the skin is embedded in 6.3.
>
> **Known dangling reference:** `spectrumWorx.cpp` and `gui.cpp` still
> `#include "boost/mmap/…"` for the settings and `.paths` files, but 0.3 deleted
> `source/externals/boost`. That is deliberate — the code using it dies in 6.3 —
> but it means these two files cannot compile until then, over and above
> everything else that cannot compile yet.

**✅ 0.5 — Move to the target layout**, in one `git mv`-only commit so it reviews
as a rename:

| From | To |
|---|---|
| `source/` | `src/` |
| `source/externals/le/` | `src/le/` |
| `source/externals/nt2_static_fft/` | `src/nt2_static_fft/` |
| `installer/ProgramFolder/Resources/*.png` | `assets/skin/` |
| `installer/ProgramFolder/Presets/` | `assets/presets/` |
| `installer/ProgramFolder/Samples/` | `assets/samples/` |
| `doc/*.doc`, `*.png`, `readme.txt` | `doc/manual/` |
| `installer/ProgramFolder/{Documents/User's Guide.PDF, ReadMe.rtf}` | `doc/manual/` |
| `installer/`, `3rd_party/` | deleted |

> **Done.** `source/externals/` is gone as a level — `le/` and `nt2_static_fft/`
> now sit directly under `src/`, which is what made the include paths collapse to
> `${PROJECT_SOURCE_DIR}` plus one entry for `nt2_static_fft`. Four `leExternals`
> path variables and the effects-configuration path were repointed to match.
> Also removed the dead `LE_SW_COMPILE_TIME_PROFILING` block, which pointed at
> `externals/boost/profile_templates2` — a directory that was never in this repo
> to begin with (scan §4.1).
>
> **`EULA.txt` was moved to `doc/manual/`, not deleted.** It is the commercial
> end-user licence agreement and it contradicts the repo's GPL-3.0 LICENSE, so
> it should almost certainly go — but that is a licensing decision (§9.3), not a
> file move, so it is preserved and flagged rather than quietly dropped.
>
> **`src/CMakeLists.txt` still contains ~400 lines of CPack/WiX/PackageMaker
> configuration pointing at the deleted `installer/resources/` tree.** Left
> deliberately: stage 1 replaces that file wholesale, and its `SOURCES_*` lists
> are the authoritative record of what was actually in the build — which is
> worth having while writing the new CMake.

**✅ 0.6 — Whole-tree mechanical hygiene**, each in its own commit:

- `iconv -f CP1252 -t UTF-8` every source file (the `©` currently renders as `�`)
- rewrite file headers to the chosen licence (write `scripts/fix_file_comments.py`;
  OB-Xf has an equivalent to crib from)
- strip `#pragma once`, keep the `#ifndef` guards
- `LE_OVERRIDE` → `override`, `LE_SEALED` → `final`
- **delete `LE_FASTCALL`** — it is meaningless on x86-64 SysV and arm64 and will
  otherwise silently follow you onto Apple Silicon
- delete `LE_ASSUME(this != 0)` and friends (UB-adjacent, MSVC-only)
- reduce `le/utility/platformSpecifics.hpp` to the handful of macros that survive
- add `.clang-format` (copy OB-Xf's), run it over the whole tree in one commit,
  and record that commit in `.git-blame-ignore-revs`

> **Done.** 14 CP-1252 files converted; 289 `#pragma once` lines stripped;
> `LE_OVERRIDE`/`LE_SEALED` → `override`/`final`; `LE_FASTCALL` (579 uses) and
> `LE_FASTCALL_ABI` (297) deleted along with their definitions; 19 UB-adjacent
> `LE_ASSUME( this )` removed; 452 file headers rewritten to
> `SPDX-License-Identifier: GPL-3.0-or-later`; 435 files reformatted.
>
> **Verification.** The reformat was checked by lexing every file (comments and
> whitespace stripped, string/char literals preserved) at `HEAD` with the
> intended macro edits applied, and comparing against the working tree. All
> residual differences are accounted for: the hand-edits above, plus
> clang-format's `SortUsingDeclarations` (reorders sibling using-declarations —
> inert) and `BreakStringLiterals` (splits long literals into adjacent literals —
> identical after concatenation). `clang-format --dry-run -Werror` is clean, so
> the tree is format-stable.
>
> Three things worth knowing:
>
> - **`SPDX-License-Identifier: GPL-3.0-or-later` is a choice, not a finding.**
>   It matches the repo's GPL-3.0 LICENSE and SST house style, but §9.3 is still
>   open — if the JUCE 8 question pushes this to AGPLv3, it is one `sed` away.
> - **`src/le/spectrumworx/effects/synth/synth.hpp` needed a manual fix.** It had
>   `Unit< '°' >`, where `°` was a *single* CP-1252 byte (0xB0). Converting to
>   UTF-8 would silently turn that into a two-byte multicharacter literal with a
>   different value — and it is a non-type template parameter, so the type
>   changes with it. Written as `'\xB0'` to preserve the original value. This is
>   the same hazard as `Unit<' dB'>`, fixed properly in 7.3.
> - **The two `.m` files are MATLAB, not Objective-C** (`cepstrum.m`,
>   `phase_locked_vocoder.m` — DSP prototypes). clang-format mangled them as
>   Objective-C; they are reverted and listed in `.clang-format-ignore`.
>   `src/nt2_static_fft/` carries its own `DisableFormat: true` so the retained
>   NT2 code stays diffable against upstream.
>
> **Outstanding:** `.git-blame-ignore-revs` cannot be written in the same commit
> as the reformat, since it must name that commit's hash. It needs a one-line
> follow-up commit.

**✅ 0.7 — Preset backward compatibility: keep the format.** Decided. The XML
schema stays exactly as it is; only the parser changes (RapidXML → tinyxml2,
8.1). The binding consequence is on stage 7: **the parameter-system refactor may
not rename, reorder or retype anything a preset file references.** 7.0's
parameter-table snapshot test is what enforces it.

**Done when:** `git ls-files | wc -l` is roughly 1,000 rather than ~13,000; the
tree contains only code you intend to ship; `git log --all --diff-filter=A --
'*aeffect*'` is empty.

> **Stage 0 complete** — 875 tracked files, all UTF-8, GPL-3.0 SPDX-headed and
> clang-format clean. Nothing compiles, which is expected: the build system does
> not exist yet (stage 1) and `boost/mmap` is still referenced by code that dies
> in 6.3.

---

### Stage 1 — Walking skeleton

**1.5–2.5 weeks.** An *empty* plugin, in all four formats, on all three OSes,
built and packaged in CI. No DSP, no GUI, no SpectrumWorx code at all.

This is the highest-value-per-week stage in the project. It proves the entire
delivery chain — submodules, CMake, clap-wrapper, code signing, notarisation,
installers, the CI matrix — while the thing being delivered is 200 lines you can
debug in an afternoon. Every problem it finds is a problem you would otherwise
have found in stage 9, tangled with 90 k lines of ported code.

**1.1 ✅** Add the submodules above; pin JUCE to 8.0.12.

> Done. Thirteen submodules under `libs/`, JUCE at tag 8.0.12 (`29396c22c9`).
> Note that `git submodule update --init --recursive` resets a submodule to the
> recorded gitlink, so the JUCE checkout has to be re-pinned and `git add`ed
> *after* the recursive init, not before.

**1.2 ✅** Top-level `CMakeLists.txt`, `libs/CMakeLists.txt`, `src/CMakeLists.txt`,
`src/clap-first/CMakeLists.txt` per the sketches above. Version from
`sst-plugininfra/cmake/git-version-functions.cmake`'s
`version_from_versionfile_or_git`.

> Done. The 2016 `src/CMakeLists.txt` is preserved as `src/legacy-build.cmake`,
> included by nothing, as the record of the old targets' sources and defines.
> See "What stage 1 found" below for the CMake ordering constraints that are
> load-bearing.

**1.3 ✅** A stub plugin deriving from `clap::helpers::Plugin<>` that:
- declares the **real** audio ports it will eventually need — stereo main in,
  stereo out, optional stereo sidechain
- exposes a handful of **fake dynamic parameters** and calls
  `clap_host_params::rescan(INFO|TEXT)` on a timer

  Do this on day one. The dynamic-parameter path is the single riskiest
  interaction with hosts, and you want to know *now* which of Reaper, Bitwig,
  Logic, Ableton and FL handle a mid-session rescan gracefully — not after
  stage 5.
- passes audio through unmodified

> Done, and deliberately more than "a handful". `src/stubParameters.{hpp,cpp}`
> reproduces the **whole** 287-entry skeleton — 7 globals, 5 slot selectors,
> 5×10 module parameters, 5×9×5 LFO parameters — and the **real packed uint32
> IDs** from `SW::ParameterID`, so hosts are being handed the exact sparse,
> non-sequential id space the ported plugin will use.
>
> Six fake effects with parameter counts 3–10 sit behind the slot selectors.
> Changing a selector changes each affected parameter's name, its module path
> (`Module 2/Vocoder/LFO`), and whether it is `CLAP_PARAM_IS_HIDDEN` — the real
> slots are ragged the same way. That is exactly the set of changes
> `CLAP_PARAM_RESCAN_INFO` is specified to cover, and no more.
>
> The trigger is the selector itself rather than a timer, which is the real
> path: `process()`/`paramsFlush()` set an atomic flag and call
> `requestCallback()` (coalesced — one callback per outstanding batch), and
> `onMainThread()` does the `paramsRescan(INFO | TEXT)`.

**1.4 ✅** `add_clap_juce_shim` + `ADD_SHIM_IMPLEMENTATION(clapJuceShim)`, with
`createEditor()` returning a `juce::Component` that paints one rectangle.

> Done — `src/stubEditor.{hpp,cpp}`. It paints the rectangle, and adds five
> "cycle this slot's effect" buttons and a bare "rescan now" button, so the
> dynamic-parameter path can be driven by hand from inside a DAW without
> writing automation first.

**1.5 ⏸ deferred.** `include(basic_installer)`; CI modelled on OB-Xf's
`.github/workflows/build-plugin.yml` using
`surge-synthesizer/sst-githubactions/prepare-for-juce` and `install-innosetup`.
Matrix: macos (universal), windows-msvc-x64, windows-arm64, linux-x64 (gcc 12/13/14),
linux-arm64. Add `clang-format-check` from the same action set.

> Take `add_clapfirst_installer` from two-filters'
> `cmake/basic_installer_clapfirst.cmake` rather than shortcircuit's hand-rolled
> `basic-installer.cmake`; note that sst-cmake itself provides only the Inno
> Setup imported target, not an installer module. Copy shortcircuit's
> stage-then-package split (`*-products` then `*-installer`) so the staged
> directory is both the CI artefact and what clap-validator points at.

**Done when:** green CI on every matrix entry; `clap-validator validate` clean;
the `.clap`, `.vst3`, `.component` and standalone all load in Reaper and Bitwig
and show the rectangle; the mac artefact is signed and notarised; the installers
install.

> Current state: all four formats build on macOS arm64 and
> `clap-cpp-validator validate` is **23 passed / 0 failed / 0 warnings**.
> Remaining before this stage closes: Windows and Linux builds, DAW load tests,
> CI, signing/notarisation, installers.

#### What stage 1 found

Things that would have cost real time later, all cheap here:

- **Enable OBJC/OBJCXX at top level, not in a subdirectory.** `sst-clap-helpers`
  does `enable_language(OBJC)` in its own scope; CMake then fails generation
  with `Missing variable is: CMAKE_OBJCXX_COMPILE_OBJECT`. Hoist both
  `enable_language` calls next to `project()`.
- **`cmake_minimum_required(VERSION 3.28)`, not 3.21.** clap-wrapper sets
  `CMP0149`, which is unknown before CMake 3.27 and hard-errors.
- **`add_subdirectory(fmt)` before clap-wrapper**, or clap-wrapper vendors a
  second copy of fmt instead of reusing `fmt-header-only`.
- **`add_library(simde INTERFACE)` by hand.** simde ships no target, and
  sst-basic-blocks silently downgrades to `SIMDE_UNAVAILABLE=1` — x86-only —
  when it can't find one. `sst-cpputils` must precede it for the same reason.
- **No `project()` call in `src/clap-first/CMakeLists.txt`.** It resets
  `PROJECT_VERSION` to empty, which make_clapfirst passes on to
  `set_target_properties` as a missing argument: `set_target_properties called
  with incorrect number of arguments`, from `wrap_standalone.cmake:97`.
- **`target_link_libraries(sw-impl PUBLIC clap)`.** The per-format entry stubs
  are compiled into the clap/vst3/auv2/standalone targets and only see
  sw-impl's *interface*; if `clap` is PRIVATE they cannot find `<clap/clap.h>`.
- **`BUNDLE_IDENTIFIER` is spelled correctly in clap-wrapper 0.15.1.** Older
  call sites in scxt/sapphire/two-filters pass `BUNDLE_IDENTIFER` (one `I`),
  which `cmake_parse_arguments` silently discards. Ours is right; the AU comes
  out as `com.littleendian.spectrumworx.auv2`, `aufx`/`SpWx`/`LiEn`.
- **The standalone's bundle id is hardcoded** to `${BUNDLE_NAME}.standalone` in
  clap-wrapper's `Info.plist.in`; no CMake property overrides it. Ours is
  therefore `SpectrumWorx.standalone`. Fix before notarisation by passing our
  own `MACOSX_BUNDLE_INFO_PLIST`.
- **Clamp on the way in, at the one place a value becomes an index.** The
  validator's `param-range-robustness` segfaulted the first build: a host
  writing outside a selector's declared range reached the effect table
  unchecked. `effectIn()` now rejects anything outside `[0, count)`, and
  `setValue` clamps to the declared range and drops non-finite values. The real
  `AutomatedModuleChain::setParameter` needs the same audit in stage 5.
- **The Rust `clap-validator` 0.3.2 cannot survive `request_callback()` from
  `process()`.** `Host::handle_callbacks_once` holds a `RefCell` borrow across
  `on_main_thread()`, so the re-entrant host call panics
  (`already mutably borrowed`). The call is `[thread-safe]` per `clap/plugin.h`
  and is the standard shortcircuit idiom, so this is a validator defect, not a
  plugin one — confirmed by suppressing only that call and watching the suite
  go green. Use `clap-cpp-validator`, which handles it.

---

### Stage 2 — Boost tier-1 sweep ✅

**1–2 weeks. Runs in parallel with stage 1, and still against a non-compiling
tree** — which is exactly why it is here and not at the end. These are
type-for-type substitutions with no behavioural content; doing them now costs a
sed script and a careful read, doing them in stage 7 costs a rebuild of a
working plugin for each one.

Per scan §5.1 tier 1 — roughly 300 include sites:

| Boost | → | Done |
|---|---|---|
| `boost/assert.hpp` (77) | `assert` / a project macro | ✅ `LE_ASSERT` etc., ~1200 macro sites |
| `boost/config/abi_{prefix,suffix}.hpp` (102) | delete, pure noise | ✅ |
| `boost/range/iterator_range_core.hpp` (21) | `std::span` | ✅ `LE::Utility::Span`, see below |
| `boost/utility/string_ref.hpp` (17) | `std::string_view` | ✅ |
| `boost/smart_ptr/intrusive_ptr.hpp` (12) | keep a 40-line local `IntrusivePtr` | ✅ `le/utility/intrusivePtr.hpp` |
| `boost/noncopyable.hpp` (8) | `= delete` | ✅ 14 classes |
| `boost/polymorphic_cast.hpp` (8) | `dynamic_cast` + assert | ✅ `le/utility/polymorphicDowncast.hpp` |
| `boost/optional` (6) | `std::optional` | ✅ incl. the in-place factories |
| `boost/core/ignore_unused.hpp` (6) | `[[maybe_unused]]` | ✅ `LE::Utility::ignoreUnused` |
| `boost/range/algorithm/*` (~12) | `std::ranges::*` | ✅ |
| endian / cstdint / array / integer / ref / mem_fn / limits / scoped_array (~30) | `<bit>`, `<cstdint>`, `<array>`, `std::ref`, `std::mem_fn`, `<limits>`, `std::unique_ptr<T[]>` | ✅ |
| `boost::signals2::mutex` | `std::mutex` | ✅ |
| `boost::filesystem` (sandbox) | `std::filesystem` | ✅ `PresetBrowser::refresh()` |
| *(not in the scan)* `boost::spirit` karma/qi | the CRT | ✅ `lexicalCast.cpp`, MSVC-only path |

`scripts/check_boost_allowlist.sh` is added and passes. Its allowlist is
`fusion|mpl|preprocessor` (tier 3a, stage 7) plus `simd|dispatch` (NT2, stage
4), `mmap` (stage 6) and `intrusive|type_traits`, each annotated in the script
with the stage that removes it. **CI wiring is deferred with the rest of stage
1.5**, so the script has to be run by hand for now.

**Done when:** the only Boost includes left under `src/` are Fusion, MPL and
Preprocessor, and CI fails if that changes. — *Includes: done, with the wider
allowlist above. CI: deferred.*

#### What stage 2 found

- **`boost::iterator_range` is not `std::span`, and pretending otherwise is a
  stage 3 job.** The engine *slides* its ranges: `advance_begin`/`advance_end`
  are how the moving average, the vocoder envelope, `SharedStorageBuffer::
  resize` and the `SubRange` walkers are written, and `std::span` has no
  mutating equivalent. Its iterators are also not raw pointers, while ~60 call
  sites in `le/math/vector.cpp` alone pass `begin()` straight into a pointer
  taking primitive. So tier 1 got `LE::Utility::Span` — the same begin/end
  pointer pair, `LE_RESTRICT` intact, layout compatible for the existing
  `Span<T>` ⇄ `Span<T const>` reinterpret_casts, with `operator std::span<T>()`
  so the real migration can happen one call site at a time once the goldens
  exist. **This is a deliberate deviation from the table above.**
- **`std::optional` engages before its payload is constructed.**
  `Module::createGUI` used a Boost typed in-place factory precisely so that a
  re-entrant `gui()` during widget construction saw an empty optional;
  `emplace()` cannot reproduce that. This is the one place the sweep gives up a
  property the old code had rather than preserving it — the call site says so,
  and **stage 6.7 has to establish whether anything depended on it.**
- **`OptionalFromInstance` had the layout backwards.** It reinterpreted the
  payload address as `boost::optional`'s by subtracting the engaged flag, which
  Boost stores *first*. libstdc++, libc++ and the MS STL all put
  `std::optional`'s payload at offset zero, so the offset is now zero and the
  debug assert checks `&*optional == &instance`.
- **`le/utility/rvalueReferences.hpp` was a live hazard.** On any standard
  library that was not libc++ or the MS STL it defined `std::move`,
  `std::forward` and `std::declval` *itself*. That would have fired the first
  time anyone built on Linux. It is now `#include <utility>`.
- **The 2016 PCH and ODR header carried most of the Boost tuning.**
  `leConfigurationAndODRHeader.h` configured Spirit, Karma, Phoenix, TR1,
  `boost/detail/endian.hpp` and `boost::throw_exception`; all of that went with
  the libraries it configured, and `boost_compiler_config_msvc.hpp` (which
  existed only to `#undef BOOST_NO_CXX11_CONSTEXPR`) is deleted.
- **Boost.Range's restrict fix-ups in `tchar.hpp` are now NT2-only.** They
  taught `boost::range_detail` about `__restrict` pointers; with Boost.Range
  gone the only remaining consumer is Boost.SIMD, so the block moved inside
  `#ifdef LE_HAS_NT2`.
- **Nothing here is compile-verified.** The tree does not build until stage 3;
  the checks that do exist are the allowlist script, a "uses X without
  including Y" sweep over every substituted name, and clang-format. Expect the
  first `sw-dsp` build to shake out residue.

#### 2.1 — The shims stage 2 added, and when they go

Removing Boost meant adding five headers and a handful of macros. Each one is a
deliberate stopgap, not a design decision, and each should be revisited at the
stage named. **None of them is load-bearing enough to justify surviving to
ship unexamined.**

| Added | Sites | Should become | Stage |
|---|---:|---|---|
| `le/utility/ignoreUnused.hpp` | 50 | `[[maybe_unused]]` on the declaration; `static_cast<void>()` for the two odr-use sites | 3 ✅ in `sw-dsp`, 5–6 elsewhere |
| `le/utility/span.hpp` | ~90 | `std::span` | 3–4 |
| `le/utility/staticLog2.hpp` | 20 | **keep** — it defines `0 → 0`, which `IsPowerOfTwo<0>` needs and `std::bit_width` does not | — |
| `le/utility/polymorphicDowncast.hpp` | 15 | probably stays | — |
| `le/utility/intrusivePtr.hpp` | 30 | *see below* — not obviously `std::shared_ptr` | 5 |
| `LE_LIKELY` / `LE_UNLIKELY` (abi.hpp) | 50 | `[[likely]]`/`[[unlikely]]` where the site is a statement, delete where it is a no-op | 3 ✅ in `sw-dsp`, 5–6 elsewhere |
| `LE_CURRENT_FUNCTION` (abi.hpp) | 2 | `std::source_location` in the assert handler | 3 ✅ |
| `le/utility/stackBuffer.hpp` | 18 files | keep — alloca has no standard spelling | — |
| `LE_LITTLE_ENDIAN` / `LE_BIG_ENDIAN` (abi.hpp) | 7 | `if constexpr (std::endian::native == …)`; the sites are `#if` only because Boost's were | 3 ✅ |
| `LE_NO_RTTI` / `LE_NO_EXCEPTIONS` (abi.hpp) | few | keep; they are compiler feature detection, not a Boost artefact | — |
| `LE_ASSERT` family (assert.hpp) | ~1200 | keep; revisit only if the handler should route to the DAW log | 9 |

**`ignoreUnused` is the easy one and the least defensible.** 48 of the 50 sites
are named parameters or locals where `[[maybe_unused]]` is strictly better —
declaration-site, no codegen, no header. It exists only because a function let
the sweep be a pure textual substitution instead of 48 declaration edits in a
tree the compiler cannot check. Convert at first compile, when the warning
either disappears or does not. The two exceptions are in
`le/plugins/vst/2.4/plugin.inl` and are not variables at all — a template
instantiation and an address-of on the exported entry point.

**`IntrusivePtr` is the one to think hardest about, and `std::shared_ptr` is
probably the wrong answer.** Four things in the module chain depend on the
count living inside the object:

- `moduleNode.hpp` asserts `sizeof(NodePtr) == sizeof(void *)`. `ModuleNode`
  stores `next_` and `previous_` as `NodePtr`; `shared_ptr` is two words, so
  this doubles every chain node and changes the layout the intrusive circular
  list algorithms walk.
- `ReferenceCount` is a *one byte* `std::atomic_uint_fast8_t`, packed into the
  node next to the other module state.
- `ModuleChainBase::chain_const_iterator` **derives from** the smart pointer.
- `Module::createGUI` does `IntrusivePtr<Module>(this)` from inside a member
  function. With `shared_ptr` that is `enable_shared_from_this`, which
  constrains how modules may be created — and they are currently placement-new
  constructed into engine-owned storage by `ModuleFactory`, not allocated.

So the question at stage 5 is not "swap in `shared_ptr`" but "does the chain
still need shared ownership at all, now that the CLAP host layer owns the
lifecycle?" If it does, the 60-line local pointer is the cheap answer and
should simply be kept and documented as such. If it does not, both the pointer
and the `intrusive_ptr_add_ref`/`intrusive_ptr_release`/
`intrusive_ptr_release_deleter` hooks scattered across `module.cpp`,
`moduleNode.hpp`, `moduleDSP.hpp` and `pimpl.hpp` go with it.

---

### Stage 3 — DSP core builds and is measured (macOS first) ✅

**3–5 weeks.** The first stage where SpectrumWorx code compiles.

**Why macOS first.** `le/math` already has a complete non-NT2 backend on Apple:
`LE_MATH_USE_ACC` routes both the vector primitives and the FFT to
Accelerate/vDSP. macOS is therefore the *only* platform where the DSP can be
brought up without first replacing the SIMD and FFT layers — which is precisely
the replacement you want golden tests to protect. So: bring it up on Apple,
capture the goldens, then port the backend (stage 4) and check the other two
platforms against them.

**✅ 3.1** `le/math/vector.cpp` includes the NT2 headers *unconditionally* at file
scope even though the Apple path never uses them. Make them conditional; confirm
the Accelerate path is self-contained. — *Done. NT2 is opt-in on `LE_HAS_NT2`
now, which nothing defines, so every platform takes the portable path until
stage 4.*

**✅ 3.2** Add the temporary Boost scaffold — CPM, not a submodule, so that it is
visibly not a dependency: `cmake/temporary-boost.cmake`, Boost 1.91.0 with
`BOOST_INCLUDE_LIBRARIES fusion;intrusive;mpl;preprocessor`, exposed as one
`sw-boost-scaffold` target. Deleting that file and the two lines that include it
removes Boost from the project.

**✅ 3.3** Build `sw-dsp` as a static library: `le/spectrumworx/{engine,effects}`,
`le/math`, `le/parameters`, `le/analysis`, the surviving parts of `le/utility`,
plus `core/automatedModuleChain` and `core/spectrumWorxCore`. No host, no GUI.
— *Done: `src/dsp.cmake`, 83 translation units, `libsw-dsp.a`.*

**✅ 3.4** Replace `effectsList.cmake`'s string-concatenation codegen with a plain
C++20 `constexpr` table — 57 effects, entirely mechanical. This kills two dozen
`configure_file()` calls that currently write generated headers back into the
source tree. — *Done. `configuration/effectsList.hpp` is now the single source
of truth: one `LE_SW_EFFECT_LIST(x)` list that the index→impl, index→group and
name tables all expand. `effectsList.cmake`, `configuration.cmake` and the nine
`.in` templates are gone; the only surviving `configure_file` is the version
header, and it writes to the build tree.*

**✅ 3.5** `tests/` with Catch2; target `sw-tests`. — *Done: 47 cases, 2216
assertions, green under CTest. Weighted towards what stage 3 wrote — the
portable arms of the `le/math` primitives that had none, the buffer and
stack-buffer replacements, the FFT, and the effect tables that are no longer
generated and so no longer consistent by construction.*

> **Linking an executable found what a static library had hidden.** `sw-dsp`
> archived happily with unresolved symbols; `sw-tests` is the first thing to
> demand they resolve, and six were missing.
> `LE::Utility::assertionFailed` **had no definition at all** — stage 2
> repointed `LE_ASSERT` at it and left `assertionHandler.cpp` still defining
> `boost::assertion_failed`, so every assert in the tree was a dangling call.
> `Math::PowerOfTwo::ceil` was declared `(float)` and defined `(float const &)`;
> `Math::negate(InputOutputRange)` was declared and never defined at all;
> `intrusive_ptr_release_deleter` was a non-inline function in a header, fine
> only for the single-TU Unity build it was written for. `DebugStr` — Carbon,
> deprecated since 10.8 — was still being called from the tracer and the
> assertion handler. And `baseParametersUIElements.cpp` /
> `commonParametersUIElements.cpp` turned out to be misnamed: they hold the
> parameter name and enumerated-value strings, which `ParameterInfo` carries
> **because presets serialise parameters by name**, so the DSP needs them.

> **Two things the test target had to give up, both stage 8's to give back.**
> `le/spectrumworx/presets.cpp` reads and writes preset files through
> `juce::File` and `boost::mmap` in the same translation unit as the parameter
> (de)serialisation, so `sw-dsp` cannot have the second without the first: it
> builds `LE_NO_PRESETS` for now. And twenty one `TuneWorx` parameters had a
> name **only** under `LE_SW_SDK_BUILD`, so no plugin build has ever had one;
> un-gating them makes the tree link, but they all share the placeholder
> `"N/A"`, which is not a usable preset key. Stage 7 owns giving them the real
> names, which are sitting commented out beside each.

**✅ 3.5b — Retire the stage 2 shims that first compile validates.** Per §2.1:
`ignoreUnused` → `[[maybe_unused]]`, `staticLog2` → `std::bit_width`,
`LE_LIKELY`/`LE_UNLIKELY` → `[[likely]]`/`[[unlikely]]` or nothing,
`LE_{LITTLE,BIG}_ENDIAN` → `if constexpr (std::endian::native …)`,
`LE_CURRENT_FUNCTION` → `std::source_location`. Each is cheap here and only
here, because the compiler confirms every single site.

*Done, with two revisions to §2.1 that only the compiler could have prompted.*

- **`LE_CURRENT_FUNCTION` and the endianness macros are gone outright.**
  `assertionFailed` and `Math::verifyFPValues` now take a defaulted
  `std::source_location`, which subsumes `__FILE__` and `__LINE__` as well —
  and firing an assert deliberately proves the whole path works, which is not
  something that could be said of it before this stage.
  `makeBool`'s `#if LE_LITTLE_ENDIAN / #elif LE_BIG_ENDIAN` left `result`
  *uninitialised* on any third answer; it now indexes with `std::endian`.
- **`staticLog2` stays, and §2.1 was wrong to say otherwise.** Inlining
  `std::bit_width(x) - 1` at the twenty call sites would drop the `0 → 0` case
  that `IsPowerOfTwo<0>` depends on — `bit_width(0) - 1` is `-1`, and the shift
  that follows is UB — and it would need every argument widened by hand. The
  shim is a better API than the expression it wraps: it names the operation and
  defines the edge. Keep it.
- **`ignoreUnused` and the branch hints are done where the compiler can see
  them, and only there.** The justification for doing this in 3.5b was
  "the compiler confirms every single site", and that holds for the ~60% of
  sites inside `sw-dsp`'s translation units. It does not hold for
  `le/plugins/{vst,au,fmod,unity}`, `gui/`, `filesystemAndroid.cpp`,
  `presets.cpp` or the three unshipped effects, none of which compile yet.
  Converting those blind is exactly the mistake stage 2 warned about, so the
  two headers stay and **stages 5 and 6 finish them as their files start
  compiling.** The no-op uses — the macro wrapping a `return` expression or a
  discarded one, where the hint can attach to nothing at all — are deleted
  everywhere.

**✅ 3.6 — Golden fixtures.** For every one of the 57 effects, at two FFT sizes ×
two overlap factors, render a fixed set of test signals (impulse, log sweep,
pink noise, one short real excerpt) and commit the output. Store a compressed
float dump plus a spectral summary; assert exact on same-platform and ~1e-4
relative cross-platform.

*Done: 464 fixtures — 58 chains (57 effects plus a bypassed one) × 4 signals ×
2 configurations — in `tests/goldens/data/goldens.txt`, reproducing bit-exactly
across runs. Three deviations, all deliberate.*

- **A digest, not a float dump.** The raw matrix is ~30 MB of binary nobody can
  review. Each row is an FNV-1a hash over the sample bits — the same-platform
  contract — plus peak, RMS, DC offset, a non-finite count and eight
  log-spaced band energies in dB, which is what a different architecture is
  held to. 76 KB, and a diff that says *which* effect moved and roughly how.
- **The fourth signal is synthetic.** "One short real excerpt" needs an audio
  file this repository has no licence for, so `Voice` is a 40-harmonic stack
  with three formants and 5 Hz vibrato: dense partials over a moving pitch,
  which is what the pitch and phase-vocoder effects actually have to cope with.
- **They render in a release build.** See below.

#### What the golden harness found

The engine cannot be instantiated without a derived class —
`Engine::Processor::modules()` downcasts to `SpectrumWorxCore` — so 3.6 began
by building one. Everything below is something that had to be fixed before a
single effect would render, and none of it was reachable from the unit tests.

- **The module deleter freed `malloc`'d storage with `delete`, on a base class
  with a non-virtual destructor.** `ModuleFactory::create` `malloc`s and
  placement-news the *derived* `Impl<Effect>`; both
  `intrusive_ptr_release_deleter` overloads said `delete &module`. Mismatched
  allocator, and the derived destructor would not have run either. There is a
  matching `ModuleFactory::destroy` now, dispatched through the same
  `switch_<ValidIndices>` the constructor uses. **This fires the moment any
  module is removed from a chain**, which is to say constantly.
- **The release build had never been compiled.** `LE_ASSERT` did not honour
  `NDEBUG` on the handler path, and the tree assumes it does — in
  `moduleChainImpl.cpp` an assert's operand is declared inside
  `#ifndef NDEBUG`, and three lines below it a `dynamic_cast` operates on a
  `ModuleNode` that is only polymorphic under `!NDEBUG`. `math.hpp` included
  `span.hpp` under `#ifndef NDEBUG` while declaring functions over `Span`
  unconditionally.
- **`ModuleParameters::parameterInfos()` was a non-inline definition in a
  header** — the comment above it read "assummes single inclusion" — and
  `intrusive_ptr_release_deleter` was the same. Making the latter `inline`
  papered over it in debug and broke in release, where it is inlined
  everywhere and no out-of-line copy survives for `module.cpp` to call. Both
  are ordinary functions in one translation unit now.
- **`Math::modulo`'s sanity assert was wrong for every negative dividend.** It
  compared against `std::fmod`, which truncates towards zero, while `modulo`
  floors — deliberately, because that is what makes it usable for phase
  mapping into [0, 2π). The two therefore differ by exactly `divisor` whenever
  the dividend is negative. Phasevolution is the first effect to feed it one.
- **The RNG could not be seeded reproducibly.** `rngSeed()` takes the clock and
  a stack address, and `initialise()` and `reset()` both call it, so Freqverb
  and Whisperer rendered differently every run. There is a
  `rngSeed(std::uint64_t)` overload now — useful well beyond the tests, for any
  reproducible render.

> **One numerical weakness is recorded rather than fixed, and it is why the
> goldens are a release-build artifact.** `Math::symmetricMovingAverage` keeps
> a running sum across thousands of bins; over pink noise the accumulated
> rounding drifts a hair below zero, so Smoother hands `amph2DFT()` a negative
> "amplitude" and the debug verification assert fires. The audible effect is a
> sign flip on a near-silent bin. Fixing it — a Kahan sum, or recomputing the
> window periodically — changes DSP output, which is exactly what must not
> happen in the commit that mints the baseline, and it belongs with the vector
> primitives in **stage 4** regardless. Until then `[golden]` skips in a
> checked build with that explanation, and the other 47 tests still run there.

> **A caveat worth stating plainly:** these goldens capture *2016 source as
> compiled by a 2026 toolchain*, not the behaviour of the 2016 binaries. Any
> difference introduced by C++20 semantics, UB fixes or 13 years of compiler
> change is baked into the baseline before you ever look at it. If fidelity to
> the shipped product matters, spend ~2 days first rendering reference output
> from the original binaries (they still run, under Rosetta or on Windows) and
> diff against those once. After that, the goldens are your contract.

**Done when:** `sw-tests` is green on macOS-arm64; goldens committed for all 57
effects; `rg LE_SW_AUTHORISATION_REQUIRED src` returns nothing.

#### What the first compile found

Stage 2 warned that nothing was compile-verified. It was right; here is the
residue, in the order it mattered.

- **The `LE_NOTHROW` family had to go, not be made consistent — 1159 sites in
  123 files.** C++17 made the exception specification part of the function
  type, and `__attribute__((nothrow))` counts. The 2016 habit was to decorate
  the out-of-line *definition* and not the header declaration, which is now a
  hard error at roughly half the sites. Spreading the macro to fix that would
  have entrenched an MSVC10-era workaround, so `LE_NOTHROW`,
  `LE_NOTHROWNOALIAS`, `LE_NOTHROWRESTRICTNOALIAS`, `LE_NOALIAS`,
  `LE_RESTRICTNOALIAS`, `LE_CONST_FUNCTION` and `LE_PURE_FUNCTION` are deleted.
  `LE_NOEXCEPT` — which stood in for the keyword and was `noexcept` on GCC but
  *nothing* on Clang, a live ABI divergence — is now plain `noexcept`, and it
  only ever sat on move constructors, where it belongs. What is genuinely lost
  is `__attribute__((const))`/`((pure))`/`((malloc))` on ~120 functions; put any
  of them back only with a measurement behind it.
- **Three functions had no non-NT2 implementation at all.** `Math::mix()` (the
  range overload), `addPolar()`, and the `#else` arm of `polar2rectangular()`
  called `nt2::sinecosine` unguarded, and `rectangular2polar()`/`amplitudes()`
  had an NT2 arm and an Accelerate arm and nothing else. On any build that is
  neither, the last two silently returned without writing their outputs.
- **`Math::clamp()`'s portable branch had never been compiled.** It called
  unqualified `min`/`max`, which in `namespace LE::Math` find only the
  `float const *` range overloads. Every previous build took an SSE or NEON
  branch instead. Now `std::min`/`std::max`.
- **`polar2rectangular`'s scalar loop shadowed its own parameter.**
  `float const *LE_RESTRICT pPhases(pPhases);` — self-initialised from itself.
  Dead in every shipped configuration, but it is what the non-NT2 path would
  have run.
- **`log2`/`exp2` were defined twice and `ln`/`log10`/`exp` not at all** once
  `LE_HAS_NT2` went undefined: `vector.cpp` defined the five unconditionally
  while `math.cpp` defined two of them under `#ifndef LE_HAS_NT2`. Both files
  now use `conversion.cpp`'s existing `defined(__APPLE__) || !defined(LE_HAS_NT2)`
  guard, so exactly one definition survives in every configuration.
- **C++20 aggregate rules broke `EffectMetaData`.** A `= delete`d copy
  constructor is *user-declared*, and since P1008 that costs a type its
  aggregate-ness — while `MakeEffectMetaData` initialises it as an aggregate.
  The const and reference members already make it non-assignable.
- **`typeTraits.hpp`'s specialisations are now ill-formed and unnecessary.**
  Specialising `std::is_pointer` et al. is a hard error under C++20, and
  libc++, libstdc++ and the MS STL all answer correctly for `T * __restrict`
  today. The 2013 workarounds are deleted.
- **`BOOST_SWITCH_LIMIT` was 50 and there are 57 effects.** Dropping editions
  made every effect always-included, so `Effects::ValidIndices` is now the full
  range and the `switch_` dispatcher had to grow. A tidy demonstration that the
  edition mechanism was load bearing in more places than the codegen.
- **Two dependencies were replaced outright rather than found.**
  `boost::mmap` — a Boost sandbox library that was never released — was two
  calls, now `mmap`/`munmap` (plus the Win32 pair) in `filesystem.cpp`. The NT2
  stack-buffer macros were three, now `le/utility/stackBuffer.hpp`, which keeps
  the alloca semantics *and* the hand alignment the original needed because
  Clang's `alloca` does not align.
- **RapidXML could not be deferred to stage 8.** Preset (de)serialisation is
  inside `ModuleParameters::{load,save}PresetParameters`, not in a separable
  preset layer, so `sw-dsp` needs an XML parser today. RapidXML 1.13 is
  vendored in `libs/rapidxml/` — two headers — and §8.1 still replaces it.
- **VST 2.4 is now behind `LE_SW_VST24`, which nothing defines.**
  `plugin2Host.hpp` reached for `aeffectx.h` whenever the target was Windows or
  macOS. The three constants it guarded (`maxNumberOfPrograms`, `category`,
  `vstUniqueID`) are stage 5's to replace.

> **One silent regression is deliberately left for stage 4.**
> `BOOST_SIMD_HAS_SSE_SUPPORT`, `BOOST_SIMD_ARCH_X86` and `BOOST_SIMD_ARCH_ARM`
> appear ~35 times in `le/math/math.{hpp,cpp}` as plain *architecture
> detection*, not as NT2 usage. Undefined, every one of them falls to a correct
> portable branch — except `FPUDisableDenormalsGuard`, whose `_mm_getcsr`/
> `_mm_setcsr` arm is now dead on x86, so **denormal flushing is off on Linux
> and Windows x86-64** (arm64 keys off `__aarch64__` and is unaffected, and
> MSVC keys off `_controlfp`). The fix is to rekey them on the `LE_HAS_SSE1`/
> `LE_HAS_SSE2` macros `leConfigurationAndODRHeader.h` already derives from
> `__SSE__`/`__SSE2__`, which is stage 4's job (§4.2). Do not ship without it —
> a denormal storm in a spectral effect is a CPU cliff, not a subtle one.
>
> It does **not** gate the macOS-first path, which is the only reason stage 4
> could be resequenced behind stage 5: the `__aarch64__` arm at `math.cpp:906`
> sets FPCR bit 24 and is live and correct on Apple Silicon today. An Intel Mac
> is *not* covered — if the macOS build is ever x86_64, this comes forward.

---

### Stage 4 — Portable SIMD/FFT backend  *(resequenced: runs after stage 6)*

**1.5–2.5 weeks**, down from 2–3 because audio file I/O left for stage 5.

> **Why this moved.** The original plan ran 4 before 5 and this section was
> written as though the macOS build still had NT2 under it. It does not. On
> Apple, **stage 4.1 is already done, by construction:** `sw-dsp` compiles no
> NT2 at all — the library survives only in `legacy-build.cmake`'s dead
> `addNT2()` and the vendored `src/nt2_static_fft/` reference tree, neither of
> which is in the live build — and `dsp.cmake:162-165` links Accelerate because
> `le/math/vector.cpp` and `le/math/dft/fft.cpp` are vDSP/vForce there already.
> The stage 3 goldens were rendered *through Accelerate*. What remains in 4.1 is
> `pffft` and `simde` for the platforms a macOS-first bring-up defers anyway, and
> 4.3 is cross-platform by definition. Neither gates a macOS CLAP plugin, so
> neither should sit in front of one.
>
> The only thing in the old stage 4 that stage 5 genuinely cannot link without
> is the audio file loader, and its platform seam is a single static function.
> It moved to **5.0** rather than dragging a two-week stage forward.

**4.1** Replace `LE_MATH_USE_NT2`, keeping the interfaces in `le/math/vector.hpp`
and `le/math/dft/fft.hpp` byte-identical so the 203 effect files do not move:
- vector primitives → `sst-basic-blocks` SIMD helpers over `simde` (which gives
  you SSE-on-NEON), or plain loops where the compiler auto-vectorises just as
  well — measure, don't assume
- FFT → `pffft` off Apple; Accelerate/vDSP stays on Apple and does not move

**4.2** Rekey the denormal guard, per the regression noted at the end of stage 3.
`FPUDisableDenormalsGuard` (`math.cpp:895`) reaches `_mm_setcsr` only under
`BOOST_SIMD_HAS_SSE_SUPPORT`, which nothing defines since NT2 went — so denormal
flushing is off on every x86-64 target, **including an Intel Mac**. The
`__aarch64__` arm at `math.cpp:906` is live and correct, which is the only reason
this is not already a shipping bug. Rekey onto `LE_HAS_SSE1`/`LE_HAS_SSE2`.

*This is the one item that can bite before stage 4 runs: if the macOS-first
build is ever x86_64 rather than arm64-only, pull it forward. It is two lines.*

**4.3** Run the goldens on all three OSes and both architectures.

**Done when:** `sw-tests` green on macOS (arm64 + x86_64), Windows (x64 +
arm64), Linux (x64 + arm64); NT2 is gone with no golden outside tolerance;
denormals flush everywhere.

---

### Stage 5 — The CLAP host layer

**2.5–4 weeks.** The stage the whole plan is built around, and the one that most
resembles ordinary engineering rather than archaeology. **It runs directly after
stage 3** — see the note under stage 4 for why nothing in the SIMD work stands
in front of it.

**5.0 — The audio file loader**, moved here out of stage 4 because it is the one
thing in that stage stage 5 cannot link without. `spectrumWorx.hpp:15` includes
`external_audio/sample.hpp` and the plugin holds a `Sample sample_`, so
`SpectrumWorxCLAP` needs a `Sample::doLoad` to exist. `sample.hpp:87` shows the
platform seam is exactly that one static function; today its only macOS
implementation is `sampleMac.cpp` over `ExtAudioFile` and the long-removed
`FSRef`, which will not build against a current SDK.

Delete `sampleWin.cpp` (DirectShow filter graphs) and `sampleMac.cpp`, and write
one `doLoad` over `juce::AudioFormatManager` — JUCE is already linked here for
clap-wrapper's standalone target, so this costs nothing extra. Roughly fifty
lines. It is also what finally removes `FSRef` from `gui.hpp:226`.

**5.1 — A new protocol tag.** `src/le/plugins/clap/tag.hpp`, mirroring the
deleted `vst/2.4/tag.hpp`:

```cpp
namespace LE::Plugins::Protocol { struct CLAP {}; }
```

with `using ParameterSelector = ParameterID` — the AU choice
(`au/plugin.hpp:574`), not the VST2 one — because `SW::ParameterID::binaryValue`
*is* a `clap_id`.

**5.2** `src/le/plugins/clap/plugin.{hpp,inl}` implementing
`Plugins::Plugin<Impl, Protocol::CLAP>` on top of `clap::helpers::Plugin<>`.

**5.3** `src/spectrumWorxCLAP.{hpp,cpp}`, mirroring the deleted
`spectrumWorxVST24.{hpp,cpp}` almost line for line:

```cpp
class SpectrumWorxCLAP final
    : public SpectrumWorxSharedImpl<SpectrumWorxCLAP, Plugins::Protocol::CLAP>
```

**5.4 — Implement the eleven pure virtuals** of
`Plugin2HostInteropControler` (`core/host_interop/plugin2Host.hpp:136-152`):

| Virtual | Implementation |
|---|---|
| `automatedParameterBeginEdit` / `EndEdit` | queue `CLAP_EVENT_PARAM_GESTURE_BEGIN` / `_END` |
| `automatedParameterChanged(ParameterID, v)` | queue `CLAP_EVENT_PARAM_VALUE`, `param_id = id.binaryValue` |
| `parameterListChanged` / `moduleChanged` | `clap_host_params::rescan(INFO\|TEXT)`, **main thread** |
| `latencyChanged` | `clap_host_latency::changed()` + `request_restart` |
| `presetChangeBegin` / `End` | suppress rescans between, then one rescan + `clap_host_state::mark_dirty` |
| `hostTryIOConfigurationChange` | `clap_host_audio_ports::request_rescan` |
| `gestureBegin` / `gestureEnd` | undo-block markers; no CLAP equivalent — record locally |

**5.5 — `clap_plugin_params`.** `count` / `get_info` / `value_to_text` /
`text_to_value` delegate straight to
`Plugin2HostPassiveInteropController::numberOfParameters(Program const*)`,
`getParameterName(ParameterID, range, Program const*)`,
`getParameterLabel(...)`, `getParameterIDs(...)`. Those functions already take
the dynamic `Program const*` context — which is exactly the argument CLAP's
rescan model wants and the argument `AudioProcessorParameter` has no way to
accept. This is the payoff for the whole decision, and it should be about a day
of work.

**5.6 — `clap_plugin_state`.** Port `effGetChunk` / `effSetChunk`
(`vst/2.4/plugin.inl:303,355`) onto `save`/`load`. Version the blob explicitly
this time.

> **This sub-stage, not stage 4, is the one with a dependency in front of it.**
> `dsp.cmake:160` defines `LE_NO_PRESETS`, which compiles out
> `ModuleParameters::{load,save}PresetParameters`
> (`moduleParameters.hpp:216-222`) — the very serialisation a state blob is made
> of. Stage 3 switched it off because `presets.cpp` welds `juce::File` and
> `mmap` into the same translation unit as the parameter (de)serialisation, and
> splitting them is stage 8's job.
>
> So either pull that split forward, or do 5.6 **last** and let 5.1–5.5 and 5.8
> run ahead of it. A plugin that loads, shows its UI, passes audio and automates
> — but forgets everything on reload — is a perfectly good intermediate target,
> and it reaches the interesting risk (the dynamic parameter model, and the
> threading in 5.8) sooner.

**5.7 — Audio ports.** Main stereo in/out plus the sidechain gated by
`LE_SW_ENGINE_INPUT_MODE`.

> **Not an in-place pair, for now.** With an input gain of exactly 1,
> `SpectrumWorxCore::process` (`spectrumWorxCore.cpp:122`) skips its own copy and
> hands the host's input pointers straight to `Engine::Processor::process`. The
> WOLA path has not been audited for aliasing input and output, so
> `in_place_pair` is `CLAP_INVALID_ID` and a test pins it that way. Turn it on
> with a test that proves it, not by reading the code.

**5.8 — Threading.** The one genuinely new engineering in the project. Today
there is a `BackgroundThread`, a `GUI::Lock` over `MessageManagerLock`, and no
lock-free parameter queue — CLAP's `[main-thread]` / `[audio-thread]`
annotations are contractual, so this stops being optional. Adopt the sst
pattern:
- engine state mutated **only** by draining the input event list at the top of
  `process()`
- an SPSC ring (`sst-cpputils`) for audio → main notifications
- every `[main-thread]` CLAP entry point routed to the main thread; nothing
  touches engine state directly
- thread-identity asserts in debug builds
- run CI under `-fsanitize=realtime` (clang 20+), as shortcircuit does

Budget 1–2 weeks of the stage total for this. It is a latent bug class in the
2016 code regardless; CLAP just refuses to let you keep ignoring it.

**5.9** `clap-validator` plus the wrapper-produced VST3 and AUv2 in CI.

**Done when:** audible in Reaper, Bitwig, Logic and Ableton **with no GUI**,
driven entirely by the host's generic parameter panel; loading a different
effect into a module renames that module's parameters in the host panel;
automation round-trips; save/reload restores exactly; `clap-validator` clean;
VST3, AUv2 and standalone all behave identically.

#### What compiling the host-interop layer found

`core/host_interop/plugin2Host.cpp` and `host2Plugin.cpp` were in no target — the
stage 0 amputation left them behind with the VST2 SDK, and `sw-dsp` is forbidden
them by design. Adding them to `sw-impl` cost **no source changes at all**: they
compile against the stage 3 core as they stand.

What they did not do is *work*. The parameter enumeration is a contract between
two functions written independently, and both halves of it were wrong:

- **`numberOfParameters` subtracted for a parameter that is not there.** It took
  `Program::Parameters::static_size - 1 /*InputMode*/` unconditionally, but
  `InputMode` only enters the parameter list at `LE_SW_ENGINE_INPUT_MODE >= 1`
  (`parameters.hpp:47`) and is only withheld from the host at `>= 2`. Two
  different conditions, and nothing defines the macro at all in this build — so
  the subtraction removed a real parameter and `getParameterIDs` wrote one ID
  past the buffer its own caller had sized. Both now derive from one
  `exportedGlobalParameters` constant.

- **The per-module LFO count overflowed a `std::uint8_t`.** `lfoExportedParameters`
  is **7** without the GUI and **5** with it (`parameters.hpp:28`), and the
  widest effects have enough parameters that `(parameters - 1) * 7` passes 255.
  It wrapped, `numberOfParameters` under-reported by exactly **256**, and
  `getParameterIDs` — which counts the same thing in wider arithmetic — wrote 256
  IDs past the end. AddressSanitizer named it in one run after an afternoon of
  reading had not.

> Both are latent since 2013 and neither could fire in 2016: every shipping
> plugin built with `LE_SW_GUI=1`, where the multiplier is 5 and no effect is
> wide enough. The port's DSP-only configuration is what exposed them. This is
> the second time a 2016 constant has turned out to be load bearing in a
> configuration nobody built — `BOOST_SWITCH_LIMIT` was the first.

The lesson for the rest of stage 5: **`numberOfParameters` and `getParameterIDs`
are a pair, and so are `paramsCount` and `paramsInfo` above them.** A test that
checks each alone passes on both of these bugs. `tests/clap/parameterModelTests.cpp`
checks them against each other, for every effect, which is what caught it.

> **Run stage 5 under `-fsanitize=address`.** The heap corruption above was
> reachable from the host's generic panel with no GUI in the picture. 5.9 already
> asks for `-fsanitize=realtime`; ASan is the cheaper and, on this evidence, more
> productive of the two right now.

> **Why the host layer before the GUI.** The host's generic parameter panel is a
> complete, free test harness for the dynamic parameter model — the part of this
> port with the most novel risk. Proving it there, against five real DAWs,
> before writing a line of GUI code, means that when the GUI misbehaves in
> stage 6 you already know the parameter layer underneath it is sound.

---

### Stage 6 — GUI

**4–6 weeks.** The largest stage, and the one that is identical under either
plugin-format decision.

**6.0 — The harness, first.** `tools/show-ui/` — `sw-show-ui`, a desktop window
that hosts GUI code with no plugin, no host and no audio under it. Everything
else in this stage is built and looked at through it, which is what keeps the
stage off the critical path (see "Sequencing", below: 6 runs against stage 1,
not stage 5).

Pages self-register from their own translation unit, so a widget becomes
viewable when *it* compiles rather than when the editor does, and a page whose
sources are not ready is simply left out of the target:

```sh
sw-show-ui                          # the default page, in a window
sw-show-ui --list                   # what there is
sw-show-ui skin                     # a named page
sw-show-ui --render skin out.png    # offscreen; no window server needed
```

`--render` is not a convenience. CI has no display and neither does a sandboxed
agent, and a GUI you cannot look at is a GUI you cannot review: it paints the
page into a `juce::Image` and writes a PNG, which is also the hook a rendering
regression test will need later.

Deliberately **not** `juce_add_gui_app()`: `add_clap_juce_shim()` sets
`JUCE_MODULES_ONLY`, under which JUCE's top-level `CMakeLists.txt` returns
before `JUCEUtils.cmake` is included, so none of the `juce_add_*` helpers exist
on this route. A plain `add_executable` plus `START_JUCE_APPLICATION` is all a
JUCE GUI app actually is. `main()` is written out rather than left to the macro,
so that `--render` can run before `JUCEApplicationBase::main()` creates an
`NSApplication` it does not need.

**6.1** JUCE 8 API drift, per scan §1.3 — `String::empty`, `Image::null`,
`ScopedPointer`, `ButtonListener`/`SliderListener`, the deprecated `Font`
constructors. A few days of grinding; the volume is small.

**6.2** ✅ *complete*. `class Theme : juce::LookAndFeel` → **`LookAndFeel_V2`**,
in its own `src/gui/theme.{hpp,cpp}`. `LookAndFeel` is abstract in JUCE 8 — it
inherits ~26 `*::LookAndFeelMethods` interfaces whose members are pure — so the
2016 derivation no longer compiles.

Theme moved out of `gui.hpp` because it is what everything else in `src/gui`
reaches for a font or a colour, and it has almost no dependencies of its own —
so it is the layer that can compile first and give the rest of the stage a
build loop. The one thing that kept it entangled was two *static members* that
took a `ModuleControlBase` and a `ModuleUI`; they were only members because
they read `Theme::settings()`, and they are free functions in `gui.hpp` now.
(`aModuleControlNeedsLFOUpdate` went with them — it had no callers.)

`sw-show-ui theme` renders it, which is how the V2 choice below was confirmed
rather than merely reasoned about: the two sliders on that page draw the skin's
bitmap thumb next to a copy of the bitmap itself.

> **Not `LookAndFeel_V4`,** which the earlier draft of this plan said and which
> is wrong. `LookAndFeel_V4::drawLinearSlider` overrides the whole slider paint
> (`juce_LookAndFeel_V4.h:211`), so `Theme::drawLinearSliderThumb` — the thing
> that draws the LFO slider's bitmap thumb — would never be called and the
> failure would be silent. `LookAndFeel_V2::drawLinearSlider` forwards to
> background + thumb (`juce_LookAndFeel_V2.cpp:1538`), which is the behaviour
> the skin was written against. Every one of Theme's other overrides has an
> identical signature on V2 in JUCE 8.
>
> One does not: `getDefaultFolderImage()` returned `juce::Image` and now returns
> `const Drawable*` (`juce_FileBrowserComponent.h:198`). It is live code, not
> dead — `presetBrowser.cpp:272` calls it explicitly qualified,
> `Theme::singleton().Theme::getDefaultFolderImage()`, which is *why* the
> `override` on it was commented out in 2016.

**6.3 — Assets.** ✅ *complete*. `assets/skin/*` is a CMakeRC resource library
(`sw::skin`, rooted at `assets/` so stage 8's presets and samples can join it),
and `src/gui/resources.{hpp,cpp}` is the new accessor. `sw-gui-resources` is the
first of three GUI layers; the other two are `sw-gui-widgets` and `sw-gui`.

Four things changed beyond "read from memory instead of disk":

- **The enum values were multi-character literals.** `EditorBackground = '01'`,
  taken apart by `boost::mpl::string` to recover the two digits of the file
  name. Multi-character literals have an implementation-defined value and the
  only thing ever wanted from them was the number, so they are the number now.
  Call sites are unchanged. This is the same problem as 7.3's `Unit<' dB'>`,
  and one of the two `boost/mpl` uses outside `le/parameters`.
- **The list is an X-macro** (`LE_SW_RESOURCE_BITMAP_LIST`), like
  `LE_SW_EFFECT_LIST`, so `tests/gui/skinTests.cpp` can walk it and assert every
  named bitmap is in the binary. A mistyped number was previously a blank widget
  nobody notices until someone looks at a screenshot.
- **The cache is releasable.** 2016 kept one function-local `static juce::Image`
  per template instantiation — ~57 images that could not be freed and outlived
  the JUCE that allocated them. It is one array now, plus
  `releaseCachedResources()`. JUCE's leak detector catches the difference
  immediately, which is how this was found.
- **The macOS gamma correction is gone.** The loader ran an in-place
  `pow(x, 2.2/1.8)` over every byte of every bitmap on macOS, converting artwork
  authored for a PC's 2.2 gamma to the Mac's 1.8. Apple moved macOS to 2.2 in
  Snow Leopard, in 2009 — so it was already wrong when this shipped, and it
  darkens the skin on every Mac since while Windows, which never did it, does
  not. It also walked the buffer a byte at a time with no regard for pixel
  stride, gamma-correcting the *alpha* channel along with the colour. **This
  changes how the plugin looks on macOS**, to what the artwork says.

Fonts stopped being an OS concern entirely: 2016 registered `Vera.ttf` /
`VeraBd.ttf` with the system (`AddFontResourceEx` on Windows,
`CTFontManagerRegisterFontsForURL` on macOS) and then referred to them by family
name — which needed a file on disk, leaked the registration if the plugin was
unloaded abruptly, and let a system font of the same name win. JUCE 8's
`Typeface::createSystemTypefaceFor(void const *, size_t)` takes the bytes. That
also deletes `makeCFURLFromPath`, `makeFSRefFromPath` and the
`ATSFontContainerRef` typedef.

Eight bitmaps were deleted rather than embedded: **19, 25, 26, 29, 36, 37, 38,
39** are the licence manager's artwork — "Authorize…", "Buy Now", "This copy of
SpectrumWorx is licensed to:" — orphaned when stage 0 removed the licence
manager, and referenced by nothing.

> **Still outstanding from 6.3.** `initializePaths()` / `mapPathsFile()` /
> `rootPath()` / `presetsFolder()` are *not* gone — the skin was only one of
> their consumers. The rest (presets, samples, `SpectrumWorx.dat`, the User's
> Guide PDF) belongs to stage 8, and the callers are `spectrumWorx.cpp`,
> `spectrumWorxSharedImpl.inl`, `presets.cpp`, `presetBrowser.cpp` and
> `spectrumWorxEditor.cpp`. Note they cannot compile as they stand regardless:
> they are written on `boost::mmap`, which the stage 3 scaffold does not fetch
> and which is not in the tree.

**6.4 — Collapse the owned-window model** (scan §6.2). The preset browser and
settings panel become ordinary child `Component`s of the editor, overlaid or in
a `juce::CallOutBox`. Deletes `gui.mm` almost entirely, `OwnedWindowBase`,
`wndProcHook`, `peerWithParentHandle`, the Carbon `HIView` path and
`-framework Carbon`. **Scope this as a redesign, not a port** — both panels need
re-laying-out.

**6.5 — Editor lifecycle** via `sst::clap_juce_shim::ClapJuceShim`:
`createEditor()` returns the `SpectrumWorxEditor`, and the shim handles
`set_parent`, sizing, scale and the Linux timer/fd plumbing. The 2016 code
already calls `juce::initialiseJuce_GUI`, holds a `MessageManagerLock` and
attaches with `addToDesktop(0, nativeHandle)` — it is written in the shim's
idiom already, which is why this is a small piece of work.

**6.6 — Resizing and HiDPI.** The skin is fixed-size bitmaps with no @2x set.
Ship non-resizable (`shim->setResizable(false)`) or integer-scaled first. Do not
block this stage on redrawing 67 PNGs.

**6.7 — Re-check `Module::createGUI`'s half-built window.** Stage 2 lost a
property the 2016 code had deliberately: its Boost typed in-place factory ran
`doCreateGUI()` and `updateForEngineSetupChanges()` *before* the optional
marked itself initialised, so anything that reached `Module::gui()` while the
child widgets were being built saw an empty optional. `std::optional::emplace()`
engages first and there is no portable way to defer that. Walk the
`doCreateGUI()` call tree — the effect specific `ModuleControl` constructors and
`ModuleUI::updateForEngineSetupChanges` — for anything that reads `gui()` or
`gui()->` and would now get a partially constructed `ModuleUI` instead of
nothing. If any does, hold the construction behind an explicit flag on
`ModuleUI` rather than trying to reproduce the optional trick.

**Done when:** the full editor works in CLAP, VST3, AUv2 and standalone on all
three OSes; open/close cycles leak nothing; no separate desktop window exists
anywhere in the process; 6.7 is resolved one way or the other.

#### What reading the GUI for 6.0 turned up

Four things that change the shape of the work, none of which the estimate above
knew about.

**1. `PopupMenu` is not a port. It reinterprets JUCE's private members.**
`gui.cpp:1081-1138` defines a `JuceHackery::MenuItemInfo` that is a bit-for-bit
copy of a JUCE-internal struct, and reaches `juce::PopupMenu::items` — which is
`private` in JUCE 8 (`juce_PopupMenu.h:1068`) and no longer has that layout. It
exists to iterate and mutate menu items after the fact: current selection, tick
state, item sizes. **Every** `GUI::PopupMenu` / `PopupMenuWithSelection`
accessor is built on it, and both `ComboBox` and the module menu are built on
those. Rewrite against the public API (`PopupMenu::Item`, `MenuItemIterator`,
`addItem(Item)`), which means keeping the selection state in *our* wrapper
rather than reading it back out of JUCE. Budget days, not hours, and do it early
— the module menu is how you put an effect in a slot, so nothing else is
testable until it works.

**2. `juceLexicalCast.cpp` was deleted, not ported.** It redefined
`NumberToStringConverters`, which lives inside `juce_String.cpp` in JUCE 8, and
added explicit specialisations of `CharacterFunctions::getIntValue` /
`readDoubleValue` that are templates defined inline in the JUCE 8 header — so
the specialisations had nothing to attach to. It also `#define`d `noexcept` to
`throw()`. The whole file was a patch against the patched JUCE fork that stage 0
deleted: it substituted LE's `lexical_cast` for JUCE's own number formatting, a
micro-optimisation with no behavioural content.

**3. The editor cannot be handed a mock, because it is not given anything.**
`SpectrumWorxEditor()` takes no arguments; it finds the plugin by pointer
arithmetic from `this` (`Utility::ParentFromOptionalMember`, via
`SpectrumWorx::effect(Editor&)` at `spectrumWorx.cpp:1176`). `ModuleUI`,
`PresetBrowser` and `Settings` do the same. Six parent-from-member
relationships hold *by memory layout*, so a harness cannot substitute for the
plugin — it has to **be** the plugin, i.e. own the editor as a member at the
same offset. Everything the editor asks of the plugin funnels through six
accessors (`spectrumWorxEditor.cpp:319-345`), so that is the seam, and it is a
small one.

The harness should therefore compile a `LE_SW_GUI=1` core (`sw-dsp` is
`LE_SW_GUI=0`, and the only difference in the core headers is one typedef at
`spectrumWorxCore.hpp:172` plus the two chain-notification functions in
`plugin2Host.cpp`) and mock only `Plugin2HostInteropControler`'s eleven pure
virtuals — the layer that genuinely does not exist until stage 5. Real effects,
real parameter metadata, real LFOs and real presets all come for free, because
they are already in `sw-dsp`.

Build it with `LE_SW_DISABLE_SIDE_CHANNEL` defined: that compiles out
`SampleArea` and every `sample_` reference, which removes `external_audio/` and
the raw-`pthread` `BackgroundThread` from the harness in one flag.

**4. Two bugs that block the first compile, found by reading:**
- `spectrumWorxEditor.cpp:1852` initialises `pRegistrationData_(0)` in
  `Settings::Settings()`. No such member exists — it went with the licence
  manager and the initialiser did not. **The GUI does not compile until this is
  deleted**, harness or no harness.
- `gui.hpp:889-891` declares `Knob::stoppedDragging()` only under `#ifndef
  NDEBUG`, while `EditorKnob::stoppedDragging` (`gui.cpp:1700`) calls
  `Knob::stoppedDragging()` unconditionally — a release-only link error, the
  same class of thing stage 3 found in `math.hpp`.

Also worth knowing: `OwnedWindow<>` has exactly two instantiations, `PresetBrowser`
and `SpectrumWorxEditor::Settings`, which bounds 6.4 usefully.

#### The JUCE 8 drift, verified against the vendored headers

Ordered by effort, because the cheap half is genuinely cheap.

**Mechanical, and safe to sweep** — `String::empty` (13), `File::nonexistent`
(10), `Image::null` (4), `var::null` (1) are all behind
`JUCE_ALLOW_STATIC_NULL_VARIABLES`, which defaults to 0 and deprecates them even
when on: `{}` throughout. `ButtonListener` and `TextEditorListener` are gone
outright, `ValueListener` never existed, and `SliderListener` still exists but is
now a *template* (`juce_Slider.h:48`) so naming it un-instantiated is an error —
all four become the nested `X::Listener`. `Desktop::create`/`destroy`,
`MessageManager::destroySingleton` and `ComponentAnimator::stopTimer` are gone or
unreachable and all three lines can simply go.

**Include hygiene is the real first step**, because nothing compiles until it is
done: 17 sites include individual JUCE headers as `"juce/<module>/<sub>/<h>.h"`.
Both halves are wrong. The `juce/` prefix came from the deleted fork's layout,
and JUCE 8's individual headers have **no include guards and no `#pragma once`**
and open `namespace juce {` mid-file — they may only be reached through the
module umbrella header. `"juce/AppConfig.h"` and the `beginIncludes.hpp` /
`endIncludes.hpp` pairs do not exist at all.

**`JUCE_MODAL_LOOPS_PERMITTED` defaults to 0** in JUCE 8 and gates seven APIs
this code is built on: `PopupMenu::showMenu`, `AlertWindow::showMessageBox` /
`showOkCancelBox` / `showNativeDialogBox`, `FileChooser::browseForDirectory` /
`browseForFileToOpen`, and `MessageManager::runDispatchLoopUntil`. Defining it to
1 restores all seven and is one line; it is also precisely what JUCE warns
against in plugins, and it defers the largest piece of work in the stage.
Converting properly inverts control flow through `ComboBox::showMenu` →
`TitledComboBox::mouseDown` → `Settings::comboBoxValueChanged`, and through the
preset browser's entire save path — and the `PopupMenu::menuActive_` static only
makes sense synchronously, so it needs rethinking alongside.

**`Slider::valueListener()` never existed in stock JUCE.** `Knob` uses it to
unhook the Slider's own `Value::Listener` from its three `Value` objects,
deliberately cutting the Slider → Value → Slider feedback loop. Stock JUCE 8
offers no handle on it. The honest port is to hold the value in our own model and
drive the Slider one-way with `dontSendNotification` — which `Knob::setValue`
already does. Note this re-litigates a 2013 bug fix whose failure mode (spurious
automation notifications) only appears at runtime under a host, so it wants a
test rather than a patch.

**One ODR hazard to delete on the way past:** `math.cpp:983-991` defines
non-template `juce::jmin`/`jmax`/`jlimit(float const &, …)` overloads. JUCE 8
declares those as `constexpr` templates (`juce_MathsFunctions.h:352`).

**Modules:** linking `juce::juce_gui_basics` alone is already correct and
sufficient — `juce_graphics`, `juce_data_structures`, `juce_events` and
`juce_core` come transitively, and nothing in `src/gui` needs `juce_gui_extra`.

#### Where 6.1 stands, and the two rewrites left

| | state |
|---|---|
| 6.0 harness | ✅ `tools/show-ui`, three pages, all three are ctest cases |
| 6.1 API drift — mechanical half | ✅ swept: the four static nulls, the four listener typedefs, the four dead lifetime calls (`Desktop::create`/`destroy`, `MessageManager::destroySingleton`, `ComponentAnimator::stopTimer`), `getMainMouseSource`, the `jmin`/`jmax`/`jlimit` ODR hazard |
| 6.1 — include hygiene | ☐ 22 includes across 7 files still spell `"juce/<module>/<sub>/<header>.h"`, plus the `AppConfig.h` and `begin`/`endIncludes.hpp` pairs |
| 6.2 `Theme` | ✅ `LookAndFeel_V2`, in `src/gui/theme.{hpp,cpp}`, rendered by `sw-show-ui theme` |
| 6.3 assets | ✅ |
| 6.1 — the widget set | ☐ blocked on the two rewrites below |
| 6.4 – 6.7 | ☐ |

**These two are not ports.** Everything swept so far was a substitution: the
JUCE 8 spelling of a thing that still exists. What is left is two pieces of
2016 design that JUCE 8 does not have an equivalent of, and translating them
line by line is not available.

**Rewrite 1 — synchronous menus and dialogs.**
`JUCE_MODAL_LOOPS_PERMITTED` defaults to 0 in JUCE 8 and gates seven APIs this
code is built on: `PopupMenu::showMenu`, `AlertWindow::showMessageBox` /
`showOkCancelBox` / `showNativeDialogBox`, `FileChooser::browseForDirectory` /
`browseForFileToOpen`, and `MessageManager::runDispatchLoopUntil`.

Only four call sites, but each is straight-line code that *blocks on a user
answer and then acts on it*, so going async inverts control flow through
`ComboBox::showMenu` → `TitledComboBox::mouseDown` →
`Settings::comboBoxValueChanged`, and through the preset browser's entire save
path (`askForOverwrite`'s `bool` gates the write). Two consequences worth
planning for: `PopupMenu::menuActive_` is a scoped static that only means
anything synchronously, and every converted call site needs a lifetime story for
`this` — a `juce::Component::SafePointer`, because a menu can outlive the widget
that opened it.

> **The escape hatch, and what taking it costs.** `JUCE_MODAL_LOOPS_PERMITTED=1`
> restores all seven verbatim, in one line. It is the fastest route to a
> compiling editor and it is a legitimate way to sequence the work — but record
> it as a deferral, not a fix. JUCE warns against it specifically in plugins,
> and a nested modal loop inside a DAW's message thread is the kind of thing
> that works on the developer's machine and hangs on someone else's.
> `tools/show-ui` deliberately sets it to **0**, so the harness will not let a
> synchronous call compile.

**Rewrite 2 — `GUI::PopupMenu` reads JUCE's private state.**
`gui.cpp`'s `JuceHackery` declares a bit-for-bit copy of a JUCE-internal struct
and reaches `juce::PopupMenu::items` to read item text, id and icon, and to
*mutate* `isTicked` in place. In JUCE 8 that array is `private`, and the layout
no longer matches anyway — `Item::image` is a `std::unique_ptr<Drawable>`,
there is a `std::function<void()> action`, and `MenuItemIterator` exposes only
`next()` and `getItem()`.

The answer is to keep the item list — id, text, icon, enabled, ticked — in
`PopupMenuWithSelection` itself and rebuild a `juce::PopupMenu` on each show,
which is what `juce::ComboBox` does. Note that the 2016 comment at the head of
that block explicitly rejects this as too slow ("recreating the whole menu when
the selection changes, holding duplicates of all items"). That judgement was
made against 2010 hardware and a menu of a few dozen entries; **re-decide it,
don't inherit it.** Two smaller casualties: `getSelectedItemIcon()` returning
`juce::Image const &` has no JUCE 8 equivalent, and `getSelectedItemText()`
returning a reference into menu internals becomes a lifetime bug waiting to
happen — both should return by value from our own list.

**Do these two together.** They touch the same ~300 lines of `gui.cpp`, and
rewriting the item model for async and rewriting it for private-member access
is one job done once.

**Then a third, smaller, which needs a test rather than a patch.**
`Knob` calls `Slider::valueListener()`, which **never existed in stock JUCE** —
it was an addition in the patched fork, used to unhook the Slider's own
`Value::Listener` from its three `Value` objects and so cut the
Slider → Value → Slider feedback loop. Stock JUCE 8 offers no handle on it. The
honest port is to hold the value in our own model and drive the Slider one-way
with `dontSendNotification`, which `Knob::setValue` already does. But this
re-litigates a 2013 bug fix whose failure mode — spurious automation
notifications — appears only at runtime under a host, so removing the hack and
declaring victory is not evidence. Get the parameter round-trip under test in
stage 5 first, or accept that this one is verified by hand in a DAW.

> **The suggested order above was wrong about what blocks what.** See the next
> section: measuring it moved the menu rewrite one step later and put something
> else first.

#### `LE_SW_GUI=1` becomes the only configuration

Stage 5 wanted to host the real editor and ran into the flag. `sw-dsp` is built
`LE_SW_GUI=0` so the goldens can run without JUCE; the plugin needs `=1`, which
is not a cosmetic difference:

- `Engine::ModuleParameters` gains four virtuals (`moduleParameters.hpp:99-109`,
  `:235-239`), whose only job is to push a new parameter value into a knob so the
  UI animates under automation and LFOs.
- The module leaf class changes to a **sibling**, not a subclass: `SW::Module`
  and `SW::ModuleDSP` both derive from `Engine::ModuleDSP`, and `SW::Module`
  embeds a `std::optional<GUI::ModuleUI>`.
- `lfoExportedParameters` is 7 without the GUI and 5 with it — a host-visible
  parameter-count change, and the constant behind the overflow bug in stage 5.
- `presets.hpp` reaches for JUCE.

Measured rather than reasoned about: of the **85 sources in `sw-dsp`, 77 compile
byte-identically** under both settings; 6 differ and 2 did not compile at all.
And the divergence is **release-only** — under `NDEBUG` the flag gives
`ModuleParameters` a vptr (`sizeof` 64 → 72, `pLFOs_` 56 → 64, the
`ModuleDSP*`→`ModuleParameters*` adjustment +8 → +0), while a debug build is
already identical because `ModuleNode` has an `!NDEBUG`-only virtual. **A mixing
mistake is therefore invisible in every debug build and corrupts memory in
release.**

That measurement would support a shared-core plus per-configuration-slice split.
**We are not building one.** The virtuals that fork the ABI exist only because a
module holds a reference to its UI; under a proper two-queue design that
reference does not exist and the fork evaporates. It is not worth engineering
around a configuration whose function is needed anyway. `LE_SW_GUI=1` becomes
the only one, starting with the tests, and how the layers ought to link is
assessed once it all works rather than now on speculation.

`scripts/check_gui_flag_parity.py` guards the fork for as long as it exists — it
compiles every `sw-dsp` source both ways under `NDEBUG` and compares the objects,
so a source that quietly becomes configuration-dependent fails a build instead of
a customer. It is a ctest case labelled `slow` (~30 s; `ctest -LE slow` is the
fast loop) and it goes when the flag does.

**✅ The core now compiles at `LE_SW_GUI=1`** — all 85 sources, after seven edits:
six in `gui.hpp` (drop the `boost::mmap` and `boost::mpl` includes; delete the
duplicate `ResourceBitmaps` enum and `resourceBitmap<>` in favour of
`resources.hpp`, whose name set is identical; delete the `mapPathsFile` overloads
and the Carbon `FSRef` declaration; disambiguate `MessageManagerLock(nullptr)`,
which JUCE 8 overloads on both `Thread*` and `ThreadPoolJob*`; stop deriving
`DrawableText` from `GlyphArrangement`, now `final`) and one in `moduleUI.hpp`
(fork include → umbrella).

#### What blocks the link is not what the section above says

Compiling was cheap. Linking is the job, and the blocker is **not** the two
rewrites:

`factory.cpp` instantiates `Module::Impl<Effect>` for all 57 effects, whose
vtables ODR-use the whole module widget set, which pulls in `moduleUI.cpp` and
`moduleControl.cpp`, which call ~14 out-of-line `SpectrumWorxEditor` members. And
`spectrumWorxEditor.cpp` is welded to the deleted 2016 `SpectrumWorx` VST2/AU
class (`effect().sample_`, `loadPreset`, … at `spectrumWorxEditor.cpp:196,321,
615-640`). **That binding is what stands between `sw-tests` and a `LE_SW_GUI=1`
link**, and it is a bigger job than either rewrite.

The rewrites are still needed, one step later: `Knob`, `ComboBox`, `PopupMenu`
and `PopupMenuWithSelection` all live in `gui.cpp` and are bases of `ModuleKnob`
and `DiscreteParameter`, so the widget layer has to link before the module layer
can. Revised order: **widget layer (`gui.cpp`, with both rewrites) → module layer
plus a temporary editor seam → flip the flag → 6.4.**

**✅ `sw-gui-widgets` compiles, at `LE_SW_GUI=1`, with both rewrites done.**

- **Rewrite 1, modal loops.** `warningMessageBox` is `showMessageBoxAsync`, and
  the `isGUIInitialised()`/`isThisTheGUIThread()` dance that chose between it and
  the now-deleted `showNativeDialogBox` went with it — the async call is safe
  from anywhere. `warningOkCancelBox` cannot return a `bool` any more and takes a
  callback; its only live callers are the preset browser's two save-path
  questions, so inverting them is that file's job. `PopupMenu::showAt` is
  `showMenuAsync`, which inverts `ComboBox::showMenu` → `TitledComboBox::mouseDown`
  → `Settings::comboBoxValueChanged` exactly as predicted. Every converted site
  captures a `Component::SafePointer`, because a menu can now outlive the widget
  that opened it — a problem the blocking version could not have.
  `menuActive_` survives with its meaning intact: it is now true from show until
  the callback runs, which is what the callers were asking anyway.
- **Rewrite 2, `PopupMenu`.** `namespace JuceHackery` is gone. `PopupMenu` holds
  its own item vector and builds a `juce::PopupMenu` at show time. **The 2016
  "rebuilding is too slow" judgement was re-decided, not inherited** — building a
  menu of a few dozen items on a click is free now, and it buys: JUCE's layout
  stops being our business, `getSelectedItemText()` and `getSelectedItemIcon()`
  return references into storage we own rather than into JUCE internals, and the
  ID mangling that burned the top byte of the ID space becomes a plain `+1`.

Two 2013 workarounds went at the same time, and they turned out to be the *same*
workaround: `Button` unhooking itself from its own `Value`, and the fork-only
`Slider::valueListener()`. Both cut a widget→`Value`→widget loop that produced
spurious automation writes. JUCE 8 cuts it itself — `juce_Slider.cpp:433` and
`juce_Button.cpp:58` both pass `dontSendNotification` on the `Value` round-trip,
and `Button` is no longer a `Value::Listener` at all, so the hack could not be
expressed even if it were wanted. Read from JUCE's sources rather than observed,
so watch for doubled automation writes the first time this runs under a host.

#### ✅ `LE_SW_GUI` is deleted, not set

Nobody should ever set it to anything but 1, so it is gone rather than pinned —
along with `LE_SW_SEPARATED_DSP_GUI`, the never-finished separated-instances
option that half its conditions were entangled with. `unifdef` resolved both
across 30 files; the handful it could not evaluate (mixed with `defined(...)`)
were done by hand, and the dead 2016 cache options went too so that nothing can
define the macros back into existence.

The layering that came out of it:

```
sw-gui-resources   skin bitmaps, fonts, Theme
sw-dsp             the engine  — now links juce_gui_basics, because its module
                   class embeds a GUI::ModuleUI
sw-gui-widgets     gui.cpp: knobs, buttons, combo boxes, menus
sw-gui             moduleUI, moduleControl, moduleDSPAndGUI
sw-impl            the CLAP plugin
```

**The link closure was much bigger than "~14 entry points".** One factory
instantiation pulls the whole module widget set, which calls nineteen editor
functions — and supplying them means defining `~SpectrumWorxEditor`, which emits
the vtables of the four panels the editor owns *by value*, which needs every
virtual each of those declares. Forty-seven functions, not fourteen.

They live in `src/gui/editor/placeholderEditor.cpp` and every one aborts. It is
the same kind of placeholder as `stubEditor.cpp`, which the plugin has been
showing since stage 1 and still shows, so none of them are reachable today. Its
size is the honest measure of the coupling, and the instruction on it is
**replace, do not extend**: `spectrumWorxEditor.cpp` takes over once it is
unbound from the deleted 2016 plugin class, and the file is deleted whole.

That unbinding is now the single thing standing between this port and a plugin
with its real UI.

One thing that is *not* in the way: `GUI::ModuleUI` is a `std::optional` that
stays empty until `createGUI()`, and `ParameterWidgets` is raw storage until
`ModuleWidgets::create()`. A headless test can construct all 57 modules, process
audio and never touch JUCE.

> **Re-run the release goldens before pushing any of this.** Stage 6 has no
> business changing DSP output, which is exactly why an unexplained golden
> movement here would be worth stopping for. The checked build skips them; only
> `build-release` runs them. Flipping the flag makes `set*Parameter` an indirect
> call on the DSP path — the arithmetic is unchanged, the codegen is not, so
> that flip is the one to check most carefully.

---

### Stage 7 — De-Boost: the parameter system

**4–6 weeks.** The last Boost dependency, and the spine of the codebase:
`LE_DEFINE_PARAMETERS` (`le/parameters/factoryMacro.hpp:157-177`) declares each
effect's parameters as a **Boost.Fusion associative sequence**, and everything
downstream — automation, preset serialisation, and the automatic generation of
per-effect GUIs — iterates it generically.

By now the golden tests from stage 3 and a working plugin from stages 5–6 have
your back. That is why this is stage 7 and not stage 2.

**7.0 — Snapshot the parameter table first.** Before touching anything, add a
test that asserts the full `(id, name, unit, min, max, default)` tuple for every
parameter of every effect against a committed snapshot. Stage 7's characteristic
failure mode is silently reordering parameters, which silently changes saved
state — and no audio test will catch it.

**7.1** Replace the Fusion adaptation (`fusionAdaptors.hpp`'s `begin_impl` /
`end_impl` / `at_impl` / `value_at_key_impl`) with a `std::tuple`-based sequence:
`forEach` via `std::apply` and index-sequence folds, `at_key<Tag>` via the
`IndexOf` that already exists in `parametersUtilities.hpp`. **Keep
`LE_DEFINE_PARAMETERS`'s call syntax identical** so the 57 effect headers do not
change.

**7.2** The preprocessor sequence syntax `( (Name)(Type)(Minimum<-48>)… )` →
variadic macros or a designated-initialiser table. This is the part that does
touch the effect headers.

**7.3** `Unit<' dB'>` — multi-character literals as non-type template parameters,
legal but implementation-defined and warning-generating. C++20 class-type NTTPs
give you `Unit<"dB">`.

**7.4** Delete the CPM Boost scaffold; empty the CI allowlist from stage 2.

**Done when:** `rg 'boost/' src` returns nothing; every golden and the parameter
snapshot are unchanged; the CI gate is an exact-zero check.

---

### Stage 8 — Presets, paths and content

**1–2 weeks.**

**8.1** RapidXML → tinyxml2 via `SST_PLUGININFRA_PROVIDE_TINYXML ON`. Per the
stage 0.7 decision, change the parser, not the schema.

**8.2** Factory presets (1.2 MB, 15 banks) embedded via CMakeRC rather than
installed to disk — removing the last runtime dependency on the installer having
run. Samples (1.4 MB) likewise, or shipped to
`sst::plugininfra::paths::bestDocumentsFolderPathFor("SpectrumWorx")`.

**8.3** User preset directory via `sst::plugininfra::paths`, not a `.paths` file.

**Done when:** factory banks load in a plugin installed by copying a single
bundle; user save/load round-trips on all three OSes; an unmodified 2016-era
preset file still loads.

---

### Stage 9 — Ship

**1–2 weeks.**

**9.1** `include(basic_installer)` from sst-cmake — dmg + notarisation on macOS,
Inno Setup on Windows, tar + deb on Linux.

**9.2** Version information wired through `sst::plugininfra::version_information`.

**9.3 — Settle the licence.** JUCE 8 is AGPLv3-or-commercial; the repo is
currently GPL-3.0 with every file header still saying "All rights reserved"
(scan §8.1.2–3). Pick a destination, make the headers agree with the LICENSE
file, and write `THIRD_PARTY.md`.

**9.4** Convert the `.doc` manual to Markdown; README with screenshots.

**9.5** Nightly and release workflows modelled on OB-Xf's.

**Done when:** a tagged release produces signed, notarised, installable
artefacts for macOS universal, Windows x64 + arm64 and Linux x64 + arm64, from
CI, with no manual steps.

---

## Sequencing and parallelism

```
0 ─┬─▶ 1 ─┬─────────────▶ 3 ──▶ 5 ─┬──▶ 7 ──▶ 8 ──▶ 9
   │      │                        │
   └─▶ 2 ─┘                        │
          └─▶ 6 (GUI, in a harness)┘

   4    off the critical path — run it when you want a second platform,
        which in practice is after 6
   5.6  may trail 8 if the presets split is not pulled forward
```

- **0 → everything.** Nothing starts before the history rewrite.
- **1 and 2 are independent** and can run concurrently against the broken tree.
- **3 needs both** (1 for the build, 2 so it compiles without a Boost zoo).
- **5 follows 3 directly.** Stage 4 used to sit between them; on a macOS-first
  bring-up it has nothing to contribute, because the Apple SIMD/FFT path is
  Accelerate today and always was. The one genuine coupling — the audio file
  loader — is now 5.0. See the note under stage 4.
- **4 runs when you want a second platform**, which in practice means after 6.
  Doing it late costs one thing worth naming: the first non-macOS build then
  happens after the threading model in 5.8 and the GUI are both settled, so any
  collision surfaces late. Judged mild — the FFT and vector interfaces are held
  byte-identical by 4.1's own constraint, and the goldens are the arbiter.
- **6 can start as soon as 1 lands.** Port `src/gui` to JUCE 8 inside a small
  standalone JUCE harness app that instantiates the editor against a mock
  controller, then wire it to the real plugin after stage 5. This keeps the
  single biggest stage off the critical path, which is the main reason the whole
  project can be two people rather than one person for eight months. The harness
  is `tools/show-ui` (6.0); note that "mock controller" turned out to mean
  mocking only the eleven-method host-interop vtable over a real core, not
  mocking the core — see the findings under stage 6.
- **7 needs 5 and 6** — you want a working plugin before refactoring its spine.

**Two-person split:** A takes 0, 1, 3, 5, 8, 9 then 4 (build, DSP, host layer);
B takes 2 and 6 (Boost sweep, then GUI) and joins A on 7.

Rows below are in stage-number order, not running order. Running order is
0, 1, 2, 3, **5, 6**, 7, 8, 9, with **4** wherever a second platform is wanted.

| Stage | | Weeks |
|---|---|---:|
| 0 | Purge and amputate ✅ | 1 |
| 1 | Walking skeleton ✅ (CI, installers and signing deferred) | 1.5–2.5 |
| 2 | Boost tier-1 sweep ✅ (CI wiring deferred) | 1–2 |
| 3 | DSP core + goldens ✅ | 3–5 |
| 4 | Portable SIMD/FFT — *deferred, runs after 6* | 1.5–2.5 |
| 5 | CLAP host layer — *includes the audio loader as 5.0* | 2.5–4 |
| 6 | GUI | 4–6 |
| 7 | De-Boost the parameter system | 4–6 |
| 8 | Presets and content | 1–2 |
| 9 | Ship | 1–2 |
| | **Serial total** | **20.5–33** |
| | **Two people, 6 in parallel** | **~16.5–25** |

---

## Risk register

| # | Risk | Mitigation | Stage |
|---|---|---|---|
| 1 | **Thread discipline.** CLAP's main/audio thread split is contractual; the 2016 code has no lock-free parameter queue. | Adopt the sst patch/patchMain pattern from day one of stage 5; rtsan in CI; thread-identity asserts. | 5 |
| 2 | **Parameter refactor silently reorders parameters**, breaking saved state — invisible to audio tests. | Commit a full parameter-table snapshot test *before* starting (7.0). | 7 |
| 3 | **Owned-window collapse is a redesign, not a port.** Two panels need re-laying-out. | Scope it as such; do not fold it into "port the GUI". | 6 |
| 4 | **SIMD/FFT swap changes DSP output.** | macOS-first bring-up on Accelerate, goldens captured before the swap. Largely retired: the Apple path *is* Accelerate and never moves, so the swap only ever risks the platforms 4 brings up. | 4 |
| 4b | **Deferring 4 past 5 and 6** means the first non-macOS build lands after the threading model and the GUI are set. | 4.1 holds the vector and FFT interfaces byte-identical, so a collision would have to be in the build, not the API; goldens arbitrate the rest. | 4 |
| 5 | **Golden baseline is 2016 source on a 2026 compiler**, not 2016 behaviour. | If fidelity matters, diff once against renders from the original binaries (~2 days). | 3 |
| 6 | **Host handling of `rescan(INFO\|TEXT)` varies.** The dynamic parameter model is the novel part of this plugin. | Exercise it with fake parameters in the stage 1 stub, across five DAWs, before writing the real thing. | 1 |
| 7 | **clap-wrapper AUv2 on current macOS** — less DAW-exercised than `juce_audio_processors`. | Prove it in stage 1 with an empty plugin, not in stage 9 with a full one. | 1 |
| 8 | **Preset compatibility** constrains stages 7 and 8. | Decide in 0.7, before any of it is designed. | 0 |
| 9 | **Boost scaffold becomes permanent.** | CPM not a submodule; CI allowlist that only ever shrinks. | 2, 7 |

---

## Explicitly out of scope

- **LV2** — clap-wrapper does not emit it. Add it later via a separate wrapper
  if there is demand.
- **AUv3 / iOS** — a different sandboxing model and a different GUI contract.
- **`LE_SW_SEPARATED_DSP_GUI`** — an unfinished 2016 build option
  (`gui.hpp:547` has `LE_UNREACHABLE_CODE()` on that path). Drop the option.
- **FMOD and Unity backends** — deleted in stage 0.
- **The RtAudio standalone app** (`le/audioio`, 20.6 k lines) — clap-wrapper's
  standalone target replaces it entirely.
- **HiDPI artwork** — a fixed-size bitmap skin is a design problem, not a
  porting problem.

---

## First week, concretely

```sh
# ✅ 0. safety net — mirror backup, plus origin still holds the old history
#       until you force-push
git tag attic/2016-import
git clone --mirror . ../SpectrumWorx-prefilter.git

# ✅ 1. the irreversible thing
git filter-repo --force --invert-paths \
  --path source/externals/le/plugins/vst/2.4/aeffect.h \
  --path source/externals/le/plugins/vst/2.4/aeffectx.h \
  --path-glob 'source/externals/le/license_key/*'
# filter-repo removes the remote; re-add before pushing
git remote add origin git@github.com:baconpaul/SpectrumWorx
git push --force origin main
git push --force --tags

# ✅ 2. retain what's worth retaining out of the delete list
git mv source/externals/nt2/modules/core/signal/include/nt2/signal/static_fft.hpp \
       source/externals/nt2_static_fft/nt2/signal/
# … plus details/, the unit test and the bench

# ✅ 3. delete
git rm -r source/externals/nt2 source/externals/le/audioio
git rm -r source/externals/le/licenser source/externals/boost
git rm -r 3rd_party installer/resources
git rm    source/externals/le/build/*.cmake source/externals/le/build/iOSUniversalBuild.sh
# le/plugins/{vst,au,fmod,unity} deliberately retained until stage 5

# 4. next: §3.2's licence-manager excision (0.4), then the git mv to src/ (0.5)

# 5. mechanical sweeps, one commit each (0.6)
scripts/to_utf8.sh ; scripts/fix_file_comments.py ; scripts/strip_pragma_once.sh
clang-format -i $(git ls-files '*.cpp' '*.hpp' '*.inl')

# 6. stage 1 starts
git submodule add https://github.com/juce-framework/JUCE libs/JUCE
git -C libs/JUCE checkout 8.0.12
```

By the end of week two you should have an empty `SpectrumWorx.clap` opening in
Bitwig with a blue rectangle in it — and from there the remaining work is
additive.
