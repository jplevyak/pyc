# Issue 025: `while True:` as a function's first statement breaks FA typing

**Status:** closed (2026-07-04, same day as filing). Root cause
traced (not the FA-entry-constraint hypothesis — see "What landed"
below): when the body's first leaf Code is the loop-header LABEL,
`Fun::build_cfg` made that label the entry PNode, so the implicit
function-entry path had no CFG edge; SSU's `new_phiphy` sizes a
loop-header phi by `cfg_pred.n`, so the phi saw only the back edge
— the formals' values never flowed into the loop variables (traced
directly: the phi had one rval, a forever-bottom Var, while the
formal's AVar held the actual's type). Fixed structurally in
`ifa/optimize/cfg.cc` `Fun::build_cfg`: prepend a synthetic
`Code_NOP` when the first leaf is a LABEL, so the entry PNode is
never a jump target. This also fixed the second symptom for free
(the LLVM `Entry block to function must not have predecessors!`
verifier failure and the C missing-return warning — back edges now
target the label, not the entry). Regression coverage:
`tests/while_true_loop.py` (in-function `while True:`-first with
break-only exit, and the original loop-over-a-parameter repro).
Found 2026-07-04 while building the committed regression test for
closing issue 005.
**Affects:** `ifa/optimize/cfg.cc` (`Fun::build_cfg`) — the entry
PNode selection; consumed by `ifa/optimize/ssu.cc` phi sizing and
the LLVM emitter's entry-block assumption. Same structural class
as issue 002's `simple_inlining` `cfg_pred[0]` bounds bug (a
construct at the very first PNode of a function).

## Symptom

```python
def run(n):
  while True:      # first statement of the function
    n = n + 1
    if n >= 5:
      break
  return n

print(run(0))
```

Both backends:

```
warning: 'n' has no type
  called from wt_first.py:8
warning: 'n' has no type
wt_first: wt_first.py.c:44: void* _CG_f_4061_0():
  Assertion `!"runtime error: matching function not found"' failed.
```

The parameter `n` never gets a type, and the `n + 1` dispatch
inside the loop falls back to a runtime matching-function abort.

Two adjacent shapes are fine, isolating the trigger precisely:

- Any statement before the loop (`i = 0` first, or loop over a
  fresh local instead of the parameter with an init statement) —
  works on both backends.
- `while True:` first at **module top level** — works (that was
  issue 005's fixed case; `tests/while_true_loop.py` covers it).

A second, downstream symptom of the same shape (seen with a
`while True:`-first function whose only exit is `break`, C
backend): the generated C function falls off the end without a
`return` (`-Wreturn-type` warning), and the LLVM verifier rejects
the module with `Entry block to function must not have
predecessors!` — the loop-back label IS the entry block, so the
back-edge targets function entry. Fixing the FA typing may not
fix this second half: codegen likely also needs the loop-back
label split from the entry block (or an implicit leading label)
when the loop opens the function body.

## Root cause hypothesis (original; resolved as #2, via SSU)

The loop-back LABEL for a `while True:` that opens a function body
is the function's first PNode. Suspects, in order:

1. FA's entry handling for a first PNode that is a loop header —
   the formal's AVar may be consulted through a path that assumes
   at least one predecessor/ordinary entry constraint before the
   back-edge re-enters (the "'n' has no type" warning suggests the
   formal's type never flows into the loop body's contour).
2. `if1_label`/CFG construction putting the back-edge target at
   `Fun::entry` itself rather than a successor label node —
   directly observed as the LLVM entry-block-predecessor verifier
   failure.

## What landed

Root cause was #2, with the FA symptom (#1's evidence) arriving
via SSU, not FA proper. Traced with temporary per-pnode tracing in
`add_pnode_constraints` (fun-name-gated, printing phi lval/rval
Var identities and `Fun::args` AVar out-sizes):

- `Fun::build_cfg` sets `entry = sym->code->pn` — the first leaf
  Code's PNode. With `while True:` opening the body, that leaf is
  the loop-header LABEL, so the entry PNode was a jump target and
  the implicit function-entry path had no CFG edge.
- `ssu.cc`'s `new_phiphy` sizes a loop-header phi's rvals by
  `y->cfg_pred.n`; with only the back edge as a predecessor, the
  phi had ONE rval, and `rename_edge` overwrote it with the
  loop-renamed Var. Observed directly: phi rval = a forever-bottom
  Var (id 3627, out 0) while the formal (id 3609) held the
  actual's type (out 1) — disconnected. Every downstream use of
  the loop variable stayed bottom, call edges got skipped on
  bottom actuals, and the runtime hit the `matching function not
  found` abort.
- Fix: `Fun::build_cfg` prepends a synthetic `Code_NOP` to the
  body when its first leaf Code is a LABEL, so the entry PNode is
  never a jump target. The NOP PNode has no lvals, stays dead for
  emission on both backends, and restores the invariant every
  consumer (SSU phi sizing, rename, LLVM entry block) silently
  assumed.
- Both downstream symptoms disappeared with the same change: the
  LLVM verifier failure (back edge now targets the label block,
  not entry) and the C `-Wreturn-type` fall-off-the-end warning
  (the reply path now emits normally).

## Verification plan (done)

1. ~~Reproduce with the program above.~~ Done — both backends,
   plus the break-only-exit variant for the verifier symptom.
2. ~~Fix; both backends print `5`.~~ Done, zero warnings.
3. ~~Extend `tests/while_true_loop.py`'s third block to drop its
   deliberate `local_guard` leading statement.~~ Done — plus a
   fourth block with the original loop-over-a-parameter repro.
4. ~~Full suites both backends.~~ 128/0 (C), 128/0 (LLVM),
   `ifa-test` 14/14.

## What this unblocks

- The natural way to write retry/poll/consume loops
  (`def worker(): while True: ...`) without a dummy leading
  statement. Today the failure mode is confusing (a type warning
  plus a runtime dispatch abort, or an LLVM verifier error,
  depending on shape) with no hint about the workaround.
