# Issue 007: User-defined decorators are silently ignored

**Status:** open.
**Affects:** `python_ifa_build_if1.cc:517-606` (`PY_decorated` case
in `build_if1_pyda`).
**Related:** `PYTHON_FRONTEND.md` §11 documents `@vector("s")` as
the one class decorator with real semantics; `@pyc_struct` (issue
`closed/015-pyc-pod-records-no-frontend-hook.md`) is the other.
No general decorator-application mechanism exists.

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

## Root cause

In `build_if1_pyda`'s `PY_decorated` case:

```cpp
case PY_decorated: {
  PyDAST *def = n->children.last();
  // Process decorators
  for (int i = 0; i < n->children.n - 1; i++) build_if1_pyda(n->children[i], ctx);
  if (def->kind == PY_funcdef) {
    ...
    gen_fun_pyda(def, def_ast, ctx);
    exit_scope(ctx);
    ast->rval = def_ast->rval;   // <-- the plain function, undecorated
    ast->code = def_ast->code;
  } else if (def->kind == PY_classdef) {
    // Scans decorators only for the literal names "vector" and
    // "pyc_struct"; anything else is inspected and discarded.
    ...
    gen_class_pyda(def, def_ast, ctx, vector_size);
    ast->rval = def_ast->rval;   // <-- the plain class, undecorated
  }
  return 0;
}
```

The decorator expression list is built (`build_if1_pyda(n->children[i], ...)`
at the top) so referencing an undefined decorator name is at
least caught, but the resulting decorator *value* is never
called with the function/class as its argument, and the result
of that call is never substituted for `ast->rval`. For classes,
only two hardcoded decorator names get any special handling at
all (`strcmp(fname, "vector")`, `strcmp(fname, "pyc_struct")`);
everything else is a no-op by construction.

## Proposed fix sketch

1. After `gen_fun_pyda`/`gen_class_pyda` produce `def_ast->rval`
   (the raw function/class Sym), for each decorator **in reverse
   order** (Python applies decorators bottom-up), emit an IF1
   `send` that calls the decorator expression with the current
   `rval` as its sole argument, and rebind `rval` to the send's
   result. This mirrors how CPython desugars
   `@d1 @d2 def f(): ...` to `f = d1(d2(f))`.
2. Keep the existing `@vector`/`@pyc_struct` special-casing as an
   early-exit fast path (they mutate the Sym in place rather than
   wrapping it, which is intentionally different from general
   decorator semantics), but fall through to the general
   call-and-rebind path for any decorator name that isn't one of
   those two.
3. Watch for the interaction with polymorphic dispatch (issues
   029/030) — a decorator that returns a closure changes the
   Sym's call-target shape, which needs to flow through FA
   normally since it's just an ordinary IF1 send.

## Verification plan

1. The `double`/`add_one` repro above prints `12`.
2. Decorator stacking: two decorators compose in the correct
   (bottom-up) order.
3. `@staticmethod` / `@classmethod` at minimum don't silently
   drop the `self`/`cls` binding semantics incorrectly (may need
   their own follow-on issue if full method-binding semantics are
   out of scope for the first pass — flag if so).
4. Existing `@vector("s")` and `@pyc_struct` tests
   (`tests/pyc_struct_basic.py`, etc.) continue to pass unchanged.
5. Add `tests/decorator_basic.py` + `.exec.check` — first
   general-decorator test in the suite (none exist today;
   confirmed via `grep -l '^@' tests/*.py` returning only
   `@pyc_struct` users).

## What this unblocks

Decorators are pervasive in idiomatic Python (`@property`,
`@staticmethod`, `@functools.lru_cache`, custom validation/
registration decorators). Silently dropping them means ported
code compiles and runs but behaves differently from CPython with
no diagnostic — a correctness trap.
