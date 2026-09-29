# 169 — a method of a function-local class that reads the function's local crashes the analysis

**Status:** open (re-verified 2026-09-28: still the `unique_AVar: Assertion 'es'` abort). Found 2026-09-25 while testing the
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
