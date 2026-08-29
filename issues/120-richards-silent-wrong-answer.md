# 120 — richards compiles clean, runs, and prints the wrong answer

**Status:** open, filed 2026-08-29 from the issues/119 corpus A/B.
**Affects:** `shedskin_examples/richards/richards.py`; a polymorphic
dispatch that resolves to nothing useful.
**Severity:** silent. Zero warnings, exit 0, wrong output — the shape
[ifa/102](../ifa/issues/102-corpus-programs-compile-then-abort-at-runtime.md)
is about, one step worse than the abort it replaced.

## Symptom

```
$ ./richards
False        (x10)
TIME 0.00
```

CPython prints `True`. Verified on a reduced copy (`iterations = 20`,
`for n in range(2)`) so CPython finishes: it prints `True`.

`TIME 0.00` is the tell — the benchmark's ten 1000-iteration runs
complete instantly, so the task scheduler is doing no work at all, and
`run()`'s self-verification then fails.

## History: this is a failure mode that MOVED, not a new defect

Before [issues/119](closed/119-nested-tuple-repr-aborts.md) (clean
`f2501586`) richards aborted instead:

```
richards.py.c:3518: _CG_any _CG_f_11410_86(_CG_any): Assertion
  `!"runtime error: polymorphic dispatch: no branch matched"' failed.
```

So the baseline could not produce the right answer either — it died in a
dispatch with no matching branch, having printed nothing.

**119 did not cause the wrong answer.** richards has no dict, no set, no
`hash()` call, and never prints a tuple, so unrolling
`tuple.__str__`/`__hash__` cannot reach its semantics. What changed is
which tuple methods are instantiated, which perturbs FA enough that the
dispatch no longer takes the abort branch. Confirmed not to be the
layout half either: `PYC_TUPLE_AS_LIST=0` and `=1` both print `False`.

The two failures are almost certainly the same underlying defect —
a polymorphic dispatch over a receiver union that has no runtime
discriminator — seen from two sides. Compare
[ifa/030](../ifa/issues/030-DISPATCH-polymorphic-dispatch-fat-pointers.md), which is
that exact shape.

## Why the sweep did not flag it

`corpus_sweep.sh -m check` compares stdout against CPython, but CPython
itself times out on richards at the default 120s (`cpy_rc=124`), so the
row records `stdout_match=-` and the program counts as neither a
run-failure nor a stdout difference. It moved `run_fail` 42 → 41 and so
read as a one-program IMPROVEMENT in the summary line.

Worth fixing in the harness independently of this bug: a program whose
CPython reference times out has no oracle, and should be reported as
`no-oracle` rather than silently passing.

## Next step

Find the dispatch. `_CG_f_11410_86(_CG_any)` in the baseline build is the
place to start — it is the one that used to abort. Establish what
receiver union reaches it and whether the branch it now takes is simply
the wrong one.

## Verification plan

- A reduced richards prints `True` on both backends and matches CPython.
- The full program prints `True` ten times and a nonzero `TIME`.
- `corpus_sweep.sh -m check` reports richards with a real oracle.
