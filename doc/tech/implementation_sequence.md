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
  skin/                     67 PNGs, was installer/ProgramFolder/Resources
  presets/                  15 factory banks
  samples/
tests/
  CMakeLists.txt            sw-tests (Catch2)
  golden/                   committed DSP fixtures
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

### Stage 0 — Purge and amputate  🟡 *in progress*

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

**0.6 — Whole-tree mechanical hygiene**, each in its own commit:

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

**0.7 — Decide preset backward compatibility now.** It is a one-sentence
decision that constrains stages 7 and 8, and it is much cheaper to make here
than to discover later. Recommendation: **keep the XML schema exactly**; change
only the parser. That means the parameter refactor in stage 7 may not rename or
reorder anything a preset file references.

**Done when:** `git ls-files | wc -l` is roughly 1,000 rather than ~13,000; the
tree contains only code you intend to ship; `git log --all --diff-filter=A --
'*aeffect*'` is empty.

---

### Stage 1 — Walking skeleton

**1.5–2.5 weeks.** An *empty* plugin, in all four formats, on all three OSes,
built and packaged in CI. No DSP, no GUI, no SpectrumWorx code at all.

This is the highest-value-per-week stage in the project. It proves the entire
delivery chain — submodules, CMake, clap-wrapper, code signing, notarisation,
installers, the CI matrix — while the thing being delivered is 200 lines you can
debug in an afternoon. Every problem it finds is a problem you would otherwise
have found in stage 9, tangled with 90 k lines of ported code.

**1.1** Add the submodules above; pin JUCE to 8.0.12.

**1.2** Top-level `CMakeLists.txt`, `libs/CMakeLists.txt`, `src/CMakeLists.txt`,
`src/clap-first/CMakeLists.txt` per the sketches above. Version from
`sst-plugininfra/cmake/git-version-functions.cmake`'s
`version_from_versionfile_or_git`.

**1.3** A stub plugin deriving from `clap::helpers::Plugin<>` that:
- declares the **real** audio ports it will eventually need — stereo main in,
  stereo out, optional stereo sidechain
- exposes a handful of **fake dynamic parameters** and calls
  `clap_host_params::rescan(INFO|TEXT)` on a timer

  Do this on day one. The dynamic-parameter path is the single riskiest
  interaction with hosts, and you want to know *now* which of Reaper, Bitwig,
  Logic, Ableton and FL handle a mid-session rescan gracefully — not after
  stage 5.
- passes audio through unmodified

**1.4** `add_clap_juce_shim` + `ADD_SHIM_IMPLEMENTATION(clapJuceShim)`, with
`createEditor()` returning a `juce::Component` that paints one rectangle.

**1.5** `include(basic_installer)`; CI modelled on OB-Xf's
`.github/workflows/build-plugin.yml` using
`surge-synthesizer/sst-githubactions/prepare-for-juce` and `install-innosetup`.
Matrix: macos (universal), windows-msvc-x64, windows-arm64, linux-x64 (gcc 12/13/14),
linux-arm64. Add `clang-format-check` from the same action set.

**Done when:** green CI on every matrix entry; `clap-validator validate` clean;
the `.clap`, `.vst3`, `.component` and standalone all load in Reaper and Bitwig
and show the rectangle; the mac artefact is signed and notarised; the installers
install.

---

### Stage 2 — Boost tier-1 sweep

**1–2 weeks. Runs in parallel with stage 1, and still against a non-compiling
tree** — which is exactly why it is here and not at the end. These are
type-for-type substitutions with no behavioural content; doing them now costs a
sed script and a careful read, doing them in stage 7 costs a rebuild of a
working plugin for each one.

Per scan §5.1 tier 1 — roughly 300 include sites:

| Boost | → |
|---|---|
| `boost/assert.hpp` (77) | `assert` / a project macro |
| `boost/config/abi_{prefix,suffix}.hpp` (102) | delete, pure noise |
| `boost/range/iterator_range_core.hpp` (21) | `std::span` |
| `boost/utility/string_ref.hpp` (17) | `std::string_view` |
| `boost/smart_ptr/intrusive_ptr.hpp` (12) | keep a 40-line local `IntrusivePtr` |
| `boost/noncopyable.hpp` (8) | `= delete` |
| `boost/polymorphic_cast.hpp` (8) | `dynamic_cast` + assert |
| `boost/optional` (6) | `std::optional` |
| `boost/core/ignore_unused.hpp` (6) | `[[maybe_unused]]` |
| `boost/range/algorithm/*` (~12) | `std::ranges::*` |
| endian / cstdint / array / integer / ref / mem_fn / limits / scoped_array (~30) | `<bit>`, `<cstdint>`, `<array>`, `std::ref`, `std::mem_fn`, `<limits>`, `std::unique_ptr<T[]>` |
| `boost::signals2::mutex` | `std::mutex` |
| `boost::filesystem` (sandbox) | `std::filesystem` |

Add `scripts/check_boost_allowlist.sh` and wire it into `code-checks.yml`, with
an allowlist that starts as `fusion|mpl|preprocessor` and empties in stage 7.

**Done when:** the only Boost includes left under `src/` are Fusion, MPL and
Preprocessor, and CI fails if that changes.

---

### Stage 3 — DSP core builds and is measured (macOS first)

**3–5 weeks.** The first stage where SpectrumWorx code compiles.

**Why macOS first.** `le/math` already has a complete non-NT2 backend on Apple:
`LE_MATH_USE_ACC` routes both the vector primitives and the FFT to
Accelerate/vDSP. macOS is therefore the *only* platform where the DSP can be
brought up without first replacing the SIMD and FFT layers — which is precisely
the replacement you want golden tests to protect. So: bring it up on Apple,
capture the goldens, then port the backend (stage 4) and check the other two
platforms against them.

**3.1** `le/math/vector.cpp` includes the NT2 headers *unconditionally* at file
scope even though the Apple path never uses them. Make them conditional; confirm
the Accelerate path is self-contained.

**3.2** Add the temporary Boost scaffold — CPM, not a submodule, so that it is
visibly not a dependency:

```cmake
# TEMPORARY SCAFFOLD — Fusion/MPL/Preprocessor only, deleted in stage 7.
# See doc/tech/implementation_sequence.md
CPMAddPackage(NAME Boost VERSION 1.89.0 GITHUB_REPOSITORY boostorg/boost …)
```

**3.3** Build `sw-dsp` as a static library: `le/spectrumworx/{engine,effects}`,
`le/math`, `le/parameters`, `le/analysis`, the surviving parts of `le/utility`,
plus `core/automatedModuleChain` and `core/spectrumWorxCore`. No host, no GUI.

**3.4** Replace `effectsList.cmake`'s string-concatenation codegen with a plain
C++20 `constexpr` table — 57 effects, entirely mechanical. This kills two dozen
`configure_file()` calls that currently write generated headers back into the
source tree.

**3.5** `tests/` with Catch2; target `sw-tests`.

**3.6 — Golden fixtures.** For every one of the 57 effects, at two FFT sizes ×
two overlap factors, render a fixed set of test signals (impulse, log sweep,
pink noise, one short real excerpt) and commit the output. Store a compressed
float dump plus a spectral summary; assert exact on same-platform and ~1e-4
relative cross-platform.

> **A caveat worth stating plainly:** these goldens capture *2016 source as
> compiled by a 2026 toolchain*, not the behaviour of the 2016 binaries. Any
> difference introduced by C++20 semantics, UB fixes or 13 years of compiler
> change is baked into the baseline before you ever look at it. If fidelity to
> the shipped product matters, spend ~2 days first rendering reference output
> from the original binaries (they still run, under Rosetta or on Windows) and
> diff against those once. After that, the goldens are your contract.

**Done when:** `sw-tests` is green on macOS-arm64; goldens committed for all 57
effects; `rg LE_SW_AUTHORISATION_REQUIRED src` returns nothing.

---

### Stage 4 — Portable SIMD/FFT backend, and audio file I/O

**2–3 weeks.**

**4.1** Replace `LE_MATH_USE_NT2`, keeping the interfaces in `le/math/vector.hpp`
and `le/math/dft/fft.hpp` byte-identical so the 203 effect files do not move:
- vector primitives → `sst-basic-blocks` SIMD helpers over `simde` (which gives
  you SSE-on-NEON), or plain loops where the compiler auto-vectorises just as
  well — measure, don't assume
- FFT → Accelerate/vDSP on Apple (unchanged), `pffft` elsewhere

**4.2** `external_audio/` — delete `sampleWin.cpp` (DirectShow filter graphs) and
`sampleMac.cpp` (`ExtAudioFile` + the long-removed `FSRef`), replace with one
`sample.cpp` over `juce::AudioFormatManager`. This is also what finally removes
`FSRef` from `gui.hpp:226`.

**4.3** Run the goldens on all three OSes and both architectures.

**Done when:** `sw-tests` green on macOS (arm64 + x86_64), Windows (x64 +
arm64), Linux (x64 + arm64); NT2 is gone with no golden outside tolerance.

---

### Stage 5 — The CLAP host layer

**2.5–4 weeks.** The stage the whole plan is built around, and the one that most
resembles ordinary engineering rather than archaeology.

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

**5.7 — Audio ports.** Main stereo in/out plus the sidechain gated by
`LE_SW_ENGINE_INPUT_MODE`.

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

> **Why the host layer before the GUI.** The host's generic parameter panel is a
> complete, free test harness for the dynamic parameter model — the part of this
> port with the most novel risk. Proving it there, against five real DAWs,
> before writing a line of GUI code, means that when the GUI misbehaves in
> stage 6 you already know the parameter layer underneath it is sound.

---

### Stage 6 — GUI

**4–6 weeks.** The largest stage, and the one that is identical under either
plugin-format decision.

**6.1** JUCE 8 API drift, per scan §1.3 — `String::empty`, `Image::null`,
`ScopedPointer`, `ButtonListener`/`SliderListener`, the deprecated `Font`
constructors. A few days of grinding; the volume is small.

**6.2** `class Theme : juce::LookAndFeel` → `LookAndFeel_V4`. `LookAndFeel` has
eight pure virtuals in JUCE 8 and is no longer directly derivable.

**6.3 — Assets.** `assets/skin/*.png` → a CMakeRC resource library;
`resourceBitmap()` reads from `cmrc` instead of disk. This deletes
`GUI::initializePaths()`, `SpectrumWorx.paths`, `mapPathsFile()`, `boost::mmap`
and `resourcesPath()` — i.e. the plugin stops requiring the installer to have
run in order to start.

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

**Done when:** the full editor works in CLAP, VST3, AUv2 and standalone on all
three OSes; open/close cycles leak nothing; no separate desktop window exists
anywhere in the process.

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
0 ─┬─▶ 1 ─┬────────────▶ 3 ──▶ 4 ──▶ 5 ─┬──▶ 7 ──▶ 8 ──▶ 9
   │      │                             │
   └─▶ 2 ─┘                             │
          └──▶ 6 (GUI, in a harness) ───┘
```

- **0 → everything.** Nothing starts before the history rewrite.
- **1 and 2 are independent** and can run concurrently against the broken tree.
- **3 needs both** (1 for the build, 2 so it compiles without a Boost zoo).
- **6 can start as soon as 1 lands.** Port `src/gui` to JUCE 8 inside a small
  standalone JUCE harness app that instantiates the editor against a mock
  controller, then wire it to the real plugin after stage 5. This keeps the
  single biggest stage off the critical path, which is the main reason the whole
  project can be two people rather than one person for eight months.
- **7 needs 5 and 6** — you want a working plugin before refactoring its spine.

**Two-person split:** A takes 0, 1, 3, 4, 5, 8, 9 (build, DSP, host layer);
B takes 2 and 6 (Boost sweep, then GUI) and joins A on 7.

| Stage | | Weeks |
|---|---|---:|
| 0 | Purge and amputate | 1 |
| 1 | Walking skeleton | 1.5–2.5 |
| 2 | Boost tier-1 sweep | 1–2 |
| 3 | DSP core + goldens | 3–5 |
| 4 | Portable SIMD/FFT + audio I/O | 2–3 |
| 5 | CLAP host layer | 2.5–4 |
| 6 | GUI | 4–6 |
| 7 | De-Boost the parameter system | 4–6 |
| 8 | Presets and content | 1–2 |
| 9 | Ship | 1–2 |
| | **Serial total** | **21.5–33.5** |
| | **Two people, 6 in parallel** | **~17–25** |

---

## Risk register

| # | Risk | Mitigation | Stage |
|---|---|---|---|
| 1 | **Thread discipline.** CLAP's main/audio thread split is contractual; the 2016 code has no lock-free parameter queue. | Adopt the sst patch/patchMain pattern from day one of stage 5; rtsan in CI; thread-identity asserts. | 5 |
| 2 | **Parameter refactor silently reorders parameters**, breaking saved state — invisible to audio tests. | Commit a full parameter-table snapshot test *before* starting (7.0). | 7 |
| 3 | **Owned-window collapse is a redesign, not a port.** Two panels need re-laying-out. | Scope it as such; do not fold it into "port the GUI". | 6 |
| 4 | **SIMD/FFT swap changes DSP output.** | macOS-first bring-up on Accelerate, goldens captured before the swap. | 3 → 4 |
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
