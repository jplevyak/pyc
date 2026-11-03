# issues/

Open work items for the pyc frontend and project-wide concerns: `pyc.cc`,
the `python_ifa_*` lowering, the `python.g` grammar, the `__pyc__/`
builtin module, `pyc_lib/`, the runtime (`pyc_c_runtime.h`) and the
harness. ifa-library issues are in [`ifa/issues/`](../ifa/issues/), same
conventions.

*Reorganised 2026-11-03: every open file rewritten to its current state
as an action item, one closed, one filed, two renamed to what they are
now about. Consolidated before that on 2026-09-28 (25 → 17). The previous
README, with the closure tables from 2026-09-28 to 2026-10-16, is in git:
`git show 23cd789:issues/README.md`.*

## Read first: the two rules every pyc issue is judged by

1. **CPython semantics or a compile error.** pyc compiles well-formed
   CPython 3 programs with the same semantics, for the subset typeable
   without boxing (AGENTS.md). Outcomes, worst first: a silent wrong
   answer, a crash, a run-time refusal, a compile-time refusal, a match.
   When pyc cannot do something it refuses and names the reason in
   CPython's terms.
2. **Every accommodation is flagged and non-strict**
   ([closed/171](closed/171-permissive-accommodations-must-be-flagged-and-non-strict.md)).
   Anything beyond CPython 3 or beyond static typing sits behind a named
   flag, is on under `--permissive` (the default), is **off under
   `--strict`**, and warns when it fires. Generic ifa code gets Python
   policy only through `IFACallbacks`. A corpus program with an actual
   type error is edited instead, under
   [shedskin_examples/PYC_CHANGES.md](../shedskin_examples/PYC_CHANGES.md).
   171's verification record (compile the suite with `--strict`, compare
   with CPython, then the strict corpus sweep) is how a new one is checked.

## The accommodations in the tree

Every deviation the default mode makes, with its flag. A new one is added
here when it lands; an item missing from this table is a bug under rule 2.

| deviation | flag | warns | `--strict` |
| --- | --- | --- | --- |
| `raise "message"` wrapped in `Exception(...)` | `PYC_RAISE_STRING` | at each site | refused |
| `{int, float}` widened to `float` (prints `1.0` for `1`) | `fruntime_errors` (ifa/145) | `show_numeric_coercions`, once per line | refused |
| a 64-bit literal past int64 wraps | `PYC_INT_LITERAL_WRAP` | at the literal | refused |
| a non-priming `next()` on a generator whose send cell is `{None, scalar}` delivers 0 | `PYC_SEND_NONE_ZERO` | at the `def` | refused |
| locals possibly read before assignment are zero-filled | `--safe` | no | not set |
| the implicit-`None` fall-off replaced by a typed default | `PYC_NO_IMPLICIT_NONE=1` (opt-in) | yes | ignored, with a warning |
| `zip`/`map`/`filter`/`reversed` are eager lists (observable only through `len`, indexing or printing the object, which CPython rejects) | none: a documented representation, author's decision 2026-09-29 | no | same |
| a write to a missing field discovers the field | `IFACallbacks::discovers_fields_by_write` | no: the Python-ism, not a deviation | same |

## The plan

Ordered by value against effort. Items in one row are independent.

| order | do | why | unblocks |
| --- | --- | --- | --- |
| 1 | the silent wrong answers with a local fix: [185](185-list-eq-tuple-is-true.md), [173](173-silent-deviations-found-by-the-strict-suite-check.md) item 1, [180](180-method-assigned-across-classes-crashes.md) | rule 1's worst outcome, each a day | |
| 2 | exception flow along the call graph, once: [175](175-an-exception-raised-in-an-operator-method-does-not-propagate.md) step 2 | three issues wait on it and the up-front check is measured too expensive | [172](172-stopiteration-value-reads-the-global-exception-slot.md), [179](179-sequence-index-out-of-range-does-not-raise.md), tuple `__setitem__`, ifa/165 |
| 3 | [175](175-an-exception-raised-in-an-operator-method-does-not-propagate.md) step 1 (operators, `ZeroDivisionError`) and [179](179-sequence-index-out-of-range-does-not-raise.md) | silent memory reads and a SIGFPE on the default path | |
| 4 | the crashes: [173](173-silent-deviations-found-by-the-strict-suite-check.md) item 3 (recursion limit), [187](187-self-import-overflows-the-frontend.md) | a crash beats a wrong answer but not by much | corpus `loop` |
| 5 | the refusals of valid programs: [184](184-classmethod-attribute-through-cls.md), [186](186-duplicate-method-definitions-are-an-ambiguous-call.md), [182](182-kwargs-and-keyword-only-parameters.md), [007](007-decorators-not-applied.md) items 1–2 | each is a common idiom with a named cause | |
| 6 | [173](173-silent-deviations-found-by-the-strict-suite-check.md) item 2 through ifa/050's write-once globals | the mechanism belongs to FA | |
| 7 | [177](177-int64-overflow-at-run-time-is-silent.md): measure the checked path | the one deviation `--strict` cannot see | othello2 under `--strict` says why it differs |
| 8 | [048](048-none-int-field-pair-runtime-abort.md), [169](169-local-class-method-reading-enclosing-local-crashes.md): wait for a program that needs them | refusals, documented, no corpus witness | |

## Open issues

### Silent wrong answers (worst first)

| issue | what |
| --- | --- |
| [185](185-list-eq-tuple-is-true.md) | `[1, 2] == (1, 2)` is True: `list.__eq__` never checks its argument is a list. |
| [179](179-sequence-index-out-of-range-does-not-raise.md) | an out-of-range index into a list, str or bytes reads memory instead of raising `IndexError`. |
| [175](175-an-exception-raised-in-an-operator-method-does-not-propagate.md) | a raise inside an operator or `__getitem__` never reaches the handler; `7 // 0` is a SIGFPE; the item-subscript check costs +72 % compile time if emitted up front. |
| [173](173-silent-deviations-found-by-the-strict-suite-check.md) | a subclass does not see a mutated base attribute; a global read before its definition sees the later value; deep recursion overflows the C stack and `setrecursionlimit` is ignored. |
| [180](180-method-assigned-across-classes-crashes.md) | `a.x = b.x` across unrelated classes compiles clean, then `a.x()` segfaults: a field holding a closure called as a method slot. |
| [177](177-int64-overflow-at-run-time-is-silent.md) | run-time int64 overflow wraps silently in every mode (othello2); literals are warned or refused. |

### Refused, not wrong

| issue | what |
| --- | --- |
| [172](172-stopiteration-value-reads-the-global-exception-slot.md) | `except StopIteration as e: e.value` reads the one global exception slot, the union of every raise in the program. |
| [184](184-classmethod-attribute-through-cls.md) | `cls.base` and `cls.h()` inside a `@classmethod` have no type; `cls(...)` works. |
| [186](186-duplicate-method-definitions-are-an-ambiguous-call.md) | a class defining a method twice is an ambiguous call; CPython takes the last. |
| [182](182-kwargs-and-keyword-only-parameters.md) | `**kwargs` parameters and keyword-only parameters. |
| [007](007-decorators-not-applied.md) | property setters and deleters, `property(GETTER)` with a non-trivial getter, class-based decorators. |
| [048](048-none-int-field-pair-runtime-abort.md) | a genuinely read `{None, scalar}`: refused in every mode; the permissive sentinel is built only when a program needs it. |
| [169](169-local-class-method-reading-enclosing-local-crashes.md) | a local class whose captured value has a different type per call (one prototype per class statement). |

### Crashes and tracking

| issue | what |
| --- | --- |
| [187](187-self-import-overflows-the-frontend.md) | `import X` from `X.py` recurses without bound in the frontend. |
| [025](025-shedskin-examples-coverage.md) | **the corpus**: 76 of 77 compile, 58 match, 18 unverifiable by the sweep and verified by hand, othello2 differs (177), othello3 exceeds the compile cap (ifa/186); the two edit candidates. |

### Closed on 2026-11-03

| closed | why | residual now in |
| --- | --- | --- |
| [110](closed/110-tuple-from-iterable-returns-a-list.md) | `tuple(x)` is a real tuple in every mode since 2026-09-28 | 175 step 3 (`tuple.__setitem__`); the eager lists are in the table above |

Every archived file starts with a dated closure note. The archive is
[`closed/`](closed/).

## Conventions

- Filenames: `NNN-short-slug.md`, NNN zero-padded and never reused; the
  numbers are independent of `ifa/issues/`. A number is cited from code
  comments and sidecars, so a renamed slug keeps its number. One issue per
  file. Cross-link with relative paths; `make test-links` checks them.
- A status line at the top: `open`, or `closed` with a date. Closed issues
  move to `closed/` with a closure note saying why and where any residual
  went.
- **Keep an open issue current, not chronological.** When a finding
  supersedes a section, rewrite it and cite the old text by
  `git show <commit>:<path>`. A reader must be able to act on the file as
  it stands. Dead ends stay only as "do not re-run" rows with the
  measurement that killed them.
- Every issue carries a runnable repro (CPython's output beside pyc's),
  the mechanism as far as measured, the plan, the dead ends, and
  verification.
- **Re-verify before trusting a status.** Several issues here were fixed
  as a side effect of other work, and several "open" repros no longer
  reproduced.

## Marking a test as a known issue

A filed bug should have a test that fails on purpose. Put the issue
reference in `tests/<name>.py.known_issue`, and write the check files to
describe the **correct** behaviour (what CPython does). The run reports it
as `KNOWN`, lists it with its issue, and does not fail the suite. The day
the bug is fixed the test turns `PASS` by itself; then delete the sidecar.
When an issue is closed or merged, repoint its sidecars.

`.expect_fail` / `.python.expect_fail` (`XFAIL`) are different: they
record an ACCEPTED divergence, and under rule 2 an accepted divergence is
an accommodation that the table above must list. Re-check them: a marker
whose test now matches CPython is silently inert. `raise_string`'s output
matches only because its `except Exception` also catches CPython's
`TypeError`; its accommodation is real. `class_attr_mutation`'s marker is
the wrong way round (it bakes in pyc's answer) and should become a
`.known_issue` under 173.

## When a corpus-only bug should get a minimal test

**Minimise when the bug has a local trigger**: the tell is that you can
name the construct. **Do not minimise an emergent or stateful bug by
guessing**: closed/045 (tonyjpegdecoder's second `main()` hung) is the
counter-example, where the obvious reduction passed while the program
still hung. Bisect the real program; if two successive cuts produce
different diagnostics, that is the finding. For a convergence bug, assert
the property with the probes (`PYC_DBG_STAGES`, `PYC_DBG_OSC`). **Always
check a reduction fails for the SAME reason**: the same diagnostic at the
same construct, or the same wrong output. One that fails differently has
found a different bug; file that separately.

## When to file here vs fix now

File when the fix is more than about an hour and does not block the
current task, needs a design decision, or touches a subsystem the task is
not auditing. Fix now when it blocks the task, or when it is a one-line
fix whose test you are already writing.
