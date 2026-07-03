# Issue 001: FA assertion crash on closures capturing enclosing-scope locals

**Status:** open — root cause fully confirmed at the mechanism
level (not just symptom), and a concrete fix design identified that
reuses existing, proven pyc machinery. Not yet implemented — this
is a real feature (synthesizing closure-carrier classes), comparable
in scope to what issue 007's investigation revealed, not a small
patch. See "Root cause (confirmed, mechanism-level)" and "Proposed
fix: reuse the bound-method pattern" below.
**Affects:** `ifa/analysis/fa.cc` (`make_AVar`, `unique_AVar`,
`edge_nest_compatible_with_entry_set`); `ifa/if1/if1.cc`
(`if1_fixup_nesting`, called from `if1_closure`) — this is core
`ifa` machinery, not a pyc-frontend bug; pyc's frontend
(`python_ifa_build_syms.cc`'s `gen_lambda_pyda`/`gen_fun_pyda`,
`python_ifa_build_if1.cc`'s `PY_lambda`/nested `PY_funcdef` handling)
is missing a piece of lowering that `gen_class_pyda` already has for
a structurally-identical problem (see below).
**Surfaced:** June 2026, while writing a `captured_local.py`
test as part of the v2 LLVM closure-handler coverage push (see
[ifa/codegen/CG_IR_PARITY_PLAN.md](../ifa/codegen/CG_IR_PARITY_PLAN.md)
recommendation 3 follow-up).
**Related:** [002-fa-crash-escaped-closure.md](002-fa-crash-escaped-closure.md)
looked at first glance like the same root cause, but is more likely
a separate bug in a different mechanism — see "Relationship to
issue 002" below; worth re-checking once this lands, not assuming
fixed. `ifa/issues/029-polymorphic-dispatch.md` /
`ifa/issues/030-polymorphic-dispatch-fat-pointers.md` are relevant
to harder cases of the fix (a variable that can hold *different*
closure shapes depending on control flow) — see "Relationship to
polymorphic dispatch" below.

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
name bound in its enclosing function's scope. Both backends
crash; the failure is in the frontend pipeline, before any
code generation runs.

## Root cause (confirmed, mechanism-level)

This is **not** "closures are unimplemented in pyc's frontend" in
the sense of a missing case statement — it's a fundamental mismatch
between two different closure models: `ifa`'s existing nested-
function support (built for its own `V` language) assumes
**stack-disciplined nesting** (a nested function can only ever be
called while its lexically enclosing function's activation is still
live on the call stack — classic Pascal/Algol nested procedures),
while Python's lambdas/nested-defs are **first-class values that
escape** their defining function's activation entirely. `ifa` faithfully
implements the stack-disciplined model; pyc's frontend just routes
Python closures through it unchanged, which only works by accident
for the cases that happen not to escape.

Traced end to end:

1. **Every ordinary local variable** gets a placeholder
   `nesting_depth = LOCALLY_NESTED` (`-1`, `ifa/if1/sym.h:48`) when
   pyc's `build_syms_pyda` finishes scanning a function's own scope
   (`python_ifa_build_syms.cc:263-267`, `529-534`, mirrored at the
   `PY_classdef` site too).
2. When a function's body is finalized via `if1_closure`
   (`ifa/if1/if1.cc:614,640` → `if1_fixup_nesting` →
   `if1_fixup_nesting_internal`, lines 567-587), every lval/rval/
   formal still carrying the `LOCALLY_NESTED` placeholder gets
   **promoted** to `f->nesting_depth + 1`, where `f` is the function
   whose closure is being built and `f->nesting_depth` is that
   function's own lexical depth (set once, unconditionally, for
   every function including top-level ones, in
   `python_ifa_build_syms.cc:103` — `fn->nesting_depth =
   ctx.scope_stack.n - 1`). This is a general convention, not
   Python-specific: a variable declared in function `f` always ends
   up tagged `f_depth + 1`, so that a *direct* reference from within
   `f`'s own body matches without needing any lookup (see next
   point).
3. At FA time, every variable reference goes through `make_AVar`
   (`ifa/analysis/fa.cc:206-214`), called for every PNode's lvals/
   rvals/tvals (`ifa/analysis/fa.cc:1107` and dozens of other sites)
   — this is not a niche path, it's the universal variable-resolution
   entry point:
   ```cpp
   AVar *make_AVar(Var *v, EntrySet *es) {
     if (v->sym->nesting_depth) {
       if (v->sym->nesting_depth != es->fun->sym->nesting_depth + 1)
         return unique_AVar(v, es->display[v->sym->nesting_depth - 1]);
       return unique_AVar(v, es);
     }
     ...
   }
   ```
   For a reference to `n` **from within `make_adder` itself**,
   `es->fun` is `make_adder`, so `v`'s tag (`make_adder_depth + 1`)
   equals `es->fun->sym->nesting_depth + 1` — the fast, no-lookup
   path (`unique_AVar(v, es)`). For a reference to `n` **from
   within the lambda**, `es->fun` is the lambda (one level deeper),
   so the tags don't match, and resolution falls through to
   `es->display[v->sym->nesting_depth - 1]` — **the display array**.
4. `EntrySet::display` (`ifa/analysis/fa.h:114`) is a `Vec<EntrySet
   *>` populated by `update_display` (`ifa/analysis/fa.cc:838-850`)
   **only when an `AEdge` (an actual call edge) is created** from a
   caller's EntrySet to a new one, and only extended for functions
   with a nonzero `nesting_depth`:
   ```cpp
   static void update_display(AEdge *e, EntrySet *es) {
     for (int i = es->display.n; i < es->fun->sym->nesting_depth; i++)
       if (i < e->from->display.n) es->display.add(e->from->display[i]);
       else es->display.add(e->from);
     ...
   }
   ```
   This is exactly the classic "display" algorithm for statically
   nested procedures: it assumes the *caller* (`e->from`) is either
   the lexically enclosing activation itself or something already
   correctly nested within it, and builds the new display by copying
   the caller's display and appending the caller. There's a
   companion consistency check,
   `edge_nest_compatible_with_entry_set` (`ifa/analysis/fa.cc:680-689`),
   that verifies a call edge's displays share a common prefix —
   i.e. `ifa` actively checks that calls respect lexical nesting.

   For `f = make_adder(3); f(4)`, the call edge for `f(4)` runs
   `module_top_level → lambda`, **not** `make_adder → lambda` — by
   the time `f(4)` executes, `make_adder`'s activation is long gone,
   and the module top-level has no lexical relationship to
   `make_adder`'s scope at all. `update_display` has no correct
   entry to populate `es->display[make_adder_depth]` with, so the
   lambda's `EntrySet::display` never gets the slot `make_AVar`
   later asks for at nesting_depth `v->sym->nesting_depth - 1`. The
   `es` passed to `unique_AVar` ends up null (an out-of-range /
   never-populated `Vec` index), tripping `assert(es)`.

This is the **only** thing that has ever changed between the working
cases in the table below and this crash: whether the call edge that
reaches the closure's body originates from something lexically
compatible with a live display chain. `f(4)`, called long after
`make_adder` returned, from a scope with no lexical relationship to
it, structurally cannot satisfy that.

## What pyc handles today (closure scope)

| Form | Works? |
|---|---|
| Top-level `g = lambda x: ...` (no capture) | yes — `lambda_basic.py` |
| Class attribute `class A: m = lambda y: ...` | yes — `lambda_class_attr.py` |
| Bound method `z = a.m`, call `z()` | yes — `lambda_closure.py` |
| Bound method (two methods, same instance) | yes — `multimethod_closure.py` |
| Bound method called from a loop | yes — `closure_in_loop.py` |
| **Inner lambda capturing enclosing local** | **no — this issue** |
| Closure passed across function boundary | no — [002](002-fa-crash-escaped-closure.md), likely separate mechanism |

The pattern in the "works" rows is telling: every one of them
carries its captured state **explicitly**, as a real value (`self`,
bound at method-lookup time), not implicitly via lexical nesting
depth. None of them touch the `nesting_depth`/`display` machinery at
all — see next section.

## Proposed fix: reuse the bound-method pattern

`gen_class_pyda` (`python_ifa_build_syms.cc:608-737`) already solves
a **structurally identical** problem for class methods, and it does
so by sidestepping `nesting_depth`/`display` entirely:

- A method's `self` is threaded as an ordinary, explicit formal
  argument (`fn->self`, added via `as.add(fn->self)` before
  `if1_closure`), not resolved through lexical nesting. Reads/writes
  of instance state go through explicit `sym_operator`/`sym_period`/
  `sym_setter` sends on `self` (e.g. lines 617-627), which is just
  ordinary message dispatch — no display lookup involved anywhere.
- Because `self` is a real, heap-allocated value (`sym_clone`/
  `sym_new` against a prototype, same file lines 683-693) that
  travels with the bound method (`z = a.m` works, called later, from
  anywhere — `lambda_closure.py`/`closure_in_loop.py` prove this),
  it has no problem escaping its creation context. There's nothing
  for it to escape *from* — it doesn't depend on any caller's stack
  frame.
- Classes can already define `__call__` and get dispatched as
  ordinary callables when invoked (`gen_class_pyda`, the
  `call_fun`/`sym___call__` block near the end of the function) —
  this is the exact remaining piece needed to make a closure
  *instance* directly callable as `f(4)`.

The fix: give lambdas and nested `def`s the same treatment as class
methods, instead of relying on `ifa`'s native (stack-disciplined)
nested-function support at all.

1. **Detect captures.** During `build_syms_pyda`'s existing
   end-of-function-scope scan (the same loop that currently marks
   unclaimed names `LOCALLY_NESTED`,
   `python_ifa_build_syms.cc:263-267`), a name referenced by a
   nested lambda/`def` but *not* assigned within it resolves (via
   the normal scope-stack walk) to a binding in an enclosing
   function's scope. Collect that set per lambda/nested-def — this
   is ordinary free-variable analysis, and the scope-stack machinery
   already in `find_PycSymbol` has everything needed to tell "found
   in the immediately enclosing function's scope" apart from
   "found in module/global scope" (which doesn't need capturing at
   all — module globals are already handled as real global Syms and
   never go through the display path, per `make_AVar`'s
   `GLOBAL_CONTOUR` fallback).
2. **Synthesize a closure-carrier class**, one per capturing
   lambda/nested-def, mirroring `gen_class_pyda`'s record-class
   construction: a fresh `Type_RECORD` `Sym` with one field per
   captured name.
3. **At the closure-creation site** (where the `lambda`/nested `def`
   expression is evaluated, i.e. where `make_adder`'s body says
   `return lambda x: x + n`), emit the same
   `sym_new`/`sym_clone` + per-field `sym_setter` sequence
   `gen_class_pyda` emits for `__init__` (lines 608-627), populating
   each field from the **current** value of the corresponding
   captured variable. This is a snapshot-at-creation-time capture —
   correct for Python's normal (non-`nonlocal`) closure semantics,
   which is what this issue's own repro needs.
4. **Rewrite the lambda/nested-def's body** to be a method on the
   closure-carrier class (its own formal parameters unchanged, plus
   an implicit `self`), with every captured-name reference rewritten
   to a `self.captured_name` field read — ordinary message dispatch,
   exactly like an instance attribute access, no `nesting_depth`
   promotion or display lookup involved.
5. **Name the method `__call__`** so the existing class-instance
   call-dispatch path (already used for `@Wrapper`-style
   `__call__`-based decorators, and already present in
   `gen_class_pyda`) makes `f(4)` "just work" as an ordinary call on
   a callable instance — no new call-site machinery needed at all.

Everything past step 2 is a direct, mechanical re-application of
code `gen_class_pyda` already contains; there is no new invocation
mechanism to invent. What's new is *step 1* (free-variable detection
for lambdas/nested defs, which classes don't need since instance
attributes are already explicit `self.x` in the source) and *step
2/3's* synthesis of a class-shaped `Sym` from a lambda/def AST node
rather than from an actual `class` statement.

### What this deliberately does not attempt

**Mutable captures (`nonlocal`).** If the inner function *assigns*
to a captured name (requiring Python's `nonlocal` declaration), a
snapshot-at-creation-time field isn't enough — CPython uses a cell/
box indirection so all closures over the same binding observe each
other's writes. Out of scope here; the repro in this issue (and the
overwhelming majority of real closures — `functools.partial`-style
adapters, factory functions, decorators) only *reads* captured
state. Should be filed as its own follow-on if/when it's needed,
building on the field-based capture this fix establishes (a cell
would just be a captured field that's itself a mutable one-element
box, shared instead of copied).

## Relationship to issue 002

Initially assumed to be the same root cause ("any escape of a
closure past its creating activation"), but on closer reading of
002's own file, the mechanism is different enough that this should
**not** be assumed without separately verifying it: 002's repro uses
**bound methods** (`c.get`, a class instance's method reference),
which already thread `self` explicitly and don't touch
`nesting_depth`/`display` at all (bound methods are in the "works"
table above, for the *non-escaping* case) — and 002's failure is a
**silent segfault**, not the `unique_AVar` assertion this issue
hits. 002's own notes point at a *different* construction path
(`lower_send_period`'s ad hoc closure-struct creation for bound
methods, `python_ifa_build_if1.cc:585`) whose type apparently
doesn't propagate correctly across a call-edge boundary — plausibly
a real, independent bug in how a bound method's `bound_self` AType
is tracked through argument-passing, not a `nesting_depth`/`display`
problem at all. Both issues are "closures don't survive escaping
their creation context," but likely via two different mechanisms.
Worth re-testing 002's repro once this issue's fix lands (fixing
one might incidentally exercise code paths that expose or resolve
the other), but don't assume it's fixed without checking — and don't
let this issue's fix design get bent to try to also solve 002's
mechanism without first confirming they're actually the same bug.

## Relationship to polymorphic dispatch (issues 029/030)

For the simplest cases (a single call site whose closure has one
fixed concrete shape, like this issue's repro), no polymorphic
dispatch is needed at all — FA already specializes per call site,
so `f`'s type at `f(4)` is a single concrete closure-carrier class.
Harder cases — a variable that can hold *different* closure shapes
depending on control flow (e.g. two different `@decorator`-produced
wrappers reaching the same call site, or a list of differently-
captured closures) — would need real dispatch over a union of
concrete callable types, which is exactly what
[../ifa/issues/029-polymorphic-dispatch.md](../ifa/issues/029-polymorphic-dispatch.md)
and
[../ifa/issues/030-polymorphic-dispatch-fat-pointers.md](../ifa/issues/030-polymorphic-dispatch-fat-pointers.md)
are already tracking (029's struct-slot dispatch is reportedly
already implemented for a related shape). Land the simple case
first; treat "a name can hold structurally different closures" as
a natural follow-on that reuses 029/030's machinery rather than
inventing new dispatch logic.

## Proposed fix (superseded outline, kept for history)

The original two-piece sketch (frontend lowering + replace the raw
assert with a `fail()`) is subsumed by the design above; the
`unique_AVar`/`make_AVar` assertions should probably stay as hard
asserts once the frontend never produces an incompatible display
reference — but hardening them into a clean `fail()` remains cheap
insurance and is still worth doing independent of the main fix,
so a *future* lowering bug (or an as-yet-undiscovered escape shape)
fails loudly with a source location instead of a bare `Aborted`.

## Verification plan

1. Reproduce: `./pyc /tmp/repro.py` with the Python program above.
   Confirms current crash.
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

   `f`/`g` from *different* calls to `make_adder` with *different*
   `n` values, each retaining its own captured value independently
   — the core test that the capture is a real per-instance value,
   not aliased.
4. Verify [002](002-fa-crash-escaped-closure.md)'s repro also passes
   once this lands (see "Relationship to issue 002" above) — confirm
   before deciding whether it needs its own separate fix.
5. Verify both backends pass:
   - `./test_pyc -k captured_local`
   - `PYC_FLAGS=-b ./test_pyc -k captured_local`
6. A nested `def` (not just `lambda`) capturing an enclosing local
   should work identically — the fix should not be lambda-specific.

## What this unblocks

- The most common Python closure pattern (`functools.partial`-
  style adapters, factory functions returning configured
  callables, decorators that wrap with captured state) —
  including, notably, [issue 007](007-decorators-not-applied.md)'s
  most common decorator shape (`def double(f): def wrapper(x): ...
  return wrapper`), which was confirmed blocked directly on this
  issue's assertion during that investigation.
- `tests/captured_local.py` (would re-add the case dropped in
  commit `b24bfbb`).
- Possibly (unconfirmed) [issue 002](002-fa-crash-escaped-closure.md)
  — worth re-testing once this lands, but likely a separate
  mechanism; see "Relationship to issue 002" above.
- Realistic stdlib porting — even `functools` shims need this.
