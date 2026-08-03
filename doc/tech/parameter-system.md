# SpectrumWorx — The Parameter System

How SpectrumWorx addresses, names and exports parameters, and why the answer is
"dynamic" in a way that constrains the plugin-format choice.

Written against commit `6e09d15` (post-restructure: `source/` → `src/`,
`src/externals/le/` → `src/le/`). Companion to
[`initial_scan.md`](initial_scan.md) §1.6.2 and §8.3, which state the conclusion;
this document shows the mechanism.

> **Still accurate as of 03.08.2026**, and the only document about this layer.
> The port did not change the parameter system: the skeleton, the addressing and
> the runtime re-meaning are what they were. Two things it does not know about,
> both additive — the base value now travels separately from the LFO's output
> (`correct_the_threading.md` §4), and `tests/parameters/parameterTable.txt`
> pins the whole 286-row enumeration. The one open item against this layer is
> `week_two.md` §3's 227 `-Wundefined-var-template`, which is a missing
> declaration in `Parameters::Name<>` rather than a flaw in what is described
> here.

---

## 1. The short version

A natural first assumption is that the parameter list is static per plugin
binary, and that the apparent variety comes from the tree containing several
plugins of different shapes. That is **not** what this codebase does.

There is **one plugin** with a **fixed parameter skeleton** whose **slots change
meaning at runtime**. Loading Convolver into module slot 3 instead of Armonizer
gives the very same host-facing parameter a different name, unit, range and
meaning — in one instance, with no reload — and the effect swap is *itself* an
automatable parameter, so a DAW can automate the entire parameter list's meaning
out from under itself.

Two independent axes are easy to conflate:

| Axis | When | What it changes |
|---|---|---|
| **Edition** (`LE_SW_INCLUDED_EFFECTS`) | build time | *which effects may be loaded* into a slot |
| **Slot occupancy** (`ModuleChainParameter`) | run time | *what every module/LFO parameter means* |

Only the second one makes the parameter list dynamic. §6 covers the first, which
is real but does not do what it might appear to.

---

## 2. The fixed skeleton

`src/configuration/constants.hpp:28-30`:

```cpp
std::uint8_t const maxNumberOfModules            (  5 );
std::uint8_t const maxNumberOfParametersPerModule( 10 );
std::uint8_t const maxNumberOfModuleParameters   ( maxNumberOfModules * maxNumberOfParametersPerModule );
```

`src/core/host_interop/parameters.hpp:38-47` derives the rest:

```cpp
static std::uint8_t  const lfoParametersPerModule = lfoExportedParameters * ( maxNumberOfParametersPerModule - 1 );
static std::uint16_t const lfoParameters          = lfoParametersPerModule * maxNumberOfModules;

static std::uint16_t const maxNumberOfParameters  = GlobalParameters::Parameters::static_size
                                                  + Constants::maxNumberOfModules
                                                  + Constants::maxNumberOfModuleParameters
                                                  + lfoParameters;
```

In the default build configuration that is:

| Block | Count | Notes |
|---|---:|---|
| Globals | 7 | InputGain, OutputGain, MixPercentage, FFTSize, OverlapFactor, WindowFunction, InputMode |
| Module chain ("which effect is in slot N") | 5 | one per slot |
| Module parameters | 50 | 5 slots × 10 |
| LFO parameters | 225 | 5 exported LFO params × 9 non-Bypass module params × 5 slots |
| **Total** | **287** | |

Caveats on the numbers, all build-flag dependent:

- Globals are 7 because `LE_SW_ENGINE_INPUT_MODE` defaults to `full` (=2) and
  `LE_SW_ENGINE_WINDOW_PRESUM` defaults to `false`
  (`src/le/spectrumworx/engine/configuration.cmake:17-29`,
  `src/le/spectrumworx/engine/parameters.hpp:29-70`).
- `lfoExportedParameters` is **5** in a normal GUI build and **7** under
  `LE_SW_SEPARATED_DSP_GUI || !LE_SW_GUI` — SyncTypes and Waveform are not
  exported for automation in the GUI build
  (`src/core/host_interop/parameters.hpp:27-36`).
- The AU path excludes `InputMode` (AU has no plugin-initiated I/O change), so
  its full static list is 286, not 287
  (`src/core/host_interop/plugin2Host.cpp:287-294`).

This skeleton never changes at runtime. Everything below is about what the slots
*mean*.

---

## 3. Addressing: `SW::ParameterID`

`src/core/parameterID.hpp:27-64` — a packed `uint32` union:

```cpp
struct Global      { Padding      padding0         ; Padding      padding1            ; std::uint8_t index      ; };
struct ModuleChain { Padding      padding0         ; Padding      padding1            ; std::uint8_t moduleIndex; };
struct Module      { Padding      padding0         ; std::uint8_t moduleParameterIndex; std::uint8_t moduleIndex; };
struct LFO         { std::uint8_t lfoParameterIndex; std::uint8_t moduleParameterIndex; std::uint8_t moduleIndex; };
```

plus a `Type` discriminator (`GlobalParameter`, `ModuleChainParameter`,
`ModuleParameter`, `LFOParameter`).

The critical design property: an ID means **"slot 3's 4th parameter"**, never
**"Convolver's Wet"**. The ID stays valid across effect swaps; only the metadata
behind it changes. That is deliberate — the framework distinguishes a parameter
*ID* from a parameter *index*, and abstracts which one a host protocol uses as a
trait (`ParameterSelector`, see §5).

Dispatch on the discriminator is centralised in
`invokeFunctorOnIdentifiedParameter` (`parameterID.hpp:83-96`), which every
name/label/value getter goes through.

---

## 4. The dynamic part

### 4.1 The effect selector is a parameter

`src/core/automatedModuleChain.hpp:41-48`:

```cpp
LE_DEFINE_PARAMETER
(
    ( ModuleChainParameter                             )
    ( Parameters::LinearSignedInteger                  )
    ( Minimum<noModule>                                )   // -1 == empty slot
    ( Maximum<Effects::Constants::numberOfEffects - 1> )
    ( Default<noModule>                                )
);
```

The host sees five parameters named `"Module 1"` … `"Module 5"`
(`src/core/host_interop/plugin2Host.cpp:566-575`) whose value is an effect index.
They are ordinary automatable parameters.

### 4.2 What a write to it does

`AutomatedModuleChain::setParameter`
(`src/core/automatedModuleChain.hpp:103-144`) destroys whatever module occupies
the slot and constructs a new one:

```cpp
auto const pNewModule( ModuleFactory::create<Module>( effectIndex ) );
if ( pNewModule && initialise( *pNewModule, moduleIndex ) )
{
    insertAtAndReplace( pCurrentModuleNode, Engine::node( *pNewModule ) );
    return std::make_pair( pNewModule, effectIndex );
}
```

`ModuleFactory::create` (`src/core/modules/factory.cpp:152-199`) turns the
runtime `int8_t` into a concrete compile-time effect type via a
`boost::switch_<Effects::ValidIndices>` over the compiled-in index range, then
placement-news it into a `std::malloc`'d block sized by the same dispatch.

So the parameter *count* per slot, the parameter *names*, the *units*, the
*ranges* and the value→display mapping all change at that moment.

### 4.3 Value scaling is dynamic too

It is not only names. `internal2AutomatedValue` / `automated2InternalValue`
(`src/core/modules/automatedModule.hpp:63-64`) take a
`ModuleParameters const &` — the normalisation for a given slot index depends on
the effect currently loaded there. A host holding a normalised 0..1 automation
value gets a different real value after a swap.

---

## 5. The mechanism: a nullable `Program const *` context

Every metadata query is written twice over, selected by whether a
`Program const *` context is supplied:

| Context | Behaviour |
|---|---|
| **non-null** | walk to the live module, report the actual effect's metadata |
| **null** | report the static fallback for the slot |

`ParameterNameGetter::operator()( ParameterID::Module, Program const * )`
(`plugin2Host.cpp:578-613`) is the clearest example:

- out-of-range for the loaded effect → `"N/A"` (`:581-585`)
- no context, beyond the shared base parameters → `"M3.P4"` (`:597-602`)
- otherwise → the live name,
  `pModule->effectSpecificParameterInfo( ... ).name` (`:605-611`)

The same fork appears in:

- `getParameterIDs` (`:271-358`) — real per-module counts
  (`Engine::actualModule<Module>( *pModule++ ).numberOfParameters()`, `:333`)
  versus `maxNumberOfParametersPerModule`
- `numberOfParameters` (`:361-384`) — sums the actual loaded modules when given a
  program, otherwise reports the maximum. **For AU the exported list length
  literally grows and shrinks.**
- `getParameterLabel` (`:238-247`) → units per live effect
- the LFO name getter (`:616-652`), which composes onto the module parameter name

The context itself is trivially `the current program`
(`src/core/spectrumWorxCore.hpp:163`):

```cpp
Program const & dynamicParameterAccessContext() const { return program(); }
```

and the *decision* to pass it is made per dispatch, at
`src/le/plugins/vst/2.4/plugin.inl:261`:

```cpp
auto const * LE_RESTRICT const pContextForDynamicParameterAccess
    ( impl.useDynamicParameterLists() ? &impl.dynamicParameterAccessContext() : nullptr );
```

then threaded into `effGetParamLabel` / `effGetParamName` /
`canParameterBeAutomated` (`:285-289`, `:449-456`).

---

## 6. Host capability probing, and the fallback

Neither VST 2.4 nor AU could be trusted to honour a changing parameter list, so
the framework **probes the host at runtime** and degrades.

**VST 2.4** (`src/le/plugins/vst/2.4/plugin.hpp:967-971`):

```cpp
bool LE_FASTCALL useDynamicParameterLists() const
{
    return host().knownToSupportDynamicParameterLists() || VSTPluginBase::allParametersInspected_;
}
```

The second disjunct is the probe. `parameterListChanged()` (`plugin.hpp:757-763`)
clears a bitset, calls `updateDisplay()`, and returns whether the host responded
by re-reading *every* parameter name — each `effGetParamName` sets its bit via
`inspectedParameter()` (`plugin.inl:288`, `plugin.hpp:973-981`). If the host
re-read all of them, it is treated as honouring dynamic lists.

**AU** (`src/le/plugins/au/plugin.cpp:101-112`) checks whether the host
acknowledged `kAudioUnitProperty_ParameterList`, falling back to
`kAudioUnitProperty_ParameterInfo`.

**When the probe fails**, `moduleChangedByUser` and `modulesChanged`
(`plugin2Host.cpp:88-89`, `:183-184`) fall through to `moduleChanged`
(`src/core/host_interop/plugin2HostImpl.inl:361-368`, whose first line asserts
`!parameterListChanged()` — "Should not get here if host supports parameter list
changes"). That path manually pushes all 50 module parameter values plus every
LFO parameter value at the host so the automation lanes at least hold correct
numbers. Names stay generic. `presetChangeEnd` does the same after a preset load
(`plugin.hpp:764-772`).

> Both backends are slated for deletion under the CLAP-first plan. The behaviour
> is documented here because it is the accumulated evidence about *what hosts
> actually do*, and because it is the thing the CLAP port gets to delete.

---

## 7. Presets are name-keyed, not index-keyed

Relevant because it decouples the preset format from the index space.

`src/le/spectrumworx/presets.cpp:420-422` resolves a saved module by **effect
name**:

```cpp
auto const effectName ( currentEffectName()                );
auto const effectIndex( Effects::effectIndex( effectName ) );
bool const foundEffect( effectIndex != noModule            );
```

and individual parameters are XML attributes keyed by mangled parameter name
(`:628`, `parameters().attribute( mangleSpaces( parameterName ) )`).

Consequences: the effect *index* space can be reordered or extended without
breaking presets, and a preset referencing an effect absent from the current
edition degrades rather than corrupting the chain. The `.swp` format is the
constraint on the parameter-system refactor, not the index numbering.

---

## 8. The edition axis (the build-time one)

`src/core/configuration.cmake:63` → `addSelectedEffects( "${LE_SW_INCLUDED_EFFECTS}" )`,
implemented at `src/le/spectrumworx/effects/configuration/effectsList.cmake:101-258`.
It selects which of the ~57 effects get compiled in — full edition
(`-DLE_SW_FULL`, `:230-231`), cut-down editions, SDK builds.

Two things stop this from making the parameter list static per binary:

1. **Indices are global and stable across editions by design**
   (`effectsList.cmake:113-123`): metadata is emitted for *all* effects, not just
   included ones, explicitly "to enable (host project) compatibility between
   different editions". A non-included effect still occupies its index;
   `Effects::includedEffects[]` is a `bool` array over the complete set
   (`includedEffects.hpp.in:39`) and the factory refuses with a warning box
   rather than shifting anything (`factory.cpp:161-171`).
2. **It changes the candidate set, not the skeleton.** Even a hypothetical
   single-effect edition still exposes 5 slots × 10 parameters.

Related: the LE framework served more than one product from this tree — see the
`TuneWorx` special-cases (`plugin2Host.cpp:114-119`,
`src/le/spectrumworx/effects/tune_worx/`, `-DLE_SIMPLE_TUNEWORX` at
`src/core/configuration.cmake:59`). That is the sense in which "several plugins
of different shapes" is true of the *framework*. This repo builds one plugin.

---

## 9. Why this drives the format decision

Restating [`initial_scan.md`](initial_scan.md) §1.6.2 with the mechanism attached:

| Protocol | Fit |
|---|---|
| **CLAP** | native. `clap_param_info.id` takes `SW::ParameterID`'s packed `uint32` with zero translation; `clap_host_params::rescan(INFO\|TEXT\|VALUES)` *is* `parameterListChanged()`; §6's probing and the whole manual-push fallback delete outright |
| **VST3** | `restartComponent( kParamTitlesChanged )`; honoured unevenly, so some form of §6 survives |
| **JUCE `AudioProcessorParameter`** | worst fit — parameters are objects constructed once with fixed `getName()`/`getLabel()`/ranges. You either expose 287 generic "Mod 3 Param 7" slots or subclass to return changing names and lean on `updateHostDisplay( ChangeDetails().withParameterInfoChanged( true ) )` |

Concrete mapping for the CLAP backend:

| Today | CLAP |
|---|---|
| `SW::ParameterID::binaryValue` | `clap_param_info.id` (verbatim) |
| `getParameterName( id, &program )` | `clap_plugin_params::get_info` → `name` |
| `getParameterLabel( id, &program )` | `clap_plugin_params::get_info` → `module`/units |
| `numberOfParameters( &program )` | `clap_plugin_params::count` |
| `parameterListChanged()` | `clap_host_params::rescan( INFO \| TEXT )` |
| `moduleChanged()` manual value push | **deleted** — `rescan( VALUES )` |
| `useDynamicParameterLists()` probe | **deleted** — the extension is contractual |

The one thing CLAP does *not* hand you: `clap_plugin_params::count` and the ID
list are `[main-thread]`, and today the module chain is mutated from the GUI
thread with a `GUI::Lock` and read on the audio thread with no visible lock-free
queue. Effect swaps become a main-thread/audio-thread handoff, not just a
notification. That is the "thread discipline: +1–2 wk" line in §1.6.4.

---

## 10. Reading order

For anyone touching this:

1. `src/core/parameterID.hpp` — the addressing scheme
2. `src/configuration/constants.hpp` + `src/core/host_interop/parameters.hpp` — the skeleton
3. `src/core/automatedModuleChain.hpp` — the slot model and the selector parameter
4. `src/core/host_interop/plugin2Host.cpp` — every dual-mode getter
5. `src/core/host_interop/plugin2Host.hpp` — the `Plugin2HostInteropControler` interface the CLAP backend implements
6. `src/core/modules/factory.cpp` — runtime index → compile-time type dispatch

### Open questions for the port

- **Does the slot count stay at 5?** Nothing in the host-facing design requires
  it; `maxNumberOfModules` is a single constant. But the GUI skin is fixed-size
  bitmaps laid out for 5, and `ParameterID` packs indices into bytes, so the ID
  encoding is good to 255.
- **`lfoExportedParameters` 5 vs 7.** The GUI build hides SyncTypes and Waveform
  from automation. Under CLAP there is no reason to keep them hidden; exporting
  them changes the parameter count and therefore any host-side automation
  bindings.
- **Preset compatibility.** Decided before the Boost.Fusion refactor, per
  §8.3 — §7 above says the format is name-keyed, which gives more freedom than
  the index-based reading might suggest.
