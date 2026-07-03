# Issue 005: `while True:` crashes FA in `update_in`

**Status:** open.

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

## Fix sketches

- Frontend: convert `while True:` to an IF1 unconditional
  loop without a runtime-tested IF (the loop just goes
  back to the entry label, exits only via `break`).
  Eliminates the synthetic IF entirely.
- FA: guard `update_in`'s `(EntrySet *)v->contour` deref
  with a null check.  Defensive; doesn't address the
  underlying "True-as-cond" lowering.

The frontend fix is cleaner; the FA guard is a useful
backstop for any other "constant cond" path.