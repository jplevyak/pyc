# 169 — a function-local class: one residual, a captured value whose type differs per call

**Status:** open on the residual. The crash (`unique_AVar: Assertion 'es'`)
and the miscompile found on the way (`make2(2), make2(3)` printing `0 0`)
were fixed 2026-10-14 by closure-converting local classes in the frontend.
Rewritten 2026-11-03; the two root causes (nesting depth claimed by first
use; the display as the capture channel) and the mechanism are in git
(`git show 23cd789:issues/169-local-class-method-reading-enclosing-local-crashes.md`).

## What works

A local class's body and methods read enclosing locals through hidden
members (`__pyc_cap_<name>_<id>`) the class statement stores on the
prototype and every instance copies; a lambda in a method reads them the
same way. `tests/local_class_reads_enclosing_local.py` and
`tests/local_class_capture.py`, both backends. As with closure carriers,
the value is copied when the class statement runs, so a later rebinding
in the function is not seen where CPython's shared cell would show it.

## The residual

```python
def inner(x):
    class I:
        def g(self): return x
    return I().g()
print(inner(1)); print(inner("a"))   # CPython: 1, a.   pyc: refused, {int64, str}
```

The prototype of a class is ONE global (`cls->self`), every `I()` clones
it, and the hidden member holds every call's value, so the union forms
flow-insensitively in a single global and no contour split separates the
calls. Fixing it means the class object becomes a per-call value that
instantiation reads: a change to how classes are represented, not to
capture. A refusal, not a wrong answer, and no corpus program has a local
class reading its function's locals (sweep `check__default__d80ebc9e+ae99302a`).

## Verification

Two calls with different argument types print `1` and `a` on both
backends; `make test` green; corpus unchanged.
