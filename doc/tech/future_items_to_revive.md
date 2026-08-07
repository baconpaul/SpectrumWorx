# Things worth building, that this tree deliberately does not have

A removal is not always a judgement that the idea was bad. Sometimes it is a
judgement that the *code* was not worth carrying — never compiled, never
measured, and holding a switch open across dozens of files for a capability
nobody could reach.

This document is where those go. One entry per idea, and each says the same three
things: what it is, why it is not here, and what reviving it would actually cost.
Anything whose cost is only "write the code" does not belong here; that is
`todo.md`. What belongs here is an idea whose price is paid in *compatibility* —
a parameter table, a preset key, an automation lane — because that price is the
part nobody remembers a year later.

The rule the other documents follow applies here too: an entry that gets built
comes out of this file, and an entry that is decided against for good comes out
as well. Neither is struck through in place.

---

## Window presum

**What it is.** Analyse a window several times longer than the FFT by folding it
down before transforming. Multiply the long window over the long input, add the
consecutive frame-sized blocks into one frame-sized buffer, transform that.
Folding in time is decimation in frequency, so the spectrum lands on the same
bin grid but each bin's analysis filter is narrower and its sidelobes lower —
better frequency selectivity at the same bin count and the same FFT cost. It is
paid for in latency and in time resolution: the analysis window really is longer,
so transients smear across more of it.

It is a standard phase-vocoder technique and it is the cheapest resolution
upgrade available to this engine, which is why it is written down rather than
forgotten.

**Why it is not here.** Removed 07.08.2026. It was a build option
(`SW_ENGINE_WINDOW_PRESUM` → `LE_SW_ENGINE_WINDOW_PRESUM`) defaulting to OFF,
34 `#if` sites across 15 files, guarding a `WindowSizeFactor` global parameter
and the plumbing that let the fold factor be anything other than one.

Three findings decided it, and the third is the one that matters:

1. **Turning it on grew the parameter table by a row**, which moved
   `parameterTable.txt`, `presetCorpus.txt` and `streamingNames.txt` and failed
   7 golden cases. That is not a fault — it is the decision the option really
   was, since a global parameter added renumbers every automation lane a host
   has saved.
2. **It had never been compiled.** The first attempt, on 04.08.2026, found that
   `WindowSizeFactor`'s own constructor named its base class unqualified — code
   that no compiler had ever seen.
3. **Nobody has ever had the parameter.** The 2016 build defaulted the option
   `false` as well, and no preset, corpus entry or golden fixture in this
   repository carries a `WindowSizeFactor` attribute. The pre-2.7 migration
   branch in `presets.cpp` was therefore compatibility code for a preset field
   that nothing has ever written. There was no installed base to preserve.

**What reviving it costs.**

- **A real bug first.** The window length is `fftSize * windowSizeFactor` held in
  a `std::uint16_t`, so at the maximum FFT size the factor cannot exceed one.
  A host writing the parameter's maximum is exactly what the CLAP validator's
  range case does, so this aborts rather than degrades. Widening that type is
  the first commit, not an afterthought.
- **A parameter table decision.** Adding a global parameter is an ABI change for
  automation. See [`parameter_system.md`](parameter_system.md) for what the
  numbering guarantees and [`streaming_format.md`](streaming_format.md) for the
  three snapshots that pin it. The goldens will need one deliberate rebaseline.
- **A UI slot**, since a factor the user cannot reach is not a feature.

**Where the code is.** In git, in the commit that removed it — search the history
of `src/le/spectrumworx/engine/setup.hpp` for `windowSizeFactor_`. The analysis
fold and the synthesis unfold were live code running at a factor of one, in
`channelData.cpp` and `channelBuffers.cpp`; they were collapsed to the
factor-of-one case in the same removal, so the arithmetic is in that diff too.
