# SpectrumWorx — The Parameter System

How SpectrumWorx addresses, names and exports parameters, and why the answer is
"dynamic" in a way that constrains the plugin-format choice.

Written against commit `6e09d15` (post-restructure: `source/` → `src/`,
`src/externals/le/` → `src/le/`), and companion to
[`streaming_format.md`](streaming_format.md), which is what happens to a
parameter once it reaches a file.

> **Still accurate as of 07.08.2026**, and the only document about this layer.
> The port did not change the parameter system: the skeleton, the addressing and
> the runtime re-meaning are what they were. Two things it does not know about,
> both additive — the base value now travels separately from the LFO's output
> (`threading_model.md` §4), and `tests/parameters/data/parameterTable.txt` pins
> the whole 286-row enumeration.
>
> Two things did change and have sections of their own. **§7** is *where* a
> parameter's name lives (04.08.2026): beside the parameter it names, in the
> header, not in a `.cpp`. **§8** is a parameter's display text, which now goes
> both ways (07.08.2026).

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
| Globals | 6 | InputGain, OutputGain, MixPercentage, FFTSize, OverlapFactor, WindowFunction |
| Module chain ("which effect is in slot N") | 5 | one per slot |
| Module parameters | 50 | 5 slots × 10 |
| LFO parameters | 225 | 5 exported LFO params × 9 non-Bypass module params × 5 slots |
| **Total** | **286** | |

Caveats on the numbers:

- Globals were 7 until 04.08.2026, the seventh being `InputMode` — a parameter
  that reconfigured the plugin's own I/O (2 in / 2 out, mono, with or without a
  side chain), behind `LE_SW_ENGINE_INPUT_MODE`. It never existed in a build this
  port produced, because nothing defined the macro, and it is gone: CLAP
  configures I/O through `audio-ports-config` and the plugin has declared a real
  side-chain port since it was written. The AU path used to exclude it separately
  (AU has no plugin-initiated I/O change), so every format's list is now the same
  one.
- A seventh could come back: window presum would add a `WindowSizeFactor`, which
  moves this table and every automation lane a host has saved against it. That is
  why it is a decision rather than a switch, and why the option that used to hold
  the place open was deleted rather than left off — see
  [`future_items_to_revive.md`](future_items_to_revive.md).
- `lfoExportedParameters` is **5**, unconditionally
  (`src/core/host_interop/parameters.hpp:31`). SyncTypes and Waveform are not
  exported for automation. It used to be 7 in a build without the GUI, behind
  two edition macros that are gone; the editor reaches those two parameters by
  another route (`spectrumWorxEditor.hpp:811-829`).

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
`ModuleParameter`, `LFOParameter`), which occupies the high byte and **starts at
one, not zero**. That is deliberate and it is worth not undoing: a zero-based
discriminator made the first global parameter's ID `0x00000000`, which is a
legal `clap_id` — only `CLAP_INVALID_ID` is reserved — but indistinguishable
from an uninitialised value in a log, a debugger, or a host's saved session.
Starting at one makes zero impossible for *any* parameter, so the default
constructor's `binaryValue{0}` now means "no parameter" and cannot be mistaken
for `In`.

It moved on 07.08.2026, which renumbered every ID by `+0x01000000`: `In` is
`0x01000000` and slot 1's selector is `0x02000000`. An ID is what a host writes
into a saved session to name an automation lane, so that was a break, taken
deliberately before the first release because it cannot be taken after one.
`tests/parameters/data/parameterTable.txt` pins all 388 of them.

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

### Where a name lives, and why it is the header

A parameter's name is a specialisation of `Parameters::Name<>`, and since
04.08.2026 it is written **in the header that declares the parameter**, directly
under it, with `EFFECT_PARAMETER_NAME` (or `UI_NAME` outside `SW::Effects`).
So does its streaming name, and so do an enumerated parameter's value strings:

```cpp
struct AhAh
{
    LE_DEFINE_PARAMETER( Center, LinearUnsignedInteger, … );
    …
};

EFFECT_PARAMETER_NAME( AhAh::Center, "Center frequency" )
EFFECT_PARAMETER_STREAMING_NAME( AhAh::Center, "Center (LFO me!)" )
```

They used to be `.cpp` definitions of a declared-but-undefined static member,
resolved by the linker. The rule that replaced it is not a style preference —
**a translation unit that instantiates `name<Parameter>()` has to have seen the
specialisation**, and one that has not is ill-formed with no diagnostic required.
Clang issues one anyway, and there were 283 of them: every place the parameter
table is built (`Detail::info<>()`, `moduleImpl.hpp`) against every parameter,
in every translation unit that reaches one.

Two things follow, and both are what §7 above is about:

- **What used to be a link error is now a compile error.** The primary template
  is still a declaration with no definition, so a parameter nobody has named
  fails at the point of use rather than at link time — and it fails in the
  translation unit that builds the table, which is `factory.cpp`. The warning
  is what catches it; the baseline in `src/CMakeLists.txt` makes it fatal.
- **A specialisation written where the users cannot see it is silent, not loud.**
  A missing *name* is at least an undefined symbol. A missing
  `DisplayValueTransformer` or `StreamingName` is not: the translation unit gets
  the primary template, and the parameter quietly streams under the wrong key or
  prints without its unit. That is the failure `EFFECT_PARAMETER_STREAMING_NAME`
  was introduced to prevent and `tests/parameters/streamingNameTests.cpp` pins.

---

## 8. Display text, both ways

A parameter's value becomes text through `Parameters::print` and text becomes a
value again through `Parameters::parse` (`src/le/parameters/{printer,parser}.hpp`).
The two are mirror images: one file per parameter tag, dispatched on
`Parameter::Tag`, and the units they meet in are supplied by the same
`DisplayValueTransformer<Parameter>` — `transform` outbound, `inverse` back. Five
parameters have a transform: the two gains (dB), the mix and the overlap factor
(%), the two effect frequencies (Hz, and the only one that needs the engine's
`Setup`), and the ExImPloder gate (dB, with `-inf` for "off").

Three properties are worth stating, because each is a bug that was actually
shipped:

- **`parse` answers `std::optional`.** A display transform is not onto: `""`,
  `"off"` and `"M3.Wet"` are text no value corresponds to, and `strtod` answers
  `0` for all three — a value every one of these parameters can hold. The first
  `text_to_value` here did exactly that and, worse, read the display units as
  storage units: clap-validator caught the input gain going `0.001` → `"-60dB"`
  → `-60.0` → `"nandB"`.
- **`parse` answers a value the parameter could hold** — clamped to its range,
  whole where it is integral, snapped where it must be a power of two. Typing
  "999 dB" is not an error; storing 999 in a ±20 dB parameter is, and
  `Parameter::setValue` answers that with an assertion in a checked build and by
  storing it in a release one.
- **An unseen `inverse` is silent**, exactly as §7's second bullet says about an
  unseen `transform`: the primary template is the identity, so a translation unit
  that cannot see the specialisation parses dB as a linear gain and says nothing.
  The specialisations live in the header beside the parameter for that reason.

Power-of-two parameters print *through* the transformer like everything else as
of 07.08.2026. They used to be the one exception — the note said they "do not
currently support/use the DisplayValueTransformer functionality" and the overlap
factor's percentage came from an explicit `print<>` specialisation instead. That
specialisation named two overloads that did not exist, so from 2011 to 08.2026
the overlap factor displayed as the raw factor with a `%` after it: "4%" for a
75% overlap, in the host's generic panel and in the settings window's own combo
box, whose comment says it exists to show the percentage.

### What the host sees

`SpectrumWorxCLAP::params{ValueToText,TextToValue}` are the edge, and two things
about them are policy rather than mechanism:

- `value_to_text` renders the parameter's **own** value and ignores the one it is
  handed. The `\todo` above it has the argument; the short version is that
  rendering a supplied value means default-constructing a parameter, and a
  dynamic range finds its bounds by walking to the LFO that owns it. `parse` has
  no such problem and takes no parameter object at all.
- A parameter no effect currently owns displays as **`notAvailable`** — the same
  `"N/A"` `paramsInfo` names it — and reads back as the default `paramsValue`
  answers with. Not cosmetic: clap-validator's `param-conversions` requires
  `text_to_value` for *all* the automatable parameters or for none, and every ID
  in the fixed list is automatable so that a host's automation lane survives an
  effect swap. An empty string, which is what these used to display as, is not
  something a parser can honestly read back.

`tests/clap/parameterTextTests.cpp` holds every parameter of every one of the 57
effects to the round trip, and does it by re-printing rather than by comparing
values: print, parse, write the parsed value back, print again, and require the
two strings to be equal. That is what "within the display's own precision" means,
and it needs no per-parameter idea of how many decimal places that is.

---

## 9. The edition axis (the build-time one)

There is no longer a build-time edition axis: every one of the 57 effects is
compiled in, and the cmake that used to select a subset was deleted on
07.08.2026 along with the four other orphan build files. What survives of it is
`Effects::includedEffects`, a constexpr array over the complete effect set whose
every entry is `true`
(`src/le/spectrumworx/effects/configuration/includedEffects.hpp:30`).

The axis is worth recording anyway, because the *shape* it forced is still the
shape the parameter system has, and two properties of it outlive the switch:

1. **Indices are global, and stable across editions by design.** Metadata was
   emitted for all effects rather than only included ones, to keep a host's saved
   automation valid across editions. A non-included effect still occupies its
   index: `includedEffects` is consulted, and the factory refuses with a warning
   rather than shifting anything (`factory.cpp:161-171`), as do the preset
   loader (`presets.cpp:580`) and the module menu
   (`moduleMenuHolder.cpp:37`).
2. **It changed the candidate set, not the skeleton.** Even a single-effect
   edition still exposed 5 slots × 10 parameters.

Related: the LE framework served more than one product from this tree — see the
`TuneWorx` special-cases (`plugin2Host.cpp:114-119`,
`src/le/spectrumworx/effects/tune_worx/`). Tune Worx also had a cut-down
13-parameter edition, selected by a switch this tree never set; it was deleted on
07.08.2026 and the full 34-parameter version is the only one there has ever been
here. That is the sense in which "several plugins of different shapes" is true of
the *framework*. This repo builds one plugin.

---

## 10. Why this drives the format decision

Restating [`old/initial_scan.md`](old/initial_scan.md) §1.6.2 with the mechanism
attached:

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
list are `[main-thread]`, while the chain those IDs describe is the audio
thread's property. So an effect swap is a main-thread/audio-thread handoff and
not just a notification — which is what `ToEngine::SetSlot` and
`ToUI::ChainChanged` are (`threading_model.md` §3 and §5).

---

## 11. Reading order

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
  bindings — and would also move `parameterTable.txt`. It has since acquired a
  second reason to be decided: having no `ParameterID` is exactly why those two
  are the last edits written straight into the engine from the message thread.
  See `tech_debt.md`, "Threading".
- **Preset compatibility.** Decided before the Boost.Fusion refactor, per
  `old/initial_scan.md` §8.3 — §7 above says the format is name-keyed, which
  gives more freedom than the index-based reading might suggest.
