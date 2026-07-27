# SpectrumWorx — Initial Modernisation Scan

## The ask

> This directory contains the 2016 instance of SpectrumWorx plugins which don't compile. I want to make it more
> like surge family products. OB-Xf is probably a good guide but shortcircuit may be also.
>
> At first I want to just do an analysis pass and create `docs/initial_scan.md`
>
> The things I want to cover
>
> 1. It uses an ancient version of JUCE. Estimate of work to pull to juce 8.0.12 as a submodule
> 2. It has several other external libraries. What is their provenance, version, and usage
> 3. It contains features like a license manager which we don't need in open source
> 4. There is no top level cmake. The cmakelists in source does a build but is ancient
> 5. It seems there's an implicit dependence on boost. How wide spread is that dependence
>    and can a move to C++20 remove it
> 6. The GUI framework seems to jsut be a wrapper on juce gui. Are there any platform specific
>    issues?
> 7. The installer framework can probably be tossed if we move to surge-style installers but
>    does it couple with the code
> 8. What else is in this directory which needs consideration if we want to port this to modern
>    3-os vst3 + au + clap type plugin

---

Analysis of the 2016 SpectrumWorx snapshot with a view to bringing it in line with
Surge Synth Team practice (OB-Xf as the reference model: a standard JUCE 8 plugin
with a top-level CMake project, submoduled dependencies, embedded binary assets,
and `basic_installer`-based packaging).

**Target shape**: VST3 + AU + CLAP (+ Standalone/LV2) on macOS (x86_64 + arm64) /
Windows / Linux, C++20, no VST2, JUCE 8 for the GUI, submoduled dependencies,
embedded binary assets, `basic_installer` packaging.

**One open question** — how the host layer gets there. §1.4/§1.5 cost a standard
JUCE `AudioProcessor` port; **§1.6 argues the alternative** (port the existing
plugin framework to native CLAP and use clap-wrapper for VST3/AUv2/standalone),
which the code turns out to fit rather better. That decision is not settled
here; §9 phase 2b proposes a one-week spike to settle it.

Date of scan: 2026-07-27. All file/line counts below were measured against the
tree at commit `3fd37b5`.

---

## 0. Inventory

### 0.1 Top-level layout

| Path | Size | What it is |
|---|---|---|
| `3rd_party/` | 33 MB | Vendored JUCE 2.1.2 source (patched) + zip archives of LibTomCrypt/LibTomMath/RapidXML |
| `source/` | 54 MB | The plugin. `source/externals/nt2` alone is ~53 MB / 10,134 files |
| `installer/` | 4.9 MB | CPack/WiX/PackageMaker inputs, **plus the GUI skin PNGs, factory presets and sample content** |
| `doc/` | 1.2 MB | `.doc`/`.PDF` manuals, screenshots, `readme.txt` |

Repo is a 4-commit import from an SVN tree; pack size 19.3 MB. No submodules.
Licence at root is **GPL-3.0**; every source header still carries
"Copyright (c) … Little Endian Ltd. All rights reserved."

### 0.2 Code volume (`.cpp/.hpp/.inl/.h/.mm`)

| Subsystem | Files | Lines | In plugin build? |
|---|---:|---:|---|
| `source/` root (`spectrumWorx.cpp` + VST24/AU shims) | 8 | ~3,000 | yes |
| `source/core` (host interop, module chain, automation) | 28 | 5,467 | yes |
| `source/gui` | 16 | 11,277 | yes (if `LE_SW_GUI`) |
| `source/external_audio` (side-chain sample loading) | 4 | 1,564 | yes |
| `externals/le/spectrumworx/effects` | 203 | 23,042 | yes (33 of which are `_unfinished`, excluded) |
| `externals/le/spectrumworx/engine` | 28 | 6,966 | yes |
| `externals/le/parameters` | 50 | 6,382 | yes |
| `externals/le/math` (vector + FFT) | 13 | 5,506 | yes |
| `externals/le/utility` | 49 | 6,470 | partially |
| `externals/le/plugins` (VST2.4 / AU / FMOD / Unity wrappers) | 24 | 10,564 | VST2.4 + AU only |
| `externals/le/licenser` + `license_key` | 18 | ~2,400 | yes (if `LE_SW_AUTHORISATION_REQUIRED`) |
| `externals/le/analysis` (pitch/peak detect, scales) | 6 | 1,412 | yes |
| `externals/le/audioio` (incl. patched RtAudio) | 25 | 20,661 | **no — dead weight** |
| `externals/nt2` | 10,134 | — | headers only, pulled in by `le/math` |

**Total excluding nt2: 115,878 lines. Excluding nt2, audioio and `_unfinished`
effects: ~91,300 lines.** That is the code you actually have to carry forward,
and about 30 % of it (the effects) is genuinely valuable DSP.

There are **no tests** anywhere in the tree.

---

## 1. JUCE: 2.1.2 → 8.0.12

### 1.1 What's there now

`3rd_party/JUCE/trunk` is a full JUCE **2.1.2** source drop (2013). It is not a
submodule and not stock:

- **276 of the 1,228 text files under `modules/` are patched**, with 2,785 occurrences of the
  `LE_PATCH(...)` / `JUCE_ORIGINAL(...)` / `LE_PATCHED_JUCE` macro family
  defined in `3rd_party/JUCE/trunk/AppConfig.h:120-159`.
- Distribution of patched files: `juce_gui_basics` 140, `juce_core` 65,
  `juce_graphics` 34, `juce_events` 21, `juce_data_structures` 7,
  `juce_gui_extra` 5, `juce_audio_utils` 2, `juce_audio_devices` 2.
- The patches are almost entirely *micro-optimisations*, not behaviour changes:
  adding `noexcept`, passing `Point<int>` by const-ref, `__declspec(novtable)`,
  `__assume`, weak symbols, and `#ifdef`-ing out unused features
  (`Component::Positioner::applyNewBounds` is one of the few genuine API
  removals).
- Only 5 modules are compiled: `juce_core`, `juce_data_structures`,
  `juce_events`, `juce_graphics`, `juce_gui_basics`
  (`3rd_party/JUCE/trunk/CMakeLists.txt:46-54`). It is built as a **separate
  static library** and linked by name via
  `$ENV{LEB_3rdParty_root}/juce/trunk/lib` (`juce.cmake:20-26`).

**Good news:** nothing in `source/` uses the `LE_PATCH` macros. The patched
fork can be deleted wholesale and replaced with a stock JUCE 8 submodule; you
lose some hand-tuning, nothing functional.

### 1.2 What the plugin actually uses from JUCE

Roughly 60 distinct types. The top of the histogram:

```
152 Component   149 String   81 File   70 Colours   64 Slider   62 Image
 61 Graphics    46 MouseEvent   30 Justification   26 ComponentPeer
 25 Font        23 TextEditor   21 Colour   19 PopupMenu   17 Desktop
 16 Button      14 MessageManager   11 AlertWindow   11 Rectangle
```

Concentrated almost entirely in `source/gui/` (11.3 k lines), plus
`external_audio/sample.hpp` and `spectrumWorx.hpp`.

### 1.3 Concrete API drift (verified against JUCE 8.0.10 in `~/dev/music/sst/OB-Xf/libs/JUCE`)

| Usage | Count | Status in JUCE 8 | Fix |
|---|---:|---|---|
| `juce::String::empty` | 14 | Exists only under `JUCE_ALLOW_STATIC_NULL_VARIABLES`, **default 0** | `{}` / `String()` |
| `juce::Image::null` | 4 | Same gate | `Image()` |
| `juce::ScopedPointer` | 1 | **Removed** | `std::unique_ptr` |
| `juce::ButtonListener` | 8 | Typedef **removed** | `Button::Listener` |
| `juce::SliderListener` | 3 | Typedef **removed** | `Slider::Listener` |
| `Font(float, int)`, `Font(String,float,int)` | ~25 | `[[deprecated]]` | `Font(FontOptions{...})` |
| `class Theme : public juce::LookAndFeel` | 1 | `LookAndFeel` has 8 pure virtuals; not directly derivable | derive `LookAndFeel_V4` (or `_V2`) |
| `getMenuWindowFlags`, `getDefaultFolderImage`, `drawLinearSliderThumb`, `drawTabAreaBehindFrontButton` | 5 | still present on `LookAndFeel_V2/V4` | re-check signatures |
| `juce::ComponentPeer::getNumPeers/getPeer` | 28 | still present | see §6 — the *design* is the problem, not the API |
| `juce::InternalTimerThread` | 2 | internal, referenced only in comments | drop |
| `LE_OVERRIDE` / `LE_SEALED` macros | many | — | plain `override` / `final` |

None of that is hard. The volume is small enough to be a few days of grinding.

### 1.4 The real cost is not the JUCE version — it's the plugin model

Today, SpectrumWorx is **not** a `juce::AudioProcessor`. `juce_audio_processors`
isn't even compiled. Instead:

- `le/plugins/` is a bespoke CRTP plugin framework
  (`plugin.hpp`, `PluginCapability` + `boost::mpl::set_c` capability sets)
  with backends for VST 2.4 (`plugin.cpp/.hpp/.inl`, 2,256 lines), AU
  (2,597 lines + a 1,296-line `properties.hpp`), FMOD and Unity.
- The editor is a raw `juce::Component` that the wrapper attaches to the host's
  native window handle by hand (`gui.mm:242 attachComponentToHostWindow`,
  `gui.cpp:763 addToDesktop(..., owner.getNativeHandle())`).

Taking the JUCE-first route means deleting `le/plugins` and rewriting
`spectrumWorx.cpp` (2,470 lines) as an `AudioProcessor`, and
`spectrumWorxEditor.cpp` (2,663 lines) as an `AudioProcessorEditor`. That work
subsumes both "upgrade JUCE" and "support VST3/AU/CLAP" — they are one job, not
two.

The estimate in §1.5 costs that route. **§1.6 makes the case for the other one**
— keeping the existing framework and giving it a native CLAP backend — which on
inspection fits this codebase better.

### 1.5 Estimate

| Task | Estimate |
|---|---|
| Delete patched JUCE, add JUCE 8 submodule, get `source/gui` to compile against it | 1.5–2.5 wk |
| Rewrite host layer as `AudioProcessor` + `AudioProcessorEditor`, delete `le/plugins` | 3–4 wk |
| Restructure the "owned window" GUI model (§6) | 2–3 wk |
| **Total** | **6.5–9.5 engineer-weeks** |

### 1.6 Alternative: native CLAP + clap-wrapper

There is a second route that deserves serious consideration, and on inspection
the code argues for it: **port `le/plugins` to a native CLAP backend and use
[clap-wrapper](https://github.com/free-audio/clap-wrapper) to emit VST3, AUv2
and standalone**, rather than rewriting the host layer as a
`juce::AudioProcessor`.

This is the six-sines / SideQuest idiom (clap-first, JUCE only for the GUI)
rather than the OB-Xf idiom (JUCE-first with `clap-juce-extensions` bolted on).
Both are Surge-family patterns.

#### 1.6.1 The framework is already shaped like CLAP

The LE plugin framework abstracts *how a host addresses parameters* as a
protocol trait, and already supports both models:

- `le/plugins/plugin.hpp:388-399` — `struct ParameterIndex` and `struct ParameterID`
- `le/plugins/vst/2.4/plugin.hpp:454` — `using ParameterSelector = ParameterIndex`
- `le/plugins/au/plugin.hpp:574` — `using ParameterSelector = ParameterID`

and `SW::ParameterID` (`core/parameterID.hpp:27-64`) is already a packed
`uint32` of `{type, moduleIndex, moduleParameterIndex, lfoParameterIndex}` —
i.e. `clap_param_info.id` with **zero translation**.

`Plugin2HostInteropControler` (`core/host_interop/plugin2Host.hpp:136-152`) is a
pure-virtual interface whose entire surface maps onto CLAP:

| LE framework | CLAP |
|---|---|
| `automatedParameterBeginEdit` / `EndEdit` | `CLAP_EVENT_PARAM_GESTURE_BEGIN` / `_END` |
| `automatedParameterChanged(ParameterID, value)` | `CLAP_EVENT_PARAM_VALUE` |
| `parameterListChanged()` | `clap_host_params::rescan(INFO\|TEXT)` |
| `moduleChanged(index, Module const*)` | `clap_host_params::rescan` |
| `latencyChanged()` | `clap_host_latency::changed()` |
| `presetChangeBegin` / `End` | `clap_host_state::mark_dirty` |
| `hostTryIOConfigurationChange` | `clap_host_audio_ports::request_rescan` |
| `effEditOpen(ptr)` → `createGUI(WindowHandle)` (`vst/2.4/plugin.inl:555-557`) | `clap_plugin_gui::set_parent` |
| `effEditGetRect` (`plugin.inl:586-619`) | `clap_plugin_gui::get_size` |
| `effGetChunk` / `effSetChunk` (`plugin.inl:303,355`) | `clap_plugin_state::save` / `load` |

The GUI layer is likewise already written in the clap-first idiom, not the
`AudioProcessorEditor` idiom: `gui.cpp` calls `juce::initialiseJuce_GUI`,
manages a `MessageManagerLock`, and attaches via
`addToDesktop(0, parentView/HWND)` — precisely the CLAP GUI contract.

#### 1.6.2 The deciding factor: dynamic parameter lists

See §8.3. SpectrumWorx's parameter *slots* are fixed (5 modules × 10), but each
slot's **name, range, unit and meaning change when you load a different effect
into it**. The framework treats this as first-class:
`useDynamicParameterLists()` (`vst/2.4/plugin.hpp:968`),
`dynamicParameterAccessContext()` (`core/spectrumWorxCore.hpp:163`), and a
`Program const*` context threaded through every `getParameterName` /
`getParameterLabel` / `getParameterDisplay`.

| Protocol | Dynamic parameter support |
|---|---|
| **CLAP** | first-class — `clap_host_params::rescan(CLAP_PARAM_RESCAN_INFO\|TEXT\|VALUES)` |
| **VST3** | `restartComponent(kParamTitlesChanged)`; honoured unevenly across hosts |
| **JUCE `AudioProcessorParameter`** | **worst fit** — parameters are objects constructed once with fixed `getName()`/`getLabel()`/ranges |

Under JUCE you would either expose 50 generic "Mod 3 Param 7" slots, or subclass
`AudioProcessorParameter` to return changing names and lean on
`updateHostDisplay(ChangeDetails().withParameterInfoChanged(true))`. Both work;
both mean fighting the framework in exactly the place where SpectrumWorx is
unusual.

#### 1.6.3 Costs of the CLAP route

1. **It does not avoid the JUCE work.** `source/gui` is 11.3 k lines of
   `juce_gui_basics`, so §1.3's API drift and §6.2's owned-window redesign are
   unchanged. You skip `juce_audio_processors` and nothing else. The
   owned-window problem gets marginally *worse*: `clap_plugin_gui` is
   emphatically a single embedded view.
2. **Threading becomes contractual.** CLAP's `[main-thread]` / `[audio-thread]`
   annotations are part of the spec. Today there is a `BackgroundThread`, a
   `GUI::Lock` over `MessageManagerLock`, and no visible lock-free parameter
   queue. Adopting the OB-Xf/SideQuest patch-vs-patchMain discipline is +1–2
   weeks. (This is a latent bug class today regardless; JUCE would simply let
   you keep ignoring it.)
3. **Sample-accurate automation.** CLAP delivers parameter changes as an
   in-block event list with sample offsets; the current design is block-boundary
   `setParameter`. Draining the event list at block start is legal, so this is
   not a blocker — just unfinished business.
4. **Less host-compat armour.** `juce_audio_processors` carries a decade of
   per-DAW workarounds. clap-wrapper is good and actively maintained by the same
   people, but has been exercised against fewer hosts.
5. **It diverges from OB-Xf**, which affects how easily a Surge-family
   contributor can drop in. Worth a deliberate decision rather than drift.

#### 1.6.4 Comparison

| | AudioProcessor route | Native CLAP route |
|---|---|---|
| JUCE 8 GUI compile fixes | 1.5–2.5 wk | 1.5–2.5 wk |
| Host layer | 3–4 wk — rewrite `spectrumWorx.cpp`, delete `le/plugins` **and** `core/host_interop` | 2–3 wk — new backend, **keep** `core/host_interop` |
| Wrapper integration | — | 0.5–1 wk (clap-wrapper → VST3/AUv2/standalone) |
| Owned-window redesign | 2–3 wk | 2–3 wk |
| Thread discipline | deferred | 1–2 wk |
| **Total** | **6.5–9.5 wk** | **7–11.5 wk** |
| Code discarded | `le/plugins` (10.5 k) + `core/host_interop` (2.3 k) | `le/plugins` VST2/AU/FMOD/Unity backends only (~10.5 k), interop retained |
| Parameter model | fought | native |

Schedule is roughly a wash; the CLAP route discards ~2.3 k fewer lines and stops
fighting the parameter model.

#### 1.6.5 Recommendation

**Spike it before committing.** The entire bet is that
`Plugin2HostInteropControler` is a clean CLAP adapter target. Writing that one
backend against the existing interface answers the question in about a week, and
is not wasted effort either way — the interface has to be understood before any
host-layer work begins.

---

## 2. Other external libraries

| Library | Version | Where | Licence | Used for | Verdict |
|---|---|---|---|---|---|
| **Boost** | 1.61.0 (2016) | *not vendored* — auto-downloaded from SourceForge by `buildOptions.cmake:38-45` into `$LEB_3rdParty_root` | BSL-1.0 | pervasive; see §5 | must be dealt with |
| **NT2 / Boost.SIMD** | unversioned snapshot, ~2015 | `source/externals/nt2` (10,134 files, 53 MB) | BSL-1.0 | SIMD vector math + FFT on non-Apple platforms | replace |
| **RapidXML** | 1.13 | zip in `3rd_party/RapidXML/1.13`, expected pre-unpacked at `$LEB_3rdParty_root` | Boost/MIT dual | preset XML + licence-file XML (`le/utility/xml.*`, `spectrumworx/presets.cpp`) | replace with `juce::XmlDocument` or keep |
| **LibTomCrypt** | 1.17 + LE patches | zip in `3rd_party/` | public domain / WTFPL | **nothing** — historical; superseded by `boost::multiprecision` in the licenser | delete |
| **LibTomMath** | 0.42 + LE patches | zip in `3rd_party/` | public domain | **nothing** | delete |
| **JUCE** | 2.1.2, heavily patched | `3rd_party/JUCE/trunk` | GPLv2+/AGPLv3/commercial at the time | GUI + core utilities | replace with JUCE 8 submodule |
| **VST 2.4 SDK** | 2006 | was `le/plugins/vst/2.4/aeffect.h`, `aeffectx.h` | **Steinberg proprietary** | VST2 wrapper | **deleted** 2026-07-27 — see §8.1 |
| **RtAudio** (patched) | — | `le/audioio/device/patchedRtAudio` | MIT | standalone audio I/O; **not in the plugin build** | delete |
| **Boost "sandbox" libs** | — | `source/externals/boost/{hash,mmap,filesystem}` | BSL-1.0 (proposed-to-Boost code) | licensing hashes; memory-mapped `.paths` file; dir iteration | delete with §3/§7 |

Two structural problems with how these are consumed:

1. **`$LEB_3rdParty_root`.** `core/configuration.cmake:127` and `juce.cmake:17`
   hard-require an environment variable pointing at a machine-local library
   tree. Nothing builds without it. The zips in `3rd_party/` are just a cache of
   what you were supposed to unpack there.
2. **`3rd_party/` contains build *instructions*, not usable libraries.**
   `LibTomCrypt/readme.txt` etc. describe manually building `.lib` files and
   copying them into `P:/3rdParty/...`.

Everything here should become git submodules under `libs/` in the OB-Xf style.

---

## 3. Licence manager

### 3.1 Extent

Smaller and cleaner than feared. The whole thing is behind
`LE_SW_AUTHORISATION_REQUIRED`, a CMake feature option
(`core/configuration.cmake:52`) with a **working `#else` branch already in
place** (`spectrumWorx.hpp:301-304` supplies a `static authorised() { return true; }`).

Files that reference it at all:

- `source/spectrumWorx.cpp` — the bulk. Lines ~110–1238 are licence parsing,
  RSA signature verification, the demo-crippling thread, and the anti-tamper
  "targetAuthorizationDataAntiHack" array games.
- `source/spectrumWorx.hpp:263-305` — `AuthorisationData`, `authorizationThread_`,
  `demoGain()`, `isCurrentlyCrippled()`, `.SWLic` extension macros.
- `source/core/spectrumWorxSharedImpl.inl:66-85` — chooses between
  `initialisePathsAndVerifyLicence()` and plain `GUI::initializePaths()`.
- `source/gui/preset_browser/presetBrowser.cpp:165-171` — gates preset saving.
- `source/gui/editor/spectrumWorxEditor.{hpp,cpp}` — a whole **"Registration"
  settings tab** (`RegistrationPage`, `showRegistrationPage()`, Authorize /
  Buy Now buttons, ~120 lines) at tab index 2 of the Settings window.
- `externals/le/licenser/` (14 files, 2,137 lines) and
  `externals/le/license_key/` (4 files).
- `source/externals/boost/hash/` (the sandbox SHA/MD5 library) exists **only**
  for `licenser/cryptography/hashing.cpp`.

### 3.2 Removal plan

1. Delete `externals/le/licenser`, `externals/le/license_key`,
   `source/externals/boost/hash`.
2. Delete the `SOURCES_Externals__License` block from `CMakeLists.txt:126-137`.
3. Excise the `#if LE_SW_AUTHORISATION_REQUIRED` blocks and the `#else` shims,
   and delete `demoLimiter.hpp`, `outputGainScale_`, `demoCripplingThread()`.
4. Remove the Registration tab and re-lay-out the Settings window (three tabs
   instead of four; `SettingsRegBg`/`SettingsRegDoneBg`/`Authorize*`/`BuyNow*`
   bitmaps become unused).
5. Drop `boost::multiprecision` (only used by `licenser/cryptography/bigInt.hpp`).

### 3.3 :rotating_light: Committed private key

`source/externals/le/license_key/` contains **`le_key_01.pem` and
`le_key_01_private.cpp` — a 4096-bit RSA private key**, already published in
this public repo. It's dead weight for an open-source build, but it should be
deleted deliberately, and if Little Endian ever reused that key elsewhere they
should be told.

Deleting it from HEAD does **not** remove it from git history. Fold this into
the single `git filter-repo` pass described in §8.1 — same rewrite, same
timing argument.

**Estimate: 3–5 days** including the Settings re-layout.

---

## 4. Build system

### 4.1 Current state

- No top-level `CMakeLists.txt`. The root is `source/CMakeLists.txt` (793
  lines), so `CMAKE_SOURCE_DIR` *is* `source/` and everything else is reached as
  `${CMAKE_SOURCE_DIR}/../installer/...`.
- `cmake_minimum_required(VERSION 3.5)`, `cmake_policy(SET CMP0043 OLD)` —
  CMP0043 was removed in CMake 4.x, so this **cannot even parse** on a current
  CMake.
- Supporting files: `le/build/buildOptions.cmake` (1,073+ lines of hand-rolled
  per-compiler flag logic), `utilities.cmake`, `3rdPartyLibs.cmake` (downloads
  and untars Boost at configure time), `crossCompilingHelper.cmake`,
  `android.toolchain.cmake`, `iOS.toolchain.cmake`.
- Targets a **prebuilt** JUCE static lib and a machine-local `$LEB_3rdParty_root`.
- Two dozen `configure_file()` calls **write generated headers back into the
  source tree** (`effects/configuration/*.hpp`,
  `configuration/versionConfiguration.hpp`, `plugins/au/resources.r`,
  `installer/resources/OSX/*/Info.plist`). In-source generation; will fight
  git and parallel/multi-config builds.
- References directories that **don't exist** in this repo: `source/fmod_lib/`,
  `source/externals/boost/profile_templates2/`, `externals/LEB/buildOptions.cmake`.
- Assumes Visual Studio 2013 / Xcode 7.3 / CMake 3.5, `x86`/`x86_64` only,
  `MACOSX_DEPLOYMENT_TARGET 10.7`. **No arm64 path at all** — Apple Silicon is
  a from-scratch target.
- Post-build steps `mkdir -p /Library/Audio/Plug-Ins/...` and symlink into
  system directories — needs root, would fail on a modern machine.

### 4.2 Target

Model directly on `~/dev/music/sst/OB-Xf/CMakeLists.txt`:

```
/CMakeLists.txt              project(SpectrumWorx), C++20, juce_add_plugin(...)
/libs/CMakeLists.txt         submodules: JUCE, clap-juce-extensions,
                             sst-basic-blocks, sst-plugininfra, sst-cpputils,
                             simde, fmt, sst-cmake
/src/                        the plugin (relocated from source/)
/assets/CMakeLists.txt       juce_add_binary_data(BinaryData ...) for the skin
/cmake/                      basic_installer via sst-cmake
/.github/workflows/          build-plugin.yml, code-checks.yml
```

`juce_add_plugin(FORMATS VST3 AU CLAP Standalone)` plus
`clap_juce_extensions_plugin(...)` replaces the entire hand-written
`le/plugins` layer and all of the CPack/WiX/PackageMaker machinery.

Work items beyond the boilerplate:

- Move the effects-list code generation to `${CMAKE_BINARY_DIR}` (or, better,
  replace `effectsList.cmake`'s string-concatenation codegen with a plain C++20
  `constexpr` table — 57 effects, entirely mechanical).
- Version info via `sst-plugininfra`'s `version_from_versionfile_or_git`.
- Delete `buildOptions.cmake` outright; the per-flag tuning it encodes is
  either obsolete or better expressed as three lines of
  `target_compile_options`.

**Estimate: 1.5–2.5 weeks** for a top-level CMake that builds all four formats
on three OSes, assuming the source compiles.

---

## 5. Boost

### 5.1 How wide is it?

**220 files include a Boost header** (excluding nt2 and the vendored
`externals/boost` sandbox). But that number overstates the difficulty — most of
it is shallow. Sorted by removal difficulty:

#### Tier 1 — mechanical, direct `std` replacements (~1–2 weeks)

| Boost | Count | C++20 replacement |
|---|---:|---|
| `boost/assert.hpp` | 77 | `assert` or a project macro |
| `boost/config/abi_{prefix,suffix}.hpp` | 102 | delete (pure noise) |
| `boost/range/iterator_range_core.hpp` | 21 | `std::span` / `std::ranges::subrange` |
| `boost/utility/string_ref.hpp` | 17 | `std::string_view` |
| `boost/smart_ptr/intrusive_ptr.hpp` | 12 | `std::shared_ptr` or keep a tiny local |
| `boost/noncopyable.hpp` | 8 | deleted copy ctor |
| `boost/polymorphic_cast.hpp` | 8 | `dynamic_cast` + assert |
| `boost/optional` | 6 | `std::optional` |
| `boost/core/ignore_unused.hpp` | 6 | `[[maybe_unused]]` |
| `boost/range/algorithm/*` | ~12 | `std::ranges::*` |
| `boost/detail/endian.hpp`, `boost/cstdint.hpp`, `boost/array.hpp`, `boost/integer*`, `boost/ref.hpp`, `boost/mem_fn.hpp`, `boost/checked_delete.hpp`, `boost/limits.hpp`, `boost/smart_ptr/scoped_array.hpp`, `boost/concept_check.hpp` | ~30 | `std::endian`, `<cstdint>`, `std::array`, `<bit>`, `std::ref`, `std::mem_fn`, `delete`, `<limits>`, `std::unique_ptr<T[]>`, concepts |

#### Tier 2 — bounded, tied to features being deleted anyway

| Boost | Where | Fate |
|---|---|---|
| `boost::multiprecision` | `licenser/cryptography/bigInt.hpp` | deleted with §3 |
| `boost::hash` (sandbox) | `licenser/cryptography/hashing.cpp` | deleted with §3 |
| `boost::random` | licence key generation | deleted with §3 |
| `boost::mmap` (sandbox, 20 files) | `gui.cpp` `mapPathsFile()` — reads the installer-written `.paths` file | deleted with §7 (embed assets instead) |
| `boost::filesystem::directory_iterator` (sandbox) | `le/utility/filesystem*` | `std::filesystem` |
| `boost::signals2::mutex` | `external_audio/sample.hpp` Windows only | `std::mutex` |
| `boost::lockfree::spsc_queue` | `le/audioio` only — **not in the plugin build** | deleted with audioio |

#### Tier 3 — architectural. This is the real Boost dependency.

**(a) Boost.Fusion + Boost.MPL parameter system.**
`le/parameters/factoryMacro.hpp:157-177` defines `LE_DEFINE_PARAMETERS`, which
declares each effect's `Parameters` struct **as a Boost.Fusion associative
sequence** (`fusion_sequence_tag`, `forward_traversal_tag`, `associative_tag`,
plus hand-written `begin_impl`/`end_impl`/`at_impl`/`value_at_key_impl`
specialisations in `fusionAdaptors.hpp`). Everything downstream iterates those
sequences generically:

- automation / parameter enumeration (`core/host_interop/parameters.hpp`)
- preset serialisation (`spectrumworx/presets.cpp`)
- **automatic GUI generation** — per `doc/readme.txt`, "individual effect/module
  GUIs are automatically generated through compiletime introspection of their
  DSP specification"
- `le/parameters/printer.hpp`, `uiElements.hpp`, `runtimeInformation.hpp`

Files touched: 12 include `boost/fusion` directly, 35 include `boost/fusion` or
`boost/mpl`; 57 effect headers use the macro.

This *can* be replaced in C++20 — a `std::tuple` of parameter types plus
`std::index_sequence` fold expressions gives you the same compile-time
introspection with far less machinery, and `LE_DEFINE_PARAMETERS` can keep the
same call syntax so the 57 effect headers barely change. But it is the core
abstraction of the codebase and the riskiest single refactor here.
Also note the preprocessor sequence syntax (`( (Name)(Type)(Minimum<-48>)... )`)
depends on Boost.Preprocessor (`BOOST_PP_SEQ_*`); either keep just that header,
or move to variadic macros / a designated-initialiser table.

Watch out for `Unit<' dB'>` — multi-character literals as non-type template
parameters. Legal but implementation-defined and warning-generating; C++20
class-type NTTPs (`Unit<"dB">`) are the clean replacement.

**(b) Boost.SIMD / NT2.**
`le/math/vector.cpp` (1,500+ lines) and `le/math/dft/fft.cpp`:

- On **Apple**: `LE_MATH_USE_ACC` → Accelerate/vDSP for both vector ops and FFT.
  Already fine.
- Everywhere else: `LE_MATH_USE_NT2` → `boost/simd/*` and
  `nt2/signal/static_fft.hpp`.
- However `vector.cpp` includes the nt2 headers **unconditionally** at file
  scope, so nt2 is required to compile even on macOS.

Replacement options, in order of preference:
1. `sst-basic-blocks` (already a Surge submodule) + `simde` for the vector ops;
   `juce::dsp::FFT` or `pffft` for the transform. This is what OB-Xf/Surge use.
2. Keep Accelerate on Apple, use `juce::dsp::FFT` elsewhere, and hand-write the
   ~40 vector primitives with plain loops + `-ffast-math`; the compiler
   auto-vectorises most of them fine on modern toolchains.

Deleting nt2 removes **53 MB and 10,134 files** — the single biggest cleanup in
the repo. Note the vendored copy has 30 LE-modified files, including
`nt2/signal/static_fft.hpp`, which appears to be Little Endian's own
contribution to NT2 — worth reading before discarding, as its FFT may be
faster than the alternatives.

### 5.2 Answer to "can C++20 remove Boost?"

**Yes, entirely** — nothing survives Tier 1–3 that needs Boost proper. But it is
not a side effect of switching to C++20; Tier 3 is real design work.

**Recommended sequencing:** do *not* try to remove Boost first. Add Boost as a
submodule (or via `find_package`) to get a green build on JUCE 8 first, then
remove it in tiers. Tier 1 can be done incrementally by anyone; Tier 3 wants one
person with a clear head.

| Tier | Estimate |
|---|---|
| Tier 1 (mechanical std swaps) | 1–2 wk |
| Tier 2 (falls out of §3 and §7) | ~0 incremental |
| Tier 3a (Fusion/MPL parameter system) | 4–6 wk |
| Tier 3b (NT2 → sst-basic-blocks / juce::dsp) | 2–4 wk |
| **Total** | **7–12 engineer-weeks** |

---

## 6. GUI framework and platform-specific issues

### 6.1 It is a thin widget set, not a framework

`source/gui/gui.hpp` (1,105 lines) defines a compact set of skinned widgets on
top of `juce_gui_basics`: `WidgetBase<>`, `BitmapButton`, `Knob`, `EditorKnob`,
`ComboBox`, `TitledComboBox`, `PopupMenu`, `BackgroundImage`, `DrawableText`,
`Theme : juce::LookAndFeel`. Plus `postMessage()` helpers over
`juce::MessageManager::MessageBase`. Nothing exotic — this ports cleanly.

### 6.2 The genuine platform problem: the "owned window" model

`OwnedWindow<>` / `OwnedWindowBase` (`gui.hpp:377-464`) make the **preset
browser and the settings panel separate top-level desktop windows**, parented to
the editor's *native* window handle and manually kept in position as the editor
moves. Implementation:

**Windows** (`gui.cpp:790-860`):
- A global `SetWindowsHookEx`/`CallWndProc` hook (`OwnedWindowBase::wndProcHook`)
  watching for host window moves.
- `peerWithParentHandle(HWND)` walks `juce::ComponentPeer::getNumPeers()` to
  find the editor peer by parent HWND.
- `addToDesktop(windowIsSemiTransparent, owner.getNativeHandle())`.

**macOS** (`gui.mm`):
- `makeEditorChild()` / `detachFromEditor()` do
  `[NSWindow addChildWindow:ordered:]` between the editor's and the child's
  `NSWindow`s.
- `attachComponentToHostWindow(Component&, WindowRef)` — a **Carbon**
  implementation using `HIViewCreate`, `HIViewAddSubview`,
  `InstallWindowEventHandler`, `GetWindowBounds`, and a "dummy HIView" position
  tracker, lifted from JUCE's old `juce_VST_wrapper.mm`. Guarded by
  `#if !JUCE_64BIT`, so the 64-bit path is much simpler — but
  `#include <Carbon/Carbon.h>` is unconditional at the top of the file, and
  `source/CMakeLists.txt:364-368` + `juce.cmake:50` link `-framework Carbon`.
- `FSRef makeFSRefFromPath()` (`gui.hpp:226`) — `FSRef` is deprecated since
  10.8 and gone from the modern SDK; used by `sampleMac.cpp` for
  `ExtAudioFileOpen`.
- `CGDisplayHideCursor` / `CGDisplayShowCursor` for the knob-drag cursor hide.

**None of this survives.** Modern plugin hosts (and every VST3/AU/CLAP wrapper)
expect a single editor view. Separate desktop windows parented to a host window
break on Retina/scaling, break in sandboxed AUv3, break with Wayland, and are
actively hostile in a CLAP `gui` extension.

**Fix:** make the preset browser and settings ordinary child `Component`s of the
`AudioProcessorEditor`, overlaid (like OB-Xf's menus/patch browser) or in a
`juce::CallOutBox`. This is straightforward, but it does mean re-laying-out both
panels and deleting `gui.mm` almost in its entirety, plus the whole
`OwnedWindowBase` Win32 hook. Budget it as a redesign, not a port.

### 6.3 Other platform-specific items

| Item | Location | Issue |
|---|---|---|
| Skin PNGs loaded from disk | `gui.cpp:475-491` `resourceBitmap()` → `resourcesPath()/NN.png` | not embedded; depends on installer. See §7 |
| Path discovery via mmapped `.paths` file | `gui.cpp:292-425`, `boost::mmap` | replace with `sst-plugininfra` paths or BinaryData |
| `displayScale()` | `gui.hpp:205` | JUCE 8 handles this; also there is no HiDPI asset set — the skin is fixed-size bitmaps |
| Windows sample loading | `external_audio/sampleWin.cpp` | **DirectShow** (`strmif.h`, filter graphs). Replace with `juce::AudioFormatManager` |
| macOS sample loading | `external_audio/sampleMac.cpp` | `ExtAudioFile` + `FSRef`. Same replacement |
| No Linux support at all | everywhere | new target |
| `-framework Carbon`, `-framework QuartzCore` | `juce.cmake:50-53` | drop |
| AU registration | `le/plugins/au/plugin.hpp:57` includes `CarbonCore.framework/Headers/Components.h`; `.r`/`.rsrc` Rez resources built at `CMakeLists.txt:339-345` | **Component Manager AU registration is dead.** Deleted along with `le/plugins` |
| MSVC-only pragmas | `gui.hpp:407`, `gui.hpp:433` `#pragma warning`, `LE_ASSUME(this != 0)` | UB-adjacent; delete |
| x86/SSE2 assumptions | `buildOptions.cmake:238-243`, `CMakeLists.txt:260-261` | need arm64/NEON |

**Estimate for §6 (folded into §1.5 above): 2–3 weeks**, dominated by the
owned-window redesign.

---

## 7. Installer — and how much it's coupled to the code

### 7.1 What's there

- **CPack + PackageMaker** on macOS (`CMakeLists.txt:535`). PackageMaker was
  removed from Xcode in 2012 and the CMakeLists says so in a comment.
- **CPack + WiX** on Windows, with a hand-maintained `files.wxs.in` /
  `features.wxs` / `directories.wxs` and a `PackageLECustom` target that shells
  out to `candle`/`light` because "the PACKAGE build step will currently fail
  due to incomplete WiX support in CMake" (`doc/readme.txt:112-115`).
- Hard-coded per-version MSI GUIDs for 2.9.0–3.0.0 (`CMakeLists.txt:625-645`).
- `make_symlinks.sh` post-install script, `createPathsFile.js`.

All of that can be deleted and replaced by `sst-cmake`'s `basic_installer`
(`include(basic_installer)` in OB-Xf's CMakeLists, one line).

### 7.2 But there IS real coupling — three things

**1. The `.paths` file — a hard runtime dependency on the installer.**
`installer/PluginsFolder/SpectrumWorx.paths.in` is `configure_file`d at build
time (`CMakeLists.txt:691-698`) into a two-line text file (root dir, presets
dir) that the installer drops next to the binary. At runtime,
`GUI::initializePaths()` (`gui.cpp:360-425`) memory-maps that file to find
everything. `BOOST_VERIFY(initializePaths())` is called from
`spectrumWorxSharedImpl.inl` — **if the installer didn't run, the plugin
asserts and dies.**

**2. GUI resources live in the installer tree.**
`installer/ProgramFolder/Resources/*.png` (844 KB, 67 files, named `01.png` …
`68.png` and referenced by the `ResourceBitmaps` enum in `gui.hpp:68-136`) are
the entire skin. They are installed to a shared directory and loaded from disk
at runtime. Also there: factory Presets (1.2 MB, 15 banks), Samples (1.4 MB),
EULA, ReadMe.rtf, the User's Guide PDF.

**3. Bundle metadata is generated backwards.**
`source/CMakeLists.txt:328-337` `configure_file`s
`installer/resources/OSX/SpectrumWorx.{vst,component}/Contents/Info.plist.in`
→ `.plist`, i.e. the build writes into the installer tree. Same for the AU
`resources.r`.

### 7.3 Fix

- `juce_add_binary_data(BinaryData SOURCES ...)` for the skin PNGs (exactly what
  OB-Xf does in `assets/CMakeLists.txt`) → `initializePaths()`, the `.paths`
  file, `boost::mmap` and `resourcesPath()` all disappear.
- Factory presets and samples: ship via the installer to a user data dir
  discovered by `sst-plugininfra`'s path helpers, not by a `.paths` file.
- `juce_add_plugin` generates the bundles and plists.
- Docs move to `docs/`; convert the `.doc` files to Markdown while you're at it.

**Estimate: 1–1.5 weeks**, most of it the BinaryData migration and preset-path
rework. The installer *itself* is a day.

---

## 8. Everything else worth knowing

### 8.1 :rotating_light: Legal / licensing

1. ~~**The VST 2.4 SDK is checked in.**~~ **Done (2026-07-27).**
   `le/plugins/vst/2.4/aeffect.h` and `aeffectx.h` were
   "© 2006, Steinberg Media Technologies, All Rights Reserved". Steinberg
   terminated VST2 licensing in 2018, so they could not stay in a GPL-3.0 repo;
   both have been `git rm`'d.
   **TODO — two follow-ups remain:**

   - [ ] **Delete the LE-authored VST2 wrapper.**
     `le/plugins/vst/2.4/plugin.{cpp,hpp,inl}` (2,256 lines) still
     `#include "aeffectx.h"` at `plugin.hpp:70` and no longer compiles. Harmless
     for now (nothing builds), and it is slated for deletion under §8.2 either
     way — but it should go in the same pass rather than linger as a dangling
     reference.
   - [ ] **Purge the SDK from git history with `git filter-repo`.** `git rm`
     only removes files from HEAD; `aeffect.h` and `aeffectx.h` **remain in
     history** at commit `d7b7602`, and remain fetchable by anyone who clones.
     Removing them from HEAD is not sufficient if clean provenance matters for
     redistribution.

     Do this **together with the RSA private key** (§3.3) — one rewrite, not
     two. Paths to purge:

     ```
     source/externals/le/plugins/vst/2.4/aeffect.h
     source/externals/le/plugins/vst/2.4/aeffectx.h
     source/externals/le/license_key/le_key_01.pem
     source/externals/le/license_key/le_key_01_private.cpp
     source/externals/le/license_key/le_key_01_private.hpp
     source/externals/le/license_key/le_key_01.der
     ```

     ```sh
     git filter-repo --invert-paths \
       --path source/externals/le/plugins/vst/2.4/aeffect.h \
       --path source/externals/le/plugins/vst/2.4/aeffectx.h \
       --path-glob 'source/externals/le/license_key/*'
     ```

     **Timing matters and the window is closing.** A history rewrite changes
     every commit hash, so it must happen *before* the repo picks up forks,
     clones, open PRs or long-lived branches. The tree is currently 4 commits
     with no forks — this is the cheapest it will ever be. It also requires a
     force-push and a heads-up to anyone holding a clone.

     Note that this only fixes *this* repository. Anything already mirrored
     elsewhere (GitHub forks, the GitHub API's dangling-object cache) is outside
     your control, which is a further argument for doing it early. If the key
     was ever reused on other Little Endian products, they should be told
     regardless of what happens to this history.
2. **Header copyrights contradict the LICENSE.** Every file says "All rights
   reserved"; the repo is GPL-3.0. Needs a blanket header rewrite (a
   `scripts/fix_file_comments.pl` equivalent — OB-Xf has exactly that).
3. **JUCE 8 licence.** JUCE 8 is AGPLv3-or-commercial. AGPLv3 and GPLv3 are
   compatible in the direction you need (you can combine GPLv3 code with AGPLv3
   and distribute the result under AGPLv3), but the project should decide
   explicitly whether to relicense to AGPLv3, take a JUCE commercial licence, or
   stay GPLv3 with the JUCE exception. Surge Synth Team projects generally go
   GPL-3.0-or-later; worth matching whatever OB-Xf does.
4. **Committed RSA private key** — see §3.3.
5. **Third-party attribution.** NT2, RapidXML, LibTom*, RtAudio all need
   `THIRD_PARTY.md` entries if retained (most won't be).

### 8.2 Dead code to delete outright

| Path | Files | Lines | Why |
|---|---:|---:|---|
| `source/externals/nt2` | 10,134 | — | replaced by sst-basic-blocks/simde |
| `source/externals/le/audioio` | 25 | 20,661 | not in the plugin build; includes a patched RtAudio |
| `source/externals/le/plugins/{fmod,unity}` | 9 | 2,080 | not built (`LE_SW_FMOD` off; `source/fmod_lib/` doesn't even exist) |
| `source/externals/le/plugins/{vst,au}` | 13 | ~7,700 | replaced by JUCE |
| `source/externals/le/licenser`, `license_key` | 18 | ~2,400 | §3 |
| `source/externals/boost/{hash,mmap,filesystem}` | ~55 | — | §3, §7 |
| `3rd_party/LibTom{Crypt,Math}` | — | 4 MB | unused |
| `3rd_party/JUCE` | 1,705 | 29 MB | §1 |
| `effects/_unfinished` | 33 | — | 17 half-built effects; **read before deleting**, several look interesting (morpheus, phase_locked_vocoder, transients_extractor) |
| `installer/resources/{Windows,OSX}` WiX/PackageMaker | — | — | §7 |
| `le/build/*.cmake`, `android.toolchain.cmake`, `iOS.toolchain.cmake` | 7 | — | §4 |

That's roughly **80 % of the file count and 90 % of the bytes** gone.

### 8.3 Architecture notes for the port

- **Parameters are dynamic — this is the single most important architectural
  fact in the codebase.** `source/configuration/constants.hpp` fixes the *slot
  count* (5 modules × 10 parameters = 50, plus globals and LFOs, 4 programs),
  but a slot's name, range, unit and meaning all change when a different effect
  is loaded into that module. The framework models this explicitly:
  `useDynamicParameterLists()` (`le/plugins/vst/2.4/plugin.hpp:968`),
  `dynamicParameterAccessContext()` (`core/spectrumWorxCore.hpp:163`),
  `parameterListChanged()` (`plugin2Host.hpp:142`), and a `Program const*`
  context passed into every `getParameterName`/`getParameterLabel`/
  `getParameterDisplay`.
  `SW::ParameterID` (`core/parameterID.hpp:27-64`) is a packed `uint32`
  `{type, moduleIndex, moduleParameterIndex, lfoParameterIndex}` — a stable
  host-facing ID, deliberately distinct from a parameter *index*.
  This drives the plugin-format decision in §1.6: CLAP handles dynamic
  parameters natively, VST3 patchily, and JUCE's `AudioProcessorParameter`
  worst of the three. Read `parameterID.hpp`,
  `core/host_interop/parameters.hpp` and `plugin2Host.hpp` before anything else.
- **`LE_SW_SEPARATED_DSP_GUI`** is a build option for running DSP and GUI as
  separate instances. It appears unfinished (`gui.hpp:547` has
  `LE_UNREACHABLE_CODE()` on that path). Drop the option.
- **Preset format** is XML (RapidXML) with a bank/preset directory layout
  (`spectrumworx/presets.cpp`, `gui/preset_browser/`). 15 factory banks ship.
  Preserving load compatibility for existing user presets is a decision to make
  early — it constrains how much you can change the parameter system in §5.3a.
- **Side-chain**: `usesSideChannel` per effect, plus file-based "external audio"
  (`external_audio/sample.cpp`) for effects like Convolver/Vocoder. In a modern
  plugin the side-chain should come from a JUCE side-chain bus; the file-loading
  path is a separate feature.
- **Threading**: `authorizationThread_`, `BackgroundThread`, `GUI::Lock` over
  `MessageManagerLock`, `postOrExecuteMessage`. Worth auditing for
  audio-thread-safety once it builds — there's no obvious lock-free parameter
  queue between GUI and audio.
- **Encoding**: many source files are CP-1252 (the `©` renders as `�`). Convert
  to UTF-8 in the same pass as the licence-header rewrite.
- **Include guards**: the codebase uses `#ifndef` guards *and* `#pragma once`
  together. Matches your stated preference for `#ifndef` — just strip the
  `#pragma once` lines.

### 8.4 No tests

There is no test target anywhere. Given that this is 23 k lines of DSP with no
reference behaviour captured, **golden-value tests should be added before the
Boost/NT2 vector-math replacement**, not after — otherwise there's no way to
know whether swapping the FFT or the SIMD backend changed the sound.

---

## 9. Suggested phasing

| Phase | Content | Est. |
|---|---|---|
| **0. Triage** | Delete nt2, audioio, LibTom\*, VST2/AU/fmod/unity wrappers, licenser, `_unfinished`, VST2 SDK headers, private key. Rewrite file headers to GPL-3.0, convert to UTF-8. **Then the `git filter-repo` history purge (§8.1) — do it here, while the repo is still 4 commits with no forks.** Nothing needs to compile yet. | 1 wk |
| **1. Skeleton** | Top-level CMake modelled on OB-Xf. `libs/` submodules (JUCE 8, clap-juce-extensions, sst-*, simde, Boost as a stopgap). `src/` layout. CI workflows. `juce_add_plugin` producing an empty VST3/AU/CLAP that loads. | 1.5–2.5 wk |
| **2. DSP first** | Get `le/spectrumworx/engine` + `effects` + `le/math` compiling standalone against C++20 + Accelerate/juce::dsp/sst-basic-blocks, behind a Catch2 target. **Write golden-value tests here.** Keeps Boost.Fusion for now. | 3–5 wk |
| **2b. Format spike** | One week, in parallel: write a native CLAP backend against the existing `Plugin2HostInteropControler` interface (§1.6.5) to decide the host-layer route. Not wasted either way. | 1 wk |
| **3. Host layer** | *Either* rewrite `spectrumWorx.cpp` as a JUCE `AudioProcessor` (delete `le/plugins` **and** `core/host_interop`), *or* finish the native CLAP backend and add clap-wrapper for VST3/AUv2/standalone (keep `core/host_interop`). See §1.6. First audible sound in a DAW. | 3–4 wk / 2.5–4 wk |
| **4. GUI** | Port `source/gui` to JUCE 8; embed the skin as BinaryData; collapse the owned-window model into child components; delete `gui.mm`, `.paths`, `boost::mmap`. | 4–6 wk |
| **5. De-Boost** | Tier 1 mechanical swaps, then the Fusion/MPL parameter system. Golden tests from phase 2 protect you. | 5–8 wk |
| **6. Ship** | `basic_installer`, notarisation, Linux, arm64, docs. | 1–2 wk |
| | **Total** | **~20–30 engineer-weeks** (+1–2 wk if the CLAP route's thread-discipline work is taken on, §1.6.3) |

Phases 2 and 4 can run in parallel with different people. Phase 5 can be
deferred indefinitely — a Boost-dependent build is not a blocker for shipping.

### Highest-risk items

1. **Plugin-format decision (§1.6).** JUCE `AudioProcessor` vs native CLAP +
   clap-wrapper. It is hard to reverse once the host layer is written, and the
   dynamic parameter model (§8.3) argues against the JUCE route. Resolve it with
   the phase-2b spike, not by assumption.
2. **Boost.Fusion parameter system (§5.3a).** It's the spine of the codebase and
   it drives GUI generation and preset serialisation. Do not touch it before
   golden tests exist.
3. **Owned-window GUI model (§6.2).** Not a port; a redesign. Scope it as one.
4. **NT2 removal changing DSP output (§5.3b).** Only detectable with tests.
5. **Preset backward compatibility.** Decide early; it constrains 2 and 4.

### Time-sensitive

The **`git filter-repo` purge (§8.1)** is the only item on this list that gets
*harder* the longer it waits. It rewrites every commit hash, so it has to land
before the repo accumulates forks, clones or open PRs. Right now that cost is
approximately zero.

### Lowest-hanging fruit

The licence manager (§3, ~4 days), the dead-code deletion (§8.2, ~1 week), and
the top-level CMake (§4) are all independent and can start immediately.
