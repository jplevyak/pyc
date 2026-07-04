# Issue 007: User-defined decorators are silently ignored

**Status:** open — investigated in depth; blocked on a chain of three
separate, deeper, pre-existing limitations, not a quick frontend fix
as originally scoped. The final and largest of the three
(`ifa/issues/closed/029-polymorphic-dispatch.md` /
`ifa/issues/030-polymorphic-dispatch-fat-pointers.md`'s fat-pointer
dispatch design, extended to a call-site shape those issues don't yet
cover) is a genuinely large feature, not something to improvise here
— see "2026-07 re-check: the real blocker is 029/030's dispatch gap,
extended to a new shape" below before attempting again.
**Affects:** `python_ifa_build_if1.cc:517-606` (`PY_decorated` case
in `build_if1_pyda`); the real fix, if attempted, also touches
`python_ifa_build_syms.cc` (symbol creation for decorated defs — see
below), not just `build_if1_pyda`; and, per the 2026-07 re-check,
`ifa/codegen/codegen_common.cc`'s `get_target_fun_core` /
`ifa/codegen/cg.cc`'s `emit_send_call` (and the LLVM-backend
equivalent) for the dispatch layer.
**Related:** `PYTHON_FRONTEND.md` §11 documents `@vector("s")` as
the one class decorator with real semantics; `@pyc_struct` (issue
`closed/015-pyc-pod-records-no-frontend-hook.md`) is the other.
No general decorator-application mechanism exists.
[001-fa-crash-captured-locals.md](001-fa-crash-captured-locals.md)
is one of the blockers found here — closure-wrapping decorators
(the most common decorator shape) hit it directly.
[ifa/issues/closed/029-polymorphic-dispatch.md](../ifa/issues/closed/029-polymorphic-dispatch.md)
/
[ifa/issues/030-polymorphic-dispatch-fat-pointers.md](../ifa/issues/030-polymorphic-dispatch-fat-pointers.md)
already track and design the general "call site resolves to ≥2
candidate `Fun`s" problem for method-call-through-receiver shapes;
this issue's Finding 2 is the same architectural gap for a shape
those issues don't cover yet (calling a bare function-typed variable
directly, no receiver object to dispatch through).

## Symptom

Any decorator other than the two builtin compiler directives
(`@vector("s")`, `@pyc_struct`) is evaluated but its result is
**never applied** — the decorated function/class behaves exactly
as if undecorated, with no error or warning.

```python
def double(f):
    def wrapper(x):
        return f(x) * 2
    return wrapper

@double
def add_one(x):
    return x + 1

print(add_one(5))
```

Expected (CPython): `12` (`(5+1)*2`).
Actual (pyc): `6` (`5+1` — the decorator never ran its wrapping
logic; `add_one` is used as if `@double` weren't there).

This affects the standard library idioms `@staticmethod`,
`@classmethod`, `@property`, `@functools.wraps`, and any
user-defined decorator — none of them do anything.

## Root cause (original, still accurate)

In `build_if1_pyda`'s `PY_decorated` case, the decorator expression
list is built (so referencing an undefined decorator name is at
least caught), but the resulting decorator *value* is never called
with the function/class as its argument, and the result of that
call is never substituted anywhere meaningful. For classes, only
two hardcoded decorator names get any special handling at all
(`strcmp(fname, "vector")`, `strcmp(fname, "pyc_struct")`);
everything else is a no-op by construction.

## Investigation: why this is bigger than it looks

A full attempt was built and tested (call-and-rebind machinery,
dotted-name + arglist resolution, bottom-up application order,
`@vector`/`@pyc_struct` preserved as a fast path) and then reverted
after testing against realistic decorator shapes. The mechanism
itself compiles cleanly and correctly handles the *degenerate*
identity-decorator case (`def noop(f): return f`), but every
realistic pattern fails, for two separate, deep reasons:

### Finding 1 — `ast->rval` reassignment is a no-op for anything but the AST node itself

The original proposed fix sketch (below) assumed "rebind `ast->rval`
to the decorator call's result" would be enough — modeled on how
CPython desugars `@d1 @d2 def f(): ...` to `f = d1(d2(f))`. This is
**not** enough in pyc's architecture: a name like `add_one` is bound
to a *fixed* Sym during the separate, earlier `build_syms_pyda` pass
(`make_PycSymbol(ctx, "add_one", PYC_LOCAL)`,
`python_ifa_build_syms.cc:202`), and *every other reference* to
`add_one` elsewhere in the program resolves through that scope
binding directly — never through this `PY_decorated` AST node's
`ast->rval`. Setting `ast->rval` here has no effect on any call site
other than the (nonexistent, since decorated defs are statements)
inline use of the decorated-def-as-an-expression.

The fix for *this* part: instead of just setting `ast->rval`,
overwrite the def's own scope-bound Sym (`def_ast->sym`, established
during `build_syms_pyda`, identical to `def_ast->rval`) with an
`if1_move`, mirroring exactly what a plain `add_one = double(add_one)`
reassignment statement does (see `PY_assign`'s plain-name-target
case in `build_if1_pyda`, `if1_move(if1, &ast->code, v->rval, a->sym)`)
— confirmed pyc already supports reassigning a def'd function name to
a *different* existing function via ordinary assignment
(`foo = bar` after `def foo` / `def bar` works correctly and prints
the right thing). This part of the fix is real and uncontroversial.

### Finding 2 — self-referential reassignment breaks FA dispatch, independent of decorators entirely

Decorator application is inherently self-referential:
`add_one = double(add_one)` reads the *old* value of `add_one` as an
argument to the call that produces the *new* value, in one
statement. This exact shape — with no decorator syntax involved at
all — was confirmed broken as a **separate, pre-existing, general
pyc limitation**:

```python
def add_one(x): return x + 1
def replacement(x): return x + 100
def get_it(f): return replacement
add_one = get_it(add_one)      # self-referential: reads then rewrites add_one
print(add_one(5))
```

fails at runtime with `assert(!"runtime error: matching function not
found")` — confirmed via the generated C: the call site
(`add_one(5)`) gets a **hardcoded, unconditional crash** baked in,
even though both candidate implementations (`replacement`, and the
original `add_one`) exist and compile individually as functions.
Introducing an explicit temp variable
(`_tmp = add_one; add_one = get_it(_tmp)`) *appeared* to fix it —
but that appearance was misleading: this fixture is so simple that
FA fully constant-folds the entire program to a single printed
literal in the temp-variable version, sidestepping real dispatch
resolution entirely rather than proving the mechanism sound. Once
the fixture is complex enough that FA can't fold it away (as any
real decorated-function call site will be), the temp-copy version
**fails identically** to the direct self-referential one.

This means: even after fixing Finding 1 (overwrite `def_ast->sym`
via `if1_move` instead of just `ast->rval`), any decorator that
returns something other than the exact same Sym it was given (i.e.
every decorator except a no-op identity function) hits this FA
dispatch bug. Confirmed across three realistic shapes, all broken:

1. **Closure-wrapping** (`def double(f): def wrapper(x): return
   f(x)*2; return wrapper` — the standard decorator shape, and this
   issue's own motivating example) — hits **issue 001**
   (`unique_AVar` assertion) directly, since `wrapper` is a nested
   function capturing `f` from the enclosing scope.
2. **Returns a different existing function**
   (`def swap(f): return replacement`) — hits the Finding 2 dispatch
   bug described above.
3. **Class-based decorator** (`@Wrapper` with `__init__`/`__call__`)
   — also broken, differently (attribute access through the stored
   function doesn't propagate correctly), likely a third symptom of
   the same underlying class of bug.

### A plausible fix shape exists, but requires touching the syms pass too

`python_ifa_build_syms.cc`'s `PY_funcdef` case (`is_method` branch,
currently around line 321-330) already has the right *pattern*, for
a different case (methods defined inside a class body): instead of
tying the closure body directly to the method's public-name Sym, it
creates a **separate internal Sym** for the actual closure
(`ast->sym = new_sym(ast, 1)`), makes the public-name Sym **alias**
to it (`ast->rval->alias = ast->sym`), and installs it via a
`setter` send into the class's `self` rather than direct
`if1_closure` attachment onto the public Sym. The `else` branch
(every *non-method* def — i.e. every module-level or nested
function) does not get this treatment: `ast->rval = ast->sym =
def_fun_pyda(n, ast, ps->sym, ctx)` ties `if1_closure` directly onto
`ps->sym`, the exact Sym every other reference to the name resolves
through.

**2026-07 re-check (post issue-001 fix), confirmed via direct
experiment:** the necessary and sufficient condition for Finding 2's
crash is not "the target was `def`'d" per se, but specifically
**reassigning a Sym that already has `if1_closure` attached directly
to it, using a call that reads that same Sym as an argument in the
same statement**:
- `other = get_it(add_one)` (distinct target, not self-referential):
  works, prints correctly.
- `add_one = get_it(0)` (self-named target, but RHS doesn't read
  `add_one`): works, prints correctly — confirms it's not merely
  "reassigning a Sym that already IS a function."
- `add_one = get_it(add_one)` (`def`-bound target, truly
  self-referential): reproduces Finding 2 exactly — a wall of
  compile-time `NOTYPE`/`SEND_ARGUMENT` violations (`'add_one' has
  no type`, `illegal call argument type`), because FA's variable
  resolution scheme (a fresh Var/SSU generation per assignment)
  looks like it should handle a plain reassignment fine — and does,
  for a plain `int` (`x = f(x)` for an `int`-valued `x` compiles and
  runs correctly, printing `6`) — but breaks specifically when the
  Sym being read-then-rewritten is one FA/if1 treats as *being* a
  function (`if1_closure` attached directly, not merely holding a
  reference to one).
- `g = lambda x: x + 1; g = get_it(g)` (lambda-bound target, self-
  referential): does **not** produce compile-time violations at all,
  but crashes at **runtime** with the same `assert(!"runtime error:
  matching function not found")` `get_target_fun`/polymorphic-
  dispatch failure documented in `ifa/CODEGEN_C.md §12` — a
  *different* visible symptom (silent-until-runtime vs. loud-at-
  compile-time) but plausibly the same underlying resolution gap,
  just caught at a different stage depending on how the Sym was
  first bound (`gen_lambda_pyda`'s `as.add(fn)` vs. `def_fun_pyda`'s
  direct `ast->sym = ps->sym` tie-in).

**This is very likely the identical mechanism** behind
[issue 001](001-fa-crash-captured-locals.md)'s own accepted
"known remaining limitation": a capturing nested `def`'s name-rebind
fix there (`if1_move(inst, ast->sym)`, rebinding a Sym that already
has `if1_closure` attached, into a freshly-built closure instance)
left one harmless-but-present FA warning at the call site precisely
because it's the same shape of self-owned-Sym reassignment — just
with a monomorphic result type (a single concrete closure-carrier
class), so FA's confusion manifests as a survivable warning rather
than the wall of violations or runtime crash seen here with a
genuinely polymorphic result (could be either of two unrelated
`Fun`s). One underlying bug, three severities depending on how
polymorphic the reassignment's result type is.

Applying the class-method alias pattern to *all* non-method
`PY_funcdef`s (not just decorated ones) — give the raw closure its
own internal Sym identity from the start via `def_fun_pyda`, never
call `if1_closure` directly on the name everyone else resolves to,
and bind the public name's Sym via an ordinary `if1_move` from that
internal Sym (mirroring how any other `name = value` assignment
already works) — would very plausibly close this gap for module-level
functions generally, not just decorators, and would likely also
clean up issue 001's remaining cosmetic warning as a side effect.
This is a **larger, higher-risk change than it looks**: it touches
Sym-identity creation for literally every function definition in
every pyc program (not just decorated or reassigned ones), including
recursive and mutually-recursive functions, where a bare-name
self-call inside the function's own body currently resolves through
the same Sym `if1_closure` is attached to — under the alias scheme,
that self-call would need to resolve through the *public* alias Sym
instead, and it's not yet confirmed whether that Sym reliably holds
the right value/type at the point the function's own body is
compiled (methods sidestep this by always calling themselves via
`self.name(...)`, never a bare name, so this ordering question has
never actually been exercised by the existing alias-pattern code).
Not attempted here — flagging as the concrete starting point for a
future attempt, since finding it took real investigation that
shouldn't need repeating, but the recursive/mutual-recursion
interaction needs to be worked out and tested before landing it
broadly.

## 2026-07 re-check: the real blocker is 029/030's dispatch gap, extended to a new shape

Re-confirmed both Finding 1 and Finding 2 still reproduce exactly as
described above, unaffected by issue 001's closure fix (which solved
a different problem — closures escaping their creation context, not
decorator application). Went looking for whether the alias-pattern
fix sketched above would be enough to make the motivating
`@double def add_one` example actually work, and found it would not
— it only shifts the failure to a different, still-unfixed layer.

**Controlled experiment isolating the exact trigger:**

| Case | Result |
|---|---|
| `other = get_it(add_one)` (distinct target, not self-referential) | works |
| `add_one = get_it(0)` (self-named target, RHS doesn't read `add_one`) | works |
| `x = f(x)` for a plain `int` (self-referential, non-function) | works |
| `add_one = get_it(add_one)` — `add_one` bound via `def` (has `if1_closure` attached directly) | Finding 2: wall of compile-time `NOTYPE`/`SEND_ARGUMENT` FA violations |
| `g = get_it(g)` — `g` bound via `g = lambda x: ...` (ordinary `if1_move`, no direct `if1_closure`) | compiles **clean**, crashes at **runtime**: `assert(!"runtime error: matching function not found")` — and crashes even a `print(g(5))` call written *before* the reassignment, since FA analyzes the whole program, not execution order |

The `def` case and the `lambda` case are **two different bugs, not
one**:

1. The `def` case's compile-time violation wall is specific to
   reassigning a Sym that has `if1_closure` attached *directly* to
   it — this is what the alias-pattern fix (previous section) targets,
   and it would very plausibly turn this case into the *lambda*
   case's failure mode (clean compile, runtime crash) — genuine
   progress, but not a fix, since the lambda case is `still broken`.
2. The lambda case's runtime crash is `ifa/issues/closed/029-polymorphic-dispatch.md`'s
   documented mechanism, confirmed by tracing it directly:
   `get_target_fun_core` (`ifa/codegen/codegen_common.cc:97`) returns
   null because the call site genuinely has ≥2 candidate `Fun`s
   (FA correctly determined `g` could be either the original lambda
   or `replacement`), and `cg.cc`'s `emit_send_call` falls through to
   the polymorphic-dispatch path (`cg.cc:769-822`) — but that path
   only knows how to dispatch a method call **through a receiver
   object**, looking up a vtable-like slot in the receiver's
   concrete-type struct (`csym->has[k]->name == method_name`,
   `cg.cc:801-802` — exactly what `tests/poly_dispatch_low.py` and
   issue 030's fat-pointer design already cover). Our case has **no
   receiver at all** — `g` itself is the polymorphic callable, called
   directly (`g(5)`), not `something.g()`. There is no object to read
   a classtag/slot from, so the dispatch path can't apply, and it
   falls through to the unconditional
   `assert(!"runtime error: matching function not found")`.

**This means issue 007's actual fix needs two layers, not one:**

1. The alias-pattern fix (previous section) — normalizes `def`-bound
   reassignment to behave like `lambda`-bound reassignment (fixes the
   asymmetry, likely also closes issue 001's cosmetic warning), but
   is *necessary, not sufficient*.
2. **Extending issue 029/030's fat-pointer/classtag dispatch design
   to cover calling a bare polymorphic function-typed value directly**
   (no receiver), not just a method call through a receiver instance.
   This means:
   - `get_target_fun_core`/`get_target_funs` needs a codegen path for
     "no receiver, dispatch on the callable value's own concrete type"
     — plausibly requiring first-class function values to carry the
     same kind of classtag/vtable-pointer materialization issue 030
     proposes for objects, even when they were never wrapped in a
     user-visible class at all (a `def`-bound or `lambda`-bound
     value is not today represented as a taggable/dispatchable
     record the way a class instance is).
   - Needs to land in both `cg.cc` (C backend) and the LLVM backend's
     equivalent emit path (030's own doc already notes this dual-
     backend requirement for its existing scope).
   - Issue 030's own design doc treats this as real, multi-step
     design work (backward-flow analysis from dispatch points, fat-
     pointer materialization, fan-out-adaptive emission) — even its
     *already-started* method-receiver case has an open, not-yet-fixed
     high-fan-out gap (`tests/poly_dispatch_high.py`'s FA-fixpoint
     issue). Extending it to cover bare callable values is additional
     scope on top of that, not a small addition.

Given this, issue 007 should not be picked up again as an isolated
decorator fix — it is now understood to require the same
architectural investment already tracked (and only partially done)
under issues 029/030, extended to one more call-site shape. Land
029/030's core fat-pointer mechanism first (for the method-receiver
case it already targets); revisit whether extending it to bare
callable values is worth doing generally (it would also unblock
polymorphic callback/strategy patterns beyond decorators) before
returning to this issue specifically.

## Proposed fix sketch (superseded — see "Investigation" above)

The original sketch below undersold the difficulty; kept for
context on what was tried first.

1. After `gen_fun_pyda`/`gen_class_pyda` produce `def_ast->rval`
   (the raw function/class Sym), for each decorator **in reverse
   order** (Python applies decorators bottom-up), emit an IF1
   `send` that calls the decorator expression with the current
   `rval` as its sole argument, and rebind `rval` to the send's
   result.
2. Keep the existing `@vector`/`@pyc_struct` special-casing as an
   early-exit fast path, falling through to the general
   call-and-rebind path for any decorator name that isn't one of
   those two.
3. Watch for the interaction with polymorphic dispatch (issues
   029/030) — confirmed relevant: this *is* a polymorphic-dispatch
   problem (Finding 2).

## Verification plan

1. The `double`/`add_one` repro above prints `12`.
2. Decorator stacking: two decorators compose in the correct
   (bottom-up) order.
3. The self-referential-reassignment fixture in Finding 2
   (`add_one = get_it(add_one)`, no decorator syntax) works
   correctly on a non-trivially-foldable program — this is the
   real blocking bug and should be the first thing re-verified
   before trusting any decorator fix built on top of it.
4. `@staticmethod` / `@classmethod` at minimum don't silently
   drop the `self`/`cls` binding semantics incorrectly (may need
   their own follow-on issue if full method-binding semantics are
   out of scope for the first pass — flag if so).
5. Existing `@vector("s")` and `@pyc_struct` tests
   (`tests/pyc_struct_basic.py`, etc.) continue to pass unchanged.
6. Add `tests/decorator_basic.py` + `.exec.check`.

## What this unblocks

Decorators are pervasive in idiomatic Python (`@property`,
`@staticmethod`, `@functools.lru_cache`, custom validation/
registration decorators). Silently dropping them means ported
code compiles and runs but behaves differently from CPython with
no diagnostic — a correctness trap.
