# SpectrumWorx — the tech documents

Six documents, and every one of them describes the tree **as it is now**. Four
say how something works, one says what is left to do, one says what finished work
left behind. Nothing here is a plan being executed or a record of how the port
got where it is; that is in [`old/`](old/).

## How it works

| | What it is |
|---|---|
| [`threading_model.md`](threading_model.md) | Which thread owns the engine, the three channels they talk over, and what the layering forbids. |
| [`parameter_system.md`](parameter_system.md) | How the plugin addresses, names and exports its 286 parameters, and why the answer is "dynamic" in a way that constrains the format choice. |
| [`streaming_format.md`](streaming_format.md) | What goes into a preset and into session state: the on-disk names, the snapshot tests that pin them, and the rules for changing any of it. |
| [`how-lfo-rates-work.md`](how-lfo-rates-work.md) | What an LFO's period holds, which bar it is a fraction of, and what tempo sync does and does not move. |

## What is left

| | What it is |
|---|---|
| [`todo.md`](todo.md) | **The work queue.** Two ordered items and a page of smaller things. An item that closes comes out of the file. |
| [`tech_debt.md`](tech_debt.md) | What finished work left behind — the half-fix, the correct-but-unsatisfying answer, the finding with no owner. An entry that is remediated comes out too. |

The line between them: `todo.md` is work somebody will sit down and do;
`tech_debt.md` is what would still be true if all of it were done exactly as
written.

## [`old/`](old/) — the path, kept for the reasoning

Neither of these describes this tree. Both are the record of how it was read and
planned before it built, both contain claims the port has since disproved, and
both are kept because they are the only account of *why* several decisions went
the way they did.

| | Read it for |
|---|---|
| [`old/initial_scan.md`](old/initial_scan.md) | The analysis pass on the 2016 snapshot, before anything was touched. Its inventory of what that tree contained is not repeated anywhere else. |
| [`old/implementation_sequence.md`](old/implementation_sequence.md) | The nine-stage plan the port was executed against, plus the per-stage "done when" each commit was measured against. |

---

## Conventions these documents share

- **A claim carries its date and its evidence.** "As of 02.08.2026" and a file
  and line, or a test name, or a measured number. A bullet with no provenance is
  unverifiable a month later, which is the same as being false.
- **A document says what is true, not what happened.** When something closes it
  leaves the todo list or the debt list rather than being struck through in
  place. If the reasoning behind it was worth keeping — and often it is, because
  several of the most useful paragraphs here are about something that turned out
  to be wrong — it moves into whichever "how it works" document owns the
  mechanism, stated as a property of the design rather than as a story.
- **Test counts are stated as of a date.** They move every time a case lands, so
  only the count in `todo.md`'s status table is meant to be current.
- **The two test binaries are `sw-dsp-tests` and `sw-plugin-tests`**, and `ctest`
  runs both. There is no `sw-tests`; it split on 02.08.2026 so that the engine's
  cases could link without JUCE, which is what proves the engine does not need
  it.
