# Issue 028: `raise Exception(...)` regressed by the qualified-static-dispatch commit (bh, richards)

**Status:** **closed 2026-08-03** — no longer reproduces, fixed by
`6d3bf055` ("Frontend: builtin Exception class + break-label scoping
fix", 2026-07-14, four days after this issue was filed — see "Status
check" below).
**Affects:** commit `a32a6467` ("Add @staticmethod/@classmethod and
qualified static dispatch, closes 027") — pyc frontend lowering.
**Found:** 2026-07-10, during issue ifa/033 stage-A corpus
validation (the change predates that session's work; bisected to
`a32a6467` by building its parent `7d7a86a2` in a worktree and
re-sweeping the four examples whose bucket membership drifted).

## Symptom

Two shedskin-corpus examples regressed from COMPILED_C_WARN to
FAIL between the ifa/033-D7 sweep record (at `8bf0d74d`) and HEAD:

```
bh        WARN -> FAIL   warning: 'Exception' has no type
richards  WARN -> FAIL   warning: 'Exception' has no type
```

richards' first diagnostic at HEAD:

```
warning: 'Exception' has no type
richards.py:235: illegal call argument type expression illegal:
  called from richards.py:225
```

richards.py:235 is `raise Exception("Bad task id %d" % id)`; bh
uses the same `raise Exception(...)` shape. At the parent commit
`7d7a86a2` both examples compile (with their old, unrelated
warnings: `richards.py:244 illegal call argument type 'self'
illegal: DeviceTask`, `bh.py:248 expression has no type`).

The same commit also FIXED two examples (go, loop: FAIL -> WARN),
so the corpus bucket COUNT stayed 22 compiled / 55 failed and a
count-only comparison missed the trade. Sweep membership checks
must compare the member SET (the ifa/033 D7 section already
records the set for exactly this reason).

## Root cause (not yet traced)

Not investigated beyond attribution. Plausible: the qualified
static dispatch changes in `a32a6467` altered how a call to a
class name that is never subclassed/instantiated-with-args
elsewhere (`Exception("...")` inside a `raise`) resolves, leaving
the `Exception` symbol with no type. Start at that commit's diff
in `python_ifa_build_if1.cc` / `python_ifa_build_syms.cc`
(qualified-name call paths) and at how `raise` lowers its operand.

## Verification plan

- `./pyc shedskin_examples/richards/richards.py` produces no
  `'Exception' has no type` warning and no line-235 violations;
  same for bh.
- `bash shedskin_sweep.sh` bucket membership: bh and richards back
  in COMPILED_C_WARN, go and loop still compiling (don't re-trade).
- Full pyc suites (C + LLVM) green; a minimal
  `raise Exception("msg %d" % n)` test added to `tests/`.

## What it unblocks

- richards is a standard benchmark (used e.g. in ifa/033's
  acceptance list as a converging FA reference); keeping it
  compiling keeps that baseline meaningful.

## Status check (2026-08-03)

Re-verified against current HEAD: `./pyc shedskin_examples/richards/richards.py`
and `./pyc shedskin_examples/bh/bh.py` both compile clean (exit 0) —
no `'Exception' has no type` warning, no line-235 violation on
richards; both back to their original `COMPILED_C_WARN` bucket
(only the pre-existing, unrelated `DeviceTaskRec`/`Packet`-dispatch
warnings remain on richards).

**Root cause confirmed, not just attributed.** `git log` on
`python_ifa_build_if1.cc`/`python_ifa_build_syms.cc` between this
issue's filing and HEAD turned up `6d3bf055` ("Frontend: builtin
Exception class + break-label scoping fix", 2026-07-14), whose own
commit message names the exact mechanism: *"pyc's builtins never
defined an Exception class, so `class X(Exception)` resolved the base
to a plain non-type Sym"* (filed originally against issue 025's
"signal 117" SIGSEGV family, not this issue — but the same missing
real-`Exception`-type gap also explains a bare `Exception(...)`
constructor call inside a `raise` resolving to no type, exactly this
issue's symptom). This predates even issue 011's full exception-handling
implementation (`04d56587`, 2026-07-17) by three days, so the original
"qualified static dispatch broke `Exception` resolution" attribution to
`a32a6467` was directionally right (that commit's era is when the
regression appeared) but the actual fix turned out to be unrelated to
qualified dispatch at all — just a genuinely missing builtin.

**One item from the original verification plan does NOT hold today,
but for an unrelated reason:** `go.py`/`loop.py` (the sibling pair
this issue's own filing noted had traded FAIL→WARN in the same
commit, hence "don't re-trade") currently fail to compile again — but
with `divmod`/`self`-type-mismatch warnings, nothing to do with
`Exception` or qualified dispatch. This is corpus drift from
unrelated work sometime after 2026-07-10, not a regression caused by
`6d3bf055` or anything else touching this issue's actual mechanism;
worth tracking under issue 025 (shedskin-examples-coverage) rather
than reopening this one.

**New regression test added:** `tests/raise_exception_qualified.py`
(+ `.exec.check`) — the exact shape from the verification plan
(`raise Exception("Bad task id %d" % n)` inside a function, caught by
`try/except Exception as e`), byte-for-byte matching real `python3`
output on **both** backends. Full regression suite clean on both:
`test_pyc.py` and `PYC_FLAGS=-b test_pyc.py` each 237 passed / 0
failed / 11 expected fails / 4 skipped (one more pass than before —
the new test).
