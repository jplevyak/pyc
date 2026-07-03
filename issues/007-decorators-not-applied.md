# Issue 007: User-defined decorators are silently ignored

**Status:** open — investigated in depth; blocked on two separate,
deeper, pre-existing FA/dispatch limitations, not a quick frontend
fix as originally scoped. See "Investigation: why this is bigger
than it looks" below before attempting again.
**Affects:** `python_ifa_build_if1.cc:517-606` (`PY_decorated` case
in `build_if1_pyda`); the real fix, if attempted, also touches
`python_ifa_build_syms.cc` (symbol creation for decorated defs — see
below), not just `build_if1_pyda`.
**Related:** `PYTHON_FRONTEND.md` §11 documents `@vector("s")` as
the one class decorator with real semantics; `@pyc_struct` (issue
`closed/015-pyc-pod-records-no-frontend-hook.md`) is the other.
No general decorator-application mechanism exists.
[001-fa-crash-captured-locals.md](001-fa-crash-captured-locals.md)
is one of the two blockers found here — closure-wrapping decorators
(the most common decorator shape) hit it directly.

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
