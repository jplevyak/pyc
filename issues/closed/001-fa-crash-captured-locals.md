# Issue 001: FA assertion crash on closures capturing enclosing-scope locals

**Status:** closed (2026-07-05). The closure-carrier-class design
below is implemented for both capturing lambdas and capturing
nested `def`s, and every residual this issue accumulated is now
resolved by issues/007's split-identity rework (commits through
2026-07-05):
- The nested-def cosmetic FA warning at call sites is GONE (the
  name-rebind no longer rewrites a Sym carrying `if1_closure`);
  `tests/nested_capture.py` (this issue's own `make_adder` repro)
  is now an official, warning-free test on both backends.
- The self-capture regression (nested recursive def) was fixed
  earlier via the `outer->sym == ast->sym` exclusion and stays
  fixed under split identity (extended to the public-name Sym).
- The mixed shape (nested recursive def that ALSO captures an
  enclosing local) now works — the in-body self-reference resolves
  to the carrier instance (`self`); covered in
  `tests/nested_recursion.py`.
- Transitive captures (a name captured from a grandparent scope
  through an intermediate function) work — needed by
  parameterized decorators; see `tests/decorator_basic.py`.
Original account follows.
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
fixed. `ifa/issues/closed/029-polymorphic-dispatch.md` /
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
| Closure passed across function boundary | no — [002](002-fa-crash-escaped-closure.md), likely separate mechanism (since fixed and closed) |

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

## What landed

The design above shipped essentially as sketched, with the capture-
detection and closure-carrier-class synthesis folded into
`build_syms_pyda` and the closure-creation-site codegen folded into
`build_if1_pyda`, rather than as new standalone passes:

- **`PycAST::closure_cls`** (`python_ifa.h`): a new field on the AST
  node for a `PY_lambda`/`PY_funcdef`, null for the common
  non-capturing case, pointing at the synthesized closure-carrier
  `Sym` otherwise. Set once in `build_syms_pyda`, read back twice in
  `build_if1_pyda` (once before the body is walked, to set up
  `fn->self`; once after, to build the instance and rebind the
  result).
- **`maybe_synthesize_closure_pyda`** (new static function in
  `python_ifa_build_syms.cc`, called from both `PY_lambda`'s and
  non-method `PY_funcdef`'s `build_syms_pyda` case, right before
  `exit_scope`): scans the just-finished scope's symbol map for
  names marked `NONLOCAL_USE` (pyc's existing free-variable
  sentinel), resolves each through `find_PycSymbol`, and — after
  filtering out three categories of false positive found during
  implementation (see below) — builds a `Type_RECORD` `Sym` named
  `"__closure__"` with one field per genuinely-captured name,
  stashing it on `ast->closure_cls` and on the scope's `in` (so the
  existing `PY_name` self.field rewrite treats captured-name
  references as ordinary instance-attribute reads, no new rewriting
  logic needed).
- **`build_closure_instance_pyda`** (new static helper in
  `python_ifa_build_if1.cc`, shared by both `PY_lambda` and
  `PY_funcdef`): emits the same `sym_new` + per-field `sym_setter`
  sequence `gen_class_pyda` uses for `__init__`, snapshotting each
  captured name's current value into the new instance.
- **`gen_fun_pyda`/`gen_lambda_pyda`** (`python_ifa_build_syms.cc`):
  when `ast->closure_cls` is set, thread the (already-created)
  `fn->self` as the function's first formal instead of a fresh
  dispatch-placeholder Sym — mirroring exactly how `gen_class_pyda`
  threads a method's `self`.

### Four bugs found and fixed during implementation

1. **Ordering: `fn->self` must exist before the body is walked, not
   after.** Originally set inside `gen_lambda_pyda`/`gen_fun_pyda`,
   which run *after* the body's `build_if1_pyda` recursion — too
   late for `PY_name`'s self.field rewrite (which needs
   `ctx.fun()->self` non-null at the point a captured-name reference
   is processed). Symptom: `if1_send`'s own `assert(v)` firing
   instead of the original crash. Fixed by moving the `fn->self`
   creation into `PY_lambda`'s/`PY_funcdef`'s `build_if1_pyda` case,
   immediately after `reenter_scope_pyda` and before any argument or
   body processing.
2. **`new_sym(ast, 1)` (anonymous overload) instead of
   `new_sym(ast, "__closure__", 1)` (named overload)** left
   `cls->name` null, crashing the C backend only, with
   `std::logic_error: basic_string: construction from null is not
   valid` — the LLVM backend tolerated it, which briefly hid the bug
   until testing two independent `make_adder` calls on `-b` off.
3. **False-positive capture via `PY_attribute`'s generic child
   recursion.** `build_syms_pyda`'s `PY_attribute` case falls through
   to a generic recursive case that also walks a trailer's
   *attribute-name* child (e.g. the `i` in `y.i`, the `append` in
   `L.append(a)`) through ordinary `PYC_USE` scope resolution — so
   `class A: i = 3; x = lambda y: y.i` (`lambda_closure.py`) and
   `L.append(a)` inside a lambda default-arg body (`default_args.py`)
   both spuriously marked `i`/`append` as `NONLOCAL_USE`. Two
   sub-cases needed two separate filters in
   `maybe_synthesize_closure_pyda`:
   - `y.i`: `i` resolves to a binding inside a class-body scope.
     Filtered by checking whether the scope at the resolved level has
     `in` set at all (a class-body scope always sets `in`, for
     *every* class, regardless of the class's ultimate `type_kind` —
     see next bug for why checking `type_kind == Type_RECORD`
     specifically was wrong).
   - `L.append(a)`: `append` resolves via the *import scope*
     fallback (`level < 0`) to `pyc_symbols.h`'s compiler-internal
     `sym_append` dispatch placeholder, not a real capture. Filtered
     with `if (level < 0) continue;`.
4. **Regression when extending capture detection from lambda-only to
   nested `def` too: 20 unrelated tests failed** (`list_comprehension.py`,
   `fstring_basic.py`, etc. — none with user-defined nested
   functions). Root cause: `__pyc__/*.py`'s own builtin-class dunder
   methods (`__str__`, `__add__`, `__eq__`, `__setitem__`, `__len__`,
   ...) were being spuriously flagged as "capturing," because the
   class-body exclusion from bug 3 checked
   `ctx.scope_stack[level]->in->type_kind == Type_RECORD`
   specifically — but `int`/`float`/`bool`/`list`/`tuple`/`str` get
   *other* `type_kind`s (`Type_SUM`, `Type_PRIMITIVE`, ...) via their
   own special registration (issue 022's finding), even though their
   method bodies are ordinary class-body scopes for this purpose.
   Fixed by simplifying the check to just `ctx.scope_stack[level]->in`
   (non-null at all), since `enter_scope` only ever sets `in` for a
   real `PY_classdef`, independent of the class's ultimate
   `type_kind`. This fully resolved the regression.

### Known remaining limitation: nested-def name-rebind warning

A named `def adder(x): ...` statement (unlike an inline `lambda`)
binds its name via a scope-bound Sym established during the earlier
build_syms pass; other references to that name (e.g. a later `return
adder`) resolve through that same Sym directly rather than re-reading
the defining node's `rval`. `if1_closure` had already attached the
raw closure body to that Sym, so an explicit
`if1_move(if1, &ast->code, inst, ast->sym, ast)` was needed to rebind
it to the closure instance (mirroring plain `x = y` reassignment)
— this fixed a hard crash (`illegal call argument type 'f' illegal:
adder` at the `return` site). Execution is correct after this fix on
both backends, but the **same warning still appears at the point the
closure is actually called** (confirmed via isolation that it
disappears if the closure is constructed but never called).
Hypothesis (not confirmed via deep FA tracing): `ast->sym` carries
both a static `if1_closure` attachment and a later dataflow-tracked
`if1_move` reassignment that FA considers simultaneously live. A full
fix likely needs the raw closure body to get its own internal Sym
identity separate from the publicly-named Sym from the start, which
is awkward to sequence (captures are only known after walking the
function body, but Sym identity/`ret`/`cont` must be established
before the body can be processed). Given execution correctness was
already achieved and the deeper fix looked more invasive than
initially hoped, this was left as a documented, accepted limitation
rather than chased further. **Because of this, `tests/captured_local.py`
(the official regression test) exercises only the lambda case**,
which is 100% clean with zero FA warnings on both backends; the
nested-def case was verified working via ad hoc scratch testing
(correct output, both backends) but is not yet an official test —
adding one would require the harness to tolerate a known-harmless
stderr warning, which it currently doesn't (`.exec.check` comparison
requires empty stderr).

**2026-07 update:** confirmed this is very likely the same
underlying mechanism as
[issue 007](../007-decorators-not-applied.md)'s "Finding 2"
(self-referential reassignment of a Sym that already has
`if1_closure` attached directly to it breaks FA's dispatch) —
see issue 007's re-check notes for a controlled experiment isolating
the exact trigger condition and three distinct severities (harmless
warning here vs. a wall of compile-time violations vs. a runtime
`matching function not found` crash, depending on how polymorphic
the reassigned value's type is). The fix sketched there (give every
non-method `def`'d function its own internal Sym identity from the
start, alias the public name to it, mirroring the class-method
pattern already at `python_ifa_build_syms.cc`'s `PY_funcdef`
`is_method` branch) would likely also close this gap — but it's a
substantial, higher-risk change with an open question about
recursive/mutually-recursive self-calls, not something to piggyback
onto this issue's own fix. Tracked as a shared follow-on in issue
007, not duplicated here.

**2026-07 follow-up regression found while investigating issue 007:**
a *nested* recursive function (`def outer(): def fact(n): ... return
n * fact(n - 1) ...`) is now spuriously treated as "capturing itself"
by `maybe_synthesize_closure_pyda`, since a function's own name,
referenced recursively from inside its own body, is indistinguishable
(from that scan's perspective) from a genuine free-variable capture
found in an intermediate enclosing scope. Confirmed via a scratch
test: execution is still correct (`fact(5)` prints `120`), but FA now
emits spurious warnings it didn't before this fix landed — the exact
same self-referential-reassignment shape as issue 007's Finding 2,
just reached via a different route (the closure-instance construction
rebinding `ast->sym`, not a decorator). Plain top-level (module-scope)
recursive functions are unaffected (`fibonacci.py` stays clean) because
module-level names resolve via `GLOBAL_CONTOUR`, never through the
capture-detection path at all. Likely fix: exclude a capture candidate
whose resolved Sym is the function's own name (`outer->sym ==
ast->sym` in `maybe_synthesize_closure_pyda`) — a function recursing
on itself doesn't need to be threaded through as a captured field, it
should keep resolving through the ordinary (nesting_depth/display)
path, which already handles this case correctly since a direct
recursive call is always stack-disciplined (never escapes).
**FIXED (2026-07-04):** implemented exactly that exclusion
(`if (outer->sym == ast->sym) continue;` in
`maybe_synthesize_closure_pyda`), verified clean (zero warnings,
correct output, both backends), and promoted to an official test —
`tests/nested_recursion.py` (self-recursive nested def; nested def
called twice with independent arguments). One adjacent shape
remains broken, pre-existing on both sides of this fix (confirmed
by A/B against the pre-fix build): a nested recursive def that
ALSO genuinely captures an enclosing local (`def count(n): ...
base ... count(n - 1)`) — the self-reference then resolves through
the carrier-class rebind and fails to compile with issue 007
Finding 2's `illegal call argument type` violations. That shape
belongs to issue 007's Sym-identity rework, noted in the test's
header comment.

### Verification results

- `./test_pyc`: 122 passed, 0 failed (C backend).
- `PYC_FLAGS="-b" ./test_pyc`: 122 passed, 0 failed (LLVM backend).
- `ifa/ifa-test`: 14 passed, 0 failed.
- `tests/captured_local.py` (two independent `make_adder` calls,
  each retaining its own captured value, plus a repeat call on the
  first instance) passes cleanly on both backends with zero stderr
  output.
- Nested-def capture verified via ad hoc scratch test: correct
  output on both backends, with the known cosmetic warning described
  above.
- Issue 002's repro was not re-tested this round (see "Relationship
  to issue 002" below — it uses a structurally different mechanism,
  bound methods rather than lexical capture, so there was no reason
  to expect this fix to touch it).

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
[../../ifa/issues/closed/029-polymorphic-dispatch.md](../../ifa/issues/closed/029-polymorphic-dispatch.md)
and
[../ifa/issues/030-polymorphic-dispatch-fat-pointers.md](../../ifa/issues/030-polymorphic-dispatch-fat-pointers.md)
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

## Verification plan (done)

1. ~~Reproduce: `./pyc /tmp/repro.py` with the Python program above.
   Confirms current crash.~~ Done — confirmed the `unique_AVar`
   assertion before the fix.
2. ~~Land the fix; rerun: should print `7`.~~ Done.
3. ~~Add `tests/captured_local.py` ... with `.exec.check` containing
   `7\n14\n103\n`~~ Done — `f`/`g` from *different* calls to
   `make_adder` with *different* `n` values, each retaining its own
   captured value independently, plus a repeat call on `f` — the core
   test that the capture is a real per-instance value, not aliased.
4. Issue 002's repro was **not** re-tested — on reflection (see
   "Relationship to issue 002" below) it exercises a structurally
   different mechanism (bound methods, not lexical capture), so there
   was no specific reason to expect this fix to affect it. Left as a
   separate open question for issue 002 itself, not re-opened here.
5. ~~Verify both backends pass~~ Done — `./test_pyc` and
   `PYC_FLAGS=-b ./test_pyc`: 122 passed, 0 failed, both backends.
6. ~~A nested `def` (not just `lambda`) capturing an enclosing local
   should work identically~~ Works, with one caveat: correct
   execution on both backends, but a known cosmetic FA warning
   remains at call sites (not present for lambda captures) — see
   "Known remaining limitation" in "What landed" above. Not added as
   an official test for this reason (the harness requires empty
   stderr); verified via ad hoc scratch testing instead.

## What this unblocks

- The most common Python closure pattern (`functools.partial`-
  style adapters, factory functions returning configured
  callables, decorators that wrap with captured state) —
  including, notably, [issue 007](../007-decorators-not-applied.md)'s
  most common decorator shape (`def double(f): def wrapper(x): ...
  return wrapper`), which was confirmed blocked directly on this
  issue's assertion during that investigation. (Note: 007's own
  shape is a *nested def*, so it will hit the known cosmetic warning
  above, not the fully-clean lambda path — worth checking when 007
  is picked up.)
- `tests/captured_local.py` (re-added the case dropped in commit
  `b24bfbb`).
- Possibly (unconfirmed) [issue 002](002-fa-crash-escaped-closure.md)
  — worth re-testing at some point, but likely a separate mechanism;
  see "Relationship to issue 002" above.
- Realistic stdlib porting — even `functools` shims need this.
