# Issue 025: `while True:` as a function's first statement breaks FA typing

**Status:** open. Found 2026-07-04 while building the committed
regression test for closing issue 005 (`tests/while_true_loop.py`
— its third block deliberately avoids this shape and points here).
**Affects:** FA (both backends fail identically, before codegen
diverges); suspected first-PNode-at-function-entry handling, the
same structural class as issue 002's `simple_inlining`
`cfg_pred[0]` bounds bug (a construct at the very first PNode of a
function, where `cfg_pred` is legitimately empty).

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

## Root cause hypothesis

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

## Verification plan

1. Reproduce with the program above (`rc != 0` / wrong output).
2. Fix; both backends print `5`.
3. Extend `tests/while_true_loop.py`'s third block to drop its
   deliberate `local_guard` leading statement (and remove the
   pointer to this issue from its comment).
4. Full suites both backends.

## What this unblocks

- The natural way to write retry/poll/consume loops
  (`def worker(): while True: ...`) without a dummy leading
  statement. Today the failure mode is confusing (a type warning
  plus a runtime dispatch abort, or an LLVM verifier error,
  depending on shape) with no hint about the workaround.
