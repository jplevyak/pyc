# Issue 005: `while True:` crashes FA in `update_in`

**Status:** closed (2026-07-04). Fixed in the 2026-06-26 "Interim"
commit `97f6a6c` (the `update_in` guard, landed before this doc was
ever updated to say so — caught during a 2026-07 re-check); the
guard was then structurally subsumed in `2b3bcd3` by
[ifa/issues/031](../../ifa/issues/031-globals-outside-fa-precision.md)
step 1: `GLOBAL_CONTOUR` is now a real singleton EntrySet
(`fa->global_es`) whose `in_es_worklist` is permanently 1, so the
deref that used to segfault is safe and the enqueue self-suppresses.
Committed regression coverage: `tests/while_true_loop.py` (module-
level `while True:`, the circular-list ring idiom, and a nested
in-function variant). Related but distinct, found while writing
that test and filed separately: `while True:` as a function's
*first statement* breaks FA typing —
[025-while-true-first-statement-of-function.md](025-while-true-first-statement-of-function.md)
(also since fixed and closed).

## Symptom

Any program using `while True:` segfaults pyc during FA.
Minimal repro:

```python
count = 0
while True:
  count = count + 1
  if count > 3:
    break
print(count)
```

```
$ ./pyc /tmp/while_true.py
Segmentation fault (core dumped)
```

gdb backtrace:

```
#0  update_in (v=..., t=...) at analysis/fa.cc:284
        if (!es->in_es_worklist) {              <- es is invalid
#1  update_gen at analysis/fa.cc:301
#2  add_var_constraint at analysis/fa.cc:1049
#3  add_es_constraints at analysis/fa.cc:2341
#4  analyze_edge at analysis/fa.cc:2431
#5  analyze_to_convergence at analysis/fa.cc:4271
#6  FA::analyze at analysis/fa.cc:4290
```

An AVar has `is_if_arg = 1` but `contour` is null/invalid,
so the EntrySet deref segfaults.  The IF in question is
the synthetic test for `while True:` — `True` is a
constant Sym, not a normal Var with a proper contour.

## Workaround

Use a real loop condition.  Patterns:

```python
# Replace:  while True: ...; if cond: break
# With:     while not cond: ...
```

Or use a sentinel variable:

```python
done = False
while not done:
  ...
  if cond:
    done = True
```

## Surfaced while

Implementing the fib-heap consolidate (issue 028 step 5).
The classic root-list collection idiom is

```python
w = self.min
while True:
  roots.append(w)
  w = w.next
  if w is self.min:
    break
```

This trips the crash before any of the field-access or
identity-compare bugs.

## Root cause hypothesis

`while True:` lowers to an IF whose condition is the
constant `sym_true` — its associated Var doesn't have
the same contour / is_if_arg setup as a runtime-condition
Var.  When the IF's per-branch AVar gets `is_if_arg = 1`
set in `add_pnode_constraints` (around the issue-025
narrowing block) without a matching `contour` set, the
later `update_in` deref segfaults.

## Fix sketches (superseded — see "What landed")

- Frontend: convert `while True:` to an IF1 unconditional
  loop without a runtime-tested IF (the loop just goes
  back to the entry label, exits only via `break`).
  Eliminates the synthetic IF entirely.
- FA: guard `update_in`'s `(EntrySet *)v->contour` deref
  with a null check.  Defensive; doesn't address the
  underlying "True-as-cond" lowering.

The frontend fix is cleaner; the FA guard is a useful
backstop for any other "constant cond" path.

## What landed

The **FA guard** sketch above was implemented, in `update_in`
(`ifa/analysis/fa.cc:283-303`):

```cpp
if (v->is_if_arg && v->contour != GLOBAL_CONTOUR) {
  // Guard against the GLOBAL_CONTOUR case: `make_AVar` returns a
  // globally-shared AVar (with `contour == GLOBAL_CONTOUR`, which is
  // `(void*)1`) when the Var is module-level and non-internal --
  // e.g. the `True` constant used as the condition of a top-level
  // `while True:`. `analyze_edge` then sets `is_if_arg = 1` on that
  // global AVar, but its `contour` isn't a real EntrySet -- dereffing
  // `(EntrySet *)1)->in_es_worklist` segfaults. Skipping the enqueue
  // is sound: the global AVar doesn't need per-ES re-analysis (no
  // per-ES contour to refine).
  EntrySet *es = (EntrySet *)v->contour;
  ...
```

i.e. exactly the root cause this issue diagnosed (`sym_true`'s AVar
gets `contour == GLOBAL_CONTOUR`, a sentinel, not a real `EntrySet*`,
but still gets `is_if_arg = 1` set on it by `analyze_edge`) — the fix
adds the `v->contour != GLOBAL_CONTOUR` guard before the deref rather
than switching the frontend lowering. The frontend-side sketch
(lower `while True:` to an unconditional-goto loop, no synthetic IF
at all) was **not** taken; `while True:` still lowers through the
same synthetic-IF path, just with FA now tolerating the constant
condition's degenerate AVar.

This landed in commit `97f6a6c` ("Interim", 2026-06-26), alongside a
large batch of other FA/codegen work — this issue's own doc was never
updated to reflect it until this 2026-07 re-check.

### Verification (2026-07 re-check)

- The issue's own minimal repro (`count`/`break` loop) and the
  original fib-heap root-list idiom quoted in "Surfaced while"
  (circular linked list, `while True: ...; if w is head: break`)
  both compile and run correctly on **both backends** (C and LLVM),
  with no crash and correct output.
- `tests/fibheap_full.py` and `tests/fibheap_decrease_key.py` already
  exercise this exact shape (`while True:` + identity comparison +
  field traversal) and both pass on both backends —
  `fibheap_full.py:10` even carries a comment noting it exercises
  "issue 005 GLOBAL_CONTOUR guard" explicitly, confirming this was a
  deliberate regression-guard addition, not an accidental side effect.
- `./test_pyc -k fibheap` / `PYC_FLAGS=-b ./test_pyc -k fibheap`: 5
  passed, 0 failed, both backends.
- No new test added — existing `fibheap_full.py`/
  `fibheap_decrease_key.py` already cover this shape.