# Issue 028: `raise Exception(...)` regressed by the qualified-static-dispatch commit (bh, richards)

**Status:** open.
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
