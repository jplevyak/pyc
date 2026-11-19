# 169 — a method of a function-local class that reads the function's local crashes the analysis

**Status:** open, narrowed 2026-10-14 to one residual: a local class
whose captured value has a DIFFERENT TYPE on different calls is refused
(see "2026-10-14" at the end). The crash is fixed, and so is a silent
wrong answer found on the way.

**Original status:** open (re-verified 2026-09-28: still the `unique_AVar: Assertion 'es'` abort). Found 2026-09-25 while testing the
[106](closed/106-empty-if-body-silently-accepted.md) parser fix: a program
exercising every compound statement nested in a method crashed here, and
the crash predates that fix.

Repro: `tests/local_class_reads_enclosing_local.py` (`.known_issue`).

## Symptom

```python
def inner(x):
    class I:
        def g(self):
            return x
    return I().g()
print(inner(1))
```

| | result |
|---|---|
| CPython | `1` |
| **pyc** | `analysis/fa.cc:160: AVar *unique_AVar(Var *, EntrySet *): Assertion 'es' failed.` (abort, both backends) |

The capture is what matters. The same local class with `g` returning a
constant compiles and prints `7`, and a local class whose body reads a
class attribute next to a function local (`return I.z + y`) prints `4`.

## Where it fails

```
#7  unique_AVar (v, es=0x0)            analysis/fa.cc:160
#8  make_AVar (v, es)                  analysis/fa.cc:220
#9  set_entry_set (e, es=0x0)          analysis/fa.cc:1491
#10 make_entry_set (e, ...)            analysis/fa.cc:1932
#11 analyze_edge (e)                   analysis/fa.cc:3418
```

`make_AVar` resolves a variable of an ENCLOSING function through the
entry set's lexical display:

```cpp
if (v->sym->nesting_depth != es->fun->sym->nesting_depth + 1)
  return unique_AVar(v, es->display[v->sym->nesting_depth - 1]);
```

For `g`, the display slot for `inner` is null: nothing connects `g`'s
contour to the contour of `inner` that created the class. A nested `def`
gets that connection from closure conversion (the closure-carrier classes
that fixed [001](closed/001-fa-crash-captured-locals.md)); a method of a
local class is reached through the class's method table, not as a closure,
so it apparently never gets it.

## Not yet decided

Whether the fix belongs in the frontend (closure-convert the class: its
methods' free variables become state the class carries, as for nested
`def`s) or in the analysis (derive the display from where the class was
created). The frontend is the first place to look, because that is how
nested functions already work, and `display` is otherwise being retired
as contour identity ([ifa/100](../ifa/issues/closed/100-FA-display-removed-from-contour-identity.md)).

Whatever the fix, a missing display slot should not be an assertion. At
minimum it should be a compile error naming the variable and the method.

## Verification plan

- `tests/local_class_reads_enclosing_local.py` prints `1`; delete its
  `.known_issue`.
- Two calls of `inner` with different argument types (`inner(1)`,
  `inner("a")`) print `1` and `a` — each class creation must see its own
  `x`.
- `make test` green.

## What this unblocks

Any program that defines a class inside a function and refers to the
function's locals from a method — a common Python idiom (local exception
classes, factory functions returning a configured class). Today it
crashes the compiler rather than failing with a diagnostic.

## 2026-10-14: closure conversion in the frontend

**Two root causes, not one.**

1. **Nesting depth by first use.** `if1_fixup_nesting` gives a
   `LOCALLY_NESTED` Sym the depth of the first closure whose code uses
   it, and closures are registered innermost first. A capturing lambda or
   nested def never reads its parent's locals directly (it goes through
   its carrier), but a local class's body and methods did, so the class
   claimed `inner`'s `x` at the class's own depth, and `inner`'s own
   formal then resolved through a display slot that did not exist: the
   assert. `if1_finalize_flatten_and_fixup_nesting` now runs the fixup
   outermost first (stable, by nesting depth), so a local is always
   claimed by its owner, the shallowest closure that can use it.
2. **The display is the wrong channel.** With (1) alone the crash goes
   and a miscompile appears: `make2(2), make2(3)` printed `0 0`. The
   class-body function's one contour, called from both of `make2`'s
   contours, has one display slot, and read one call's `x` for both.
   That is ifa/100's reason the display left contour identity.

**The fix** (`python_ifa_build_syms.cc`, `local_class_captures` and
`gen_class_pyda`; `pyc_read_captured` in `python_ifa_build_if1.cc`):
names a local class's body or methods read from an enclosing function
(NONLOCAL_USE resolving to a function scope, the filter
`maybe_synthesize_closure_pyda` uses) are passed, not looked up. The
class-body function takes one extra formal per name, the `class`
statement passes the values read where it runs, the body stores each on
the prototype under a hidden member (`__pyc_cap_<name>_<id>`, so it
cannot collide with a user attribute), every instance copies it when
cloned from the prototype, and a method reads `self.<member>`. A
carrier made inside a method (a lambda capturing the same name) reads it
the same way.

As with carriers, the value is copied when the class statement runs: a
later rebinding in the function is not seen, where CPython's shared cell
would show it.

`tests/local_class_reads_enclosing_local.py` passes (its `.known_issue`
is gone); `tests/local_class_capture.py` covers a method capture, a
class-body capture, a lambda in a method capturing transitively, two
objects from two calls each keeping its own values, and the `make2` case.
Both backends.

**Residual: a different type per call.** `inner(1)` then `inner("a")` is
refused (`{int64, str}`). The prototype of a class lives in ONE global
(`cls->self`), and every `I()` clones it, so the hidden member holds
every call's value, and no contour split separates the calls: the union
forms in a single global, flow-insensitively. Making it per call needs
the class object to be a per-call value that instantiation reads, which
is a change to how classes are represented, not to capture. Values of
one type across calls are correct.

`make test` green (463 on each backend). Corpus check -R
`check__default__d80ebc9e+ae99302a`: every column identical on all 77
(no corpus program has a local class reading its function's locals).
