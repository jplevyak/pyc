# 115 — a generator METHOD is not a generator at all

**Status:** open, found 2026-08-21 while verifying issues/114.
Pre-existing since generators landed (`3e18bcfa`); never diagnosed
because no test in the suite defines one.

## Symptom

```python
class P:
    def __init__(self, a):
        self.a = a
    def nums(self):
        yield self.a
        yield self.a + 1

for m in P(7).nums():
    print(m)
```

    CPython:  7          pyc:  warning: unresolved call '__iter__'
              8                assert(!"runtime error: matching function not found")

Not type-specific — this is a plain int-yielding generator, the case
issues/114 explicitly left working. Any `yield` inside a `def` that is
a class member fails the same way.

## Cause

`__pyc_generator__` is only ever constructed in one place, the
synthesized wrapper in build_if1_pyda's `PY_funcdef` case
(python_ifa_build_if1.cc), and that wrapper is gated on
**`!fd_is_method`**:

```cpp
} else if (!fd_is_method && ast->rval != ast->sym && ast->sym->is_generator) {
```

A method is installed into its class by the setter build_syms_pyda
emits, and gets no variable binding here — so it never reaches the
wrapper. `P().nums()` therefore calls the raw coroutine body and hands
back the bare `long long` handle, which has no `__iter__`,
`__pyc_more__` or `__next__`. The guard is original to the generator
commit; the two branches were written for the "public name != internal
Fun" split (issues/007) and the method case was simply not covered.

## Proposed fix

Wrap on the method path too. The wrapper is a pure passthrough over
`ast->sym->has[1..]`, so the shape carries over directly — what changes
is where the result is installed: for a method it has to go through the
same class setter the method itself uses, with the wrapper (not the
coroutine body) as the installed value, and `self` forwarded as the
first argument rather than dropped.

Worth checking while there: the wrapper builds fresh formal Syms and
does not handle `*args`/`**kwargs`/defaults/keyword-only, which its own
comment already flags. A method's `self` must not be routed through
that gap.

## What this blocks

`shedskin_examples/sunfish` — `Position.gen_moves` is a **method**
generator, so line 448's `move not in hist[-1].gen_moves()` cannot work
even now that issues/114 lets generators carry tuples. Verified by
reduction: the identical program with a module-level generator runs
correctly and matches CPython, and the method version fails on plain
ints. This, not the value channel, is the remaining half of issues/025
item 4.

Also blocks any class exposing a lazy sequence the Pythonic way, which
is most iterator-shaped OO code.

## Verification plan

- The repro above prints `7` / `8`.
- A method generator yielding tuples round-trips (issues/114's channel
  fix already covers the typing once the wrapper exists).
- `(7, 1) in P(7).moves()` resolves `__contains__`.
- A method generator taking arguments besides `self` forwards them.
- sunfish's line 448 no longer reports `unresolved call '__not__'`.
- Existing module-level generator tests unchanged.
