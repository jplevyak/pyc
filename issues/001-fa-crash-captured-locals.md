# Issue 001: FA assertion crash on closures capturing enclosing-scope locals

**Status:** open.
**Affects:** `ifa/analysis/fa.cc:154` (`unique_AVar` assertion);
fires from pyc's frontend lowering of an inner `lambda` /
nested `def` that references an outer function's local.
**Surfaced:** June 2026, while writing a `captured_local.py`
test as part of the v2 LLVM closure-handler coverage push (see
[ifa/codegen/CG_IR_PARITY_PLAN.md](../ifa/codegen/CG_IR_PARITY_PLAN.md)
recommendation 3 follow-up).

## Symptom

This Python program crashes pyc with an assertion failure:

```python
def make_adder(n):
  return lambda x: x + n

f = make_adder(3)
print(f(4))
```

```
pyc: analysis/fa.cc:154: AVar *unique_AVar(Var *, EntrySet *):
  Assertion `es' failed.
Aborted (core dumped)
```

The pattern: an inner function (lambda or `def`) references a
name bound in its enclosing function's scope.  Both backends
crash; the failure is in the frontend pipeline, before any
code generation runs.

## What we know

`unique_AVar` is called with a `Var *` and an `EntrySet *`. The
assertion checks `es != nullptr`. So somewhere upstream a call
site is passing `nullptr` as the `EntrySet`.

A closure that captures `n` from an enclosing scope creates an
IF1 closure node whose `has[]` includes a slot for `n`.  Pyc's
gen_class_pyda / gen_fun_pyda paths build closure structs but
only for class methods (`a.method`) — the enclosing-function
capture pattern lacks a corresponding closure-struct
construction in the lowering.  The FA then encounters a closure
reference whose `EntrySet` was never established for the
capture context, and hits the assert.

Code pointers:
- `ifa/analysis/fa.cc:153` — assertion site.
- `ifa/codegen/codegen_common.cc:92` — `is_closure_var` predicate
  (this is the closure-shape sniff the FA uses).
- `python_ifa_build_syms.cc:537` — `gen_class_pyda` (handles
  class-method closures only).
- `python_ifa_build_if1.cc` — IF1 emission for `lambda` /
  function defs; no captured-local lowering exists.

## What pyc handles today (closure scope)

| Form | Works? |
|---|---|
| Top-level `g = lambda x: ...` (no capture) | yes — `lambda_basic.py` |
| Class attribute `class A: m = lambda y: ...` | yes — `lambda_class_attr.py` |
| Bound method `z = a.m`, call `z()` | yes — `lambda_closure.py` |
| Bound method (two methods, same instance) | yes — `multimethod_closure.py` |
| Bound method called from a loop | yes — `closure_in_loop.py` |
| **Inner lambda capturing enclosing local** | **no — this issue** |
| Closure passed across function boundary | no — [002](002-fa-crash-escaped-closure.md) |

## Proposed fix

Two pieces are needed:

1. **Frontend lowering** in `python_ifa_build_syms.cc` /
   `python_ifa_build_if1.cc`: detect inner-function references
   to enclosing locals, materialize a closure type analogous to
   `gen_class_pyda`'s class-method closure (selector at e0,
   captured locals at e1..eN), and rewrite the inner function's
   formals to take the closure pointer + its own real args.

2. **FA hardening**: even after (1), the `unique_AVar`
   assertion should be replaced by a graceful `fail()` with a
   source-location-aware message so future lowering bugs land
   as compile errors instead of segfaults.  The current assert
   gives no hint about what user-code triggered it.

The v2 LLVM closure handler (see commit `db4270a`) already
unpacks captured locals from e2..eN — the dispatch side is
ready.  Only the construction side (the IF1 build) is missing.

## Verification plan

1. Reproduce: `./pyc -D. /tmp/repro.py` with the Python program
   above.  Confirms current crash.
2. Land the fix; rerun: should print `7`.
3. Add `tests/captured_local.py` (currently dropped — see
   commit `b24bfbb`) with `.exec.check` containing `7\n14\n103\n`:

   ```python
   def make_adder(n):
     return lambda x: x + n
   f = make_adder(3)
   g = make_adder(10)
   print(f(4)); print(g(4)); print(f(100))
   ```

4. Verify both backends pass:
   - `./test_pyc -k captured_local`
   - `IFA_LLVM_V2=1 PYC_FLAGS=-b ./test_pyc -k captured_local`
5. Confirm v2's existing closure handler (cg_normalize_v2.cc
   `lower_send_period` / `lower_send_call`) needs no change —
   it already reads any closure-field index.

## What this unblocks

- The most common Python closure pattern (`functools.partial`-
  style adapters, factory functions returning configured
  callables, decorators that wrap with captured state).
- `tests/captured_local.py` (would re-add the case dropped in
  commit `b24bfbb`).
- Realistic stdlib porting — even `functools` shims need this.
