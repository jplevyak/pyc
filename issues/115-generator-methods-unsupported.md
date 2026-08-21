# 115 — a generator METHOD is not a generator at all

**Status:** FIXED 2026-08-21, found while verifying issues/114.
Pre-existing since generators landed (`3e18bcfa`); never diagnosed
because no test in the suite defined one.

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

## FIXED 2026-08-21

`tests/generator_methods.py`. Both backends 288 -> 292 passed / 0
failed (with issues/116-118's tests).

### What landed

The split a plain generator def already had, extended to methods —
three small pieces, and one shared builder instead of the copy that
would otherwise have been:

- **build_syms_pyda's `PY_funcdef`** creates the wrapper Sym for a
  generator method and puts THAT in the class setter, not the coroutine
  body. It has to decide before `def_fun_pyda` has run (the setter is
  emitted right after it), so it scans the AST for a `yield` directly —
  the same scan `def_fun_pyda` itself uses. `ast->rval->alias` becomes
  the wrapper too: the alias is what `gen_class_pyda`'s `includes` loop
  copies into a subclass and what `find_class_method_fn` resolves, so
  leaving it on the body would have handed subclasses the raw coroutine.
- **`gen_fun_pyda`** gives a generator method's BODY the value-carried
  `as[0] = fn` convention instead of the name-symbol placeholder, so the
  wrapper can call it directly by Sym. Both under one name would be an
  ambiguity or an infinite recursion depending on which won. `fn->self`
  is unaffected — the `!cls && is_method` block still takes `as[1]`,
  which is the Python `self` parameter either way.
- **build_if1_pyda's `PY_funcdef`** fills in the wrapper body once
  `gen_fun_pyda` has built the coroutine to call, and specializes the
  wrapper's forwarded `self` to the enclosing class so a same-named
  generator method on an unrelated class cannot capture the dispatch.
- **`build_generator_wrapper`** is the shared builder. The plain-def and
  method cases differ in exactly one value — what becomes the wrapper's
  `has[0]`: the wrapper Fun itself (value-carried, for a call site that
  reads a variable and calls the value) or a placeholder specialized to
  the method's name symbol (for the period send).

Covered: arguments besides `self`, inheritance, same-named methods on
unrelated classes, `yield from self.inner()`, `.send()`, non-int yields,
and the `None`-seeded membership loop sunfish uses. `*args`/`**kwargs`/
defaults/keyword-only are still not forwarded — the same gap the
plain-def wrapper already had, now noted in one place.

### sunfish: this was not the last blocker

Three more pre-existing bugs stood behind it, each found by reducing
the previous one and none generator-related:

- **[issues/117](117-string-literal-decoder-truncation.md)** — implicit
  string concatenation dropped every fragment after the first, so
  sunfish's 120-character board was 10 characters. Fixed.
- **[issues/118](118-str-case-predicates-missing.md)** — `str.isspace`,
  `islower` and `swapcase` did not exist. Fixed.
- **[issues/116](116-iterator-protocol-needs-pyc-more.md)** — a class
  with only `__iter__`/`__next__` iterates zero times because
  `object.__pyc_more__` returns False; `itertools.count` was the live
  instance. Fixed for `count`; the protocol gap is open.

`Position.gen_moves` now compiles clean. sunfish's remaining
diagnostics are elsewhere: `str.split()` called with no `sep`, and
something reached through `re.py`.
