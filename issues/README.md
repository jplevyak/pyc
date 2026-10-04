# issues/

Open work items for the pyc frontend and project-wide concerns:
`pyc.cc`, the `python_ifa_*` lowering, the `python.g` grammar, the
`__pyc__/` builtin module, `pyc_lib/`, the runtime (`pyc_c_runtime.h`),
and the harness. For ifa-library issues see [`ifa/issues/`](../ifa/issues/),
which uses the same conventions.

*Consolidated 2026-09-28: 25 open → 17. Resolved issues were archived,
overlapping ones merged, and every open issue was re-verified and rewritten
to its current state. The previous README, with its full per-issue index
history, is in git: `git show e3b44e2c:issues/README.md`.*

## Read first: the two rules every pyc issue is judged by

1. **CPython semantics or a compile error.** pyc compiles well-formed
   CPython 3 programs with the same semantics, for the subset typeable
   without boxing (AGENTS.md). A silent wrong answer is the worst outcome:
   worse than a crash, which is worse than a refusal. When pyc cannot do
   something, it refuses and names the reason in CPython's terms.
2. **Every Python accommodation is flagged and non-strict**
   ([171](closed/171-permissive-accommodations-must-be-flagged-and-non-strict.md)).
   Anything beyond CPython 3 or beyond static typing is an accommodation:
   accepting `raise "string"`, widening `int` to `float`, a typed default
   for an implicit `None`, a list where CPython returns a tuple. Each one
   sits behind a named flag, is on under `--permissive` (the default), is
   **off under `--strict`**, and warns when it fires. Generic ifa code gets
   Python policy only through `IFACallbacks`. For corpus programs with an
   actual type error, the alternative is a source edit; the policy is
   [shedskin_examples/PYC_CHANGES.md](../shedskin_examples/PYC_CHANGES.md).

## Open issues

### Policy and tracking

| issue | what |
| --- | --- |
| [171](closed/171-permissive-accommodations-must-be-flagged-and-non-strict.md) | **the rule and its audit** (closed 2026-09-29): every accommodation is flagged, warns, and is off under `--strict`. All thirteen items are gated, fixed, or documented. Its verification record (the strict-suite comparison and the strict corpus sweep) is how to check a new one. |
| [025](025-shedskin-examples-coverage.md) | **the corpus**: per-program state from the latest `check` sweep, edit candidates, and the 2026-09-28 first-blocker triage (13 of 17 "has no type" failures are missing builtin surface). |

### Silent wrong answers (worst first)

| issue | what |
| --- | --- |
| [173](173-silent-deviations-found-by-the-strict-suite-check.md) | found by 171's strict comparison: `isinstance` on a `{list, str}` value folds to False; a subclass does not see a mutated base class attribute; a global read before its definition sees the later value; `loop` overflows the C stack (ignores `setrecursionlimit`). |
| [007](007-decorators-not-applied.md) | decorators and descriptors. `@property` getters and dotted decorators now work, and anything unresolved is refused. Open: property setters, and a `@property` in an imported module. |
| [043](043-slice-target-augmented-assignment-silently-wrong.md) | `a[i:j] += x` acts like `a[i:j] = x`. |
| [168](168-object-at-a-computed-format-s-prints-garbage.md) | an object at a computed `%s` prints raw memory. |
| [041](041-stdlib-shim-stubs-silently-wrong.md) | `hashlib` is a stub returning `""`. Shims must be real or raise. |
| [110](110-tuple-from-iterable-returns-a-list.md) | `tuple(iterable)` returns a list in every mode. The fix is built (`PYC_MAKESEQ`) and held on a copy-of-copy deepcopy blocker; re-test after the ifa deepcopy fixes. |

### Typing and representation

| issue | what |
| --- | --- |
| [172](172-generator-send-channel-and-stopiteration-value.md) | a generator's `send` value travels as `int64` (gated by `PYC_YIELD_INT_SEND`; refused under `--strict`), and `StopIteration.value` reads the global exception slot. |
| [048](048-none-int-field-pair-runtime-abort.md) | genuine scalar unions (`{None, int}`, `{None, float}`, `{int, float}` elements): strict refuses; permissive gets a flagged sentinel or widening. |
| [125](125-in-has-no-iterable-fallback.md) | `x in y` has no iterable fallback; `object.__contains__` should be CPython's scan. |

### Diagnostics, crashes, performance, harness

| issue | what |
| --- | --- |
| [103](103-unknown-kwarg-silently-bound-positionally.md) | an unknown keyword or an undefined name is refused, but reported as `'X' has no type`, not in CPython's terms. |
| [169](169-local-class-method-reading-enclosing-local-crashes.md) | a method of a function-local class reading the function's local aborts the compiler. |
| [111](111-checks-embed-builtin-library-line-numbers.md) | COMPILE-OUT checks embed `__pyc__.py` line numbers (one left). |

### Closed on 2026-10-03

| closed | why | residual now in |
| --- | --- | --- |
| [123](closed/123-str-does-not-fall-back-to-repr.md) | `object.__str__` calls `__repr__`, as in CPython; `go` matches CPython | 173 (the default repr text) |

### Closed on 2026-09-29 and 2026-09-30

| closed | why | residual now in |
| --- | --- | --- |
| [171](closed/171-permissive-accommodations-must-be-flagged-and-non-strict.md) | every audited accommodation gated, fixed, or documented | 172, 173, 007, 128, ifa/156 |
| [124](closed/124-crlf-source-not-newline-normalized.md) | universal newlines at read time; pinned by a harness-staged CRLF test (2026-09-30) | — |
| [050](closed/050-pyc-string-builders-are-quadratic.md) | `join`/case maps/`replace`/substring in one allocation; `replace("", x)` fixed (2026-09-30) | — |
| [039](closed/039-list-mul-shared-element-type-cross-contamination.md) | leak gone at convergence (setter split of `list.__mul__`); fixture was failing on a genuine union and now tests the leak | 128 (bh's transient promotion) |
| [128](closed/128-cross-class-field-promotion.md) | field discovery behind a hook; promoted fields retracted at a fixed point; MIXED writes are a demand; no diagnostic for genuine unions | ifa/177 |

### Closed on 2026-09-28

| closed | why | residual now in |
| --- | --- | --- |
| [035](closed/035-list-element-cast-salvage-guard-and-set-item-union.md) | guard landed | 048 |
| [042](closed/042-package-directory-import-resolution.md), [113](closed/113-package-imports.md) | package imports implemented | — |
| [046](closed/046-default-arg-omitted-differently-silently-wrong.md) | fixed (prints `0`); stale sidecar removed | — |
| [107](closed/107-undefined-names-warn-then-segfault.md) | refused in every position | 103 (wording) |
| [112](closed/112-tuple-deepcopy-copy-of-copy-unresolved.md) | merged | 110 |
| [117](closed/117-property-and-reflected-ordering.md) | trivial `property` and reflected ordering landed | 007 (descriptors) |
| [120](closed/120-richards-silent-wrong-answer.md) | richards matches CPython (ifa/174) | — |
| [162](closed/162-dict-insertion-is-quadratic.md) | hashed dict (closed/118) | — |

Every archived file starts with a dated closure note. The archive is
[`closed/`](closed/).

## Conventions

- Filenames: `NNN-short-slug.md`, NNN zero-padded and never reused; the
  numbers are independent of `ifa/issues/`. One issue per file.
  Cross-link with relative paths; `make test-links` checks them.
- A status line at the top: `open`, or `closed` with a date or commit.
  Closed issues move to `closed/` with a closure note saying why and where
  any residual went.
- **Keep an open issue current, not chronological.** When a finding
  supersedes a section, rewrite it, and cite the old text by
  `git show <commit>:<path>`. A reader must be able to act on the file
  as it stands.
- Every issue carries a runnable repro (CPython's output beside pyc's),
  the cause as far as measured, the fix, the dead ends with the
  measurement that killed each, and verification.
- **Re-verify before trusting a status.** Several issues here had been
  fixed as a side effect of other work, and several "open" repros no
  longer reproduced.

## Marking a test as a known issue

A filed bug should have a test that fails on purpose. Put the issue
reference in `tests/<name>.py.known_issue`, and write the check files to
describe the **correct** behaviour (what CPython does). The run reports it
as `KNOWN`, lists it with its issue, and does not fail the suite. The day
the bug is fixed the test turns `PASS` by itself; then delete the sidecar
(the harness does not flag a sidecar whose test already passes).

`.expect_fail` / `.python.expect_fail` (`XFAIL`) are different: they
record an ACCEPTED divergence, and under rule 2 an accepted divergence is
a permissive accommodation that 171 must list. Re-check them: a marker
whose test now matches CPython is silently inert. On 2026-09-28 the four
float-repr markers (`float_literal_precision`, `colorsys_module`,
`str_builtin_call`, `minmax_3arg`) were stale and were deleted. CPython's
float repr has landed. `raise_string`'s output matches only because its
`except Exception` also catches CPython's `TypeError`; its accommodation
is real (171 #1).

## When a corpus-only bug should get a minimal test

**Minimise when the bug has a local trigger.** 035, 039, 046 and 048 all
reduced to 8-27 lines, and each reduction sharpened the diagnosis. The
tell is that you can name the construct.

**Do not minimise an emergent or stateful bug by guessing.** closed/045
(tonyjpegdecoder's second `main()` hung) is the counter-example: the
obvious reduction passed while the program still hung. Writing that test
would have recorded a false negative under an issue number. Bisect the
real program instead. If two successive cuts produce different
diagnostics, that is the finding: stop minimising and record the
full-program repro. For a convergence bug, assert the property with the
probes (`PYC_DBG_STAGES`, `PYC_DBG_OSC`) rather than a reduction that
cannot exist.

**Always check a reduction fails for the SAME reason**: the same
diagnostic at the same construct, or the same wrong output. A reduction
that fails differently has found a different bug. File that separately.

## When to file here vs fix now

File when the fix is more than about an hour and does not block the
current task, needs a design decision, or touches a subsystem the task is
not auditing. Fix now when it blocks the task, or when it is a one-line fix
whose test you are already writing.
