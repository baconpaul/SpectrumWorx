# SpectrumWorx — the tech documents, and which of them to read

Seven documents, written in the order the work happened rather than in the order
a newcomer would want them. This says what each one is for **now**, so that the
question "which of these is still true?" has an answer that is not "read all
6,900 lines".

**Where this is going.** One architecture document — what the plugin is, how the
three threads divide it, what the parameter and preset formats are, and what is
deliberately not done — with the rest deleted or folded into it. That is not
today: three of the four live documents below are still being written against,
and consolidating a plan while it is being executed loses the reasoning without
gaining a reader. The trigger to do it is item 1 (a DAW drive that confirms the
threading redesign) plus item 6 (CI) in `week_two.md`; at that point the
sequence and the scan have nothing left to owe and this file becomes the outline
of what replaces them.

---

## Live — read these

| | What it is | Status |
|---|---|---|
| [`week_two.md`](week_two.md) | **The work queue.** Ten ordered items, four still open, plus four audits of the tree as it stands. Supersedes the two originals below wherever they disagree. | current |
| [`tech_debt.md`](tech_debt.md) | What finished work left behind — the half-fix, the correct-but-unsatisfying answer, the finding with no owner. Appended to as work happens. | current |
| [`correct_the_threading.md`](correct_the_threading.md) | The threading redesign: the design, its nine stages (all landed), and what it deliberately did not do. `week_two.md` §1 item 3 is a pointer to it. | current; **unverified in a DAW** |
| [`streaming_format.md`](streaming_format.md) | What goes into a preset and a session: the on-disk names, the three snapshot tests that pin them, and the rules for changing any of it. | current |

## Reference — accurate, and about the code rather than the plan

| | What it is |
|---|---|
| [`parameter-system.md`](parameter-system.md) | How the plugin addresses, names and exports its 286 parameters, and why the answer is "dynamic" in a way that constrains the format choice. Written against the 2016 shape and still true of it: the mechanism did not change in the port. |

## Evidence — kept for the reasoning, not for the plan

Neither of these is a to-do list any more. Both are the record of how the tree
was read before it built, and both contain claims that the port has since
disproved; where they disagree with `week_two.md`, `week_two.md` is right.

| | What it is | Read it for |
|---|---|---|
| [`initial_scan.md`](initial_scan.md) | The analysis pass on the 2016 snapshot, before anything was touched. | Why a decision went the way it did. Its inventory of what the 2016 tree contained is not repeated anywhere else. |
| [`implementation_sequence.md`](implementation_sequence.md) | The nine-stage plan the port was executed against, stages 0–8 marked as they closed. | The same, at a finer grain, plus the per-stage "done when" that each commit was measured against. |

---

## Conventions these documents share

- **A claim carries its date and its evidence.** "As of 02.08.2026" and a file
  and line, or a test name, or a measured number. A bullet with no provenance is
  unverifiable a month later, which is the same as being false.
- **A superseded passage is struck through and kept, not deleted**, with a note
  saying what replaced it and why. Several of the most useful paragraphs in
  these files are about something that turned out to be wrong.
- **Test counts are stated as of a date.** They move every time a case lands, so
  a count inside a "done" block is history and correct; only the counts in a
  document's status table are meant to be current.
- **The two test binaries are `sw-dsp-tests` and `sw-plugin-tests`**, and `ctest`
  runs both. There is no `sw-tests`; it split on 02.08.2026 so that the engine's
  cases could link without JUCE, which is what proves the engine does not need
  it.
