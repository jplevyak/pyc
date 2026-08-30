# 120 — richards compiles clean, runs, and prints the wrong answer

**Status:** open, filed 2026-08-29 from the issues/119 corpus A/B.
**Affects:** `shedskin_examples/richards/richards.py`; a polymorphic
dispatch that resolves to nothing useful.
**Severity:** silent. Exit 0, wrong output (it does emit 4 warnings, all named below) — the shape
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

## Investigated 2026-08-30: three defects, one of them root-caused

`schedule()` walks the task list and does nothing, so `holdCount` and
`qpktCount` stay 0 and the self-check fails. Instrumenting the loop
against CPython:

```
        CPython                 pyc
loop 1  ident 6                 ident 6
loop 2  ident 5                 ident 5
loop 3  ident 4                 ident 3000        <- priority, not ident
loop 4  ident 4                 ident 0
loop 5  ident 4                 ident -44889163002019841
```

**1. Wrong field offsets — FIXED, see
[121](121-sibling-subclass-field-layout.md).** The four `Task`
subclasses get their inherited data fields at different struct slots
(`ident` at e31/e32/e33 across siblings), so a union receiver reads a
neighbouring field. `3000` is `HandlerTask`'s `priority`. Fixing this
alone makes the traced prefix match CPython **exactly**. Landed 2026-08-30;
richards still prints False, so 2 and 3 remain.

**2. `t` is typed `Packet` in `schedule()`.** Two warnings pyc already
emits:

    richards.py:358: illegal call argument type 't' illegal: Packet

`Packet` and `Task` both have `link` and `ident` fields. Renaming
`Packet.link` to `plink` (semantics-preserving; CPython still prints
`True`) removes both warnings, so the task list's element type really is
being contaminated by `Packet`. A two-class shared-field-name repro
(`tests/`-sized, both classes with `link` + their own id field) does
NOT reproduce it, so something further in richards' shape is required —
not yet isolated.

**3. The `TaskRec` union is not narrowed.** Two more warnings:

    richards.py:272: illegal call argument type 'h'
                     illegal: ( DeviceTaskRec IdleTaskRec WorkerTaskRec )

`Task.handle` is one member slot shared by all four subclasses, so
`self.fn(msg, self.handle)` in `runTask` passes the union of all four
`TaskRec` types into each `fn`, and `assert isinstance(h,
HandlerTaskRec)` does not narrow it away. The union is pure imprecision
— a `HandlerTask`'s handle is always a `HandlerTaskRec`. A 20-line
repro of that shape (base storing a handle, subclasses asserting its
type) compiles clean and runs correctly, so again richards needs more
than the obvious pattern. Converting the `assert` to `if not
isinstance(...): return` makes it worse (11 warnings, compile fails).

With 1 fixed and 2 worked around by the rename, richards still prints
`False` — so 3, or something past it, is independently wrong.
