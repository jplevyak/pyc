# Issue 008: Set literals and generator expressions crash the compiler

**Status:** set literals and set comprehensions fixed. Generator
expressions: FIXED 2026-07-18 (see
[issues/025](../issues/025-shedskin-examples-coverage.md)'s
"Generator expressions implemented" entry) -- eagerly materializes
into a `list`, not a true lazy iterator (`yield`-backed laziness,
issue 014, was deliberately not used; no corpus usage found that
needs it). Revisit with real laziness if that changes.
**Affects:** `python_ifa_build_if1.cc` (`build_syms_pyda`'s and
`build_if1_pyda`'s `PY_set` cases — new; `PY_genexpr` given its own
`fail()` instead of falling into the shared no-op default);
`__pyc__/08_set.py` — new `set`/`__set_iter__` classes;
`python_ifa_build_if1.cc`'s `PY_compare` — fixed a pre-existing
`x in y` / `x not in y` operand-order bug found while wiring up
`set.__contains__` (see "What landed").
**Related:** issue 009 (dict comprehensions — same family of gap,
different failure mode, still open); **issue 017** (closed) — filed
after this fix exposed a serious pre-existing multi-instance data
corruption bug (affects `dict` too, nothing to do with sets
specifically), then fixed in a follow-up pass.

## Symptom

Both a set literal and a generator expression abort the compiler
process with an internal assertion — not a clean diagnostic:

```python
s = {1, 2, 3}
print(len(s))
```

```
pyc: if1/if1.cc:171: Code *if1_move(IF1 *, Code **, Sym *, Sym *, IFAAST *):
Assertion `a && b && a != b' failed.
Aborted (core dumped)
```

```python
g = (x * 2 for x in [1, 2, 3])
for v in g:
    print(v)
```

produces the identical crash and backtrace shape.

A set *comprehension* (`{x for x in y}`) hits the same assertion
(see issue 009 for the broader comprehension write-up).

## Root cause

`PY_set` and `PY_genexpr` are listed among the "nodes handled by
recursing into children with no special codegen" fallthrough in
`build_if1_pyda`:

```cpp
case PY_set:
case PY_genexpr:
case PY_yield_expr:
case PY_slice:
case PY_subscriptlist:
case PY_dotted_name:
case PY_dotted_as_name:
case PY_import_as_name:
default:
  for (auto c : n->children.values()) build_if1_pyda(c, ctx);
  return 0;
```

Unlike `PY_list`, `PY_tuple`, and `PY_dict` (each of which has its
own case above this, allocating a container and calling
`__setitem__`/append equivalents), `PY_set` and `PY_genexpr` never
allocate a result Sym (`ast->rval` is left null) — the code just
recurses into the element expressions and returns. Whatever
enclosing construct then tries to `if1_move` this AST's
(nonexistent) `rval` into a destination Sym hits
`assert(a && b && a != b)` with `a` (or `b`) null, because nothing
upstream ever produced a value for the set/genexpr node.

## Proposed fix sketch

1. **Set literals** — mirror the existing `PY_list`/`PY_tuple`
   case shape: allocate via the `set` builtin class (add if
   missing to `__pyc__/`), then call `__pyc_add__`/`add`-equivalent
   for each element expression, same pattern already used for
   `PY_dict`'s `__setitem__` loop just below it in the same
   function.
2. **Generator expressions** — these are more involved: a genexpr
   needs to become an actual lazily-evaluated iterator object
   (likely reusing whatever mechanism eventually backs `yield`,
   see issue 014), not just an eagerly-materialized list. If full
   laziness is out of scope initially, consider an explicit
   `fail("generator expressions not yet supported")` as an interim
   fix — turning the crash into a clean, testable diagnostic is
   strictly better even before the real feature lands.
3. Either way, the *immediate* actionable fix is: **stop letting
   these fall through to the no-op default case**. At minimum,
   add an explicit `fail(...)` for `PY_genexpr` so it degrades
   from "compiler crash" to "clean rejection" while the real
   fix is designed.

## What landed

1. **`__pyc__/08_set.py`**: a new `set`/`__set_iter__` pair, modeled
   directly on `07_dict.py`'s linear-scan-over-a-backing-list
   pattern (`_items`/`_len`, `self._items = self._items.append(x)`
   dedup-on-add). Implements `__len__`, `__contains__`, `add`,
   `discard`, `remove`, `pop`, `clear`, `__iter__`,
   `__pyc_to_bool__`, `__eq__`/`__ne__`, `__str__`.
2. **`build_syms_pyda`'s `PY_set` case**: resolves `ast->sym` to the
   `set` class (mirroring `PY_dict`), and — since `dictorsetmaker`
   gives the comprehension form `{expr for ...}` a 2-child shape
   (`[expr, PY_comp_for]`) distinct from the flat-literal N-child
   shape — gives the comprehension form its own scope, mirroring
   `PY_listcomp`/`PY_genexpr` just above it.
3. **`build_if1_pyda`'s `PY_set` case**: instantiates an empty `set`
   via a real constructor call (like `PY_dict`, since there's no
   low-level `sym_set` primitive the way `PY_list`/`PY_tuple` have);
   flat literals call `add()` per element; comprehensions reuse
   `build_list_comp_pyda` (the same loop-lowering machinery
   `PY_listcomp` uses), parameterized with a new `accum_method`
   argument so it calls `add()` instead of `sym_append`.
4. **`PY_genexpr`**: split out of the shared silent-recursion
   fallthrough into its own `fail("generator expressions not yet
   supported")` — the interim fix this issue's sketch proposed.
5. **Fixed `x in y` / `x not in y`** (`PY_compare`): discovered while
   wiring up `2 in {1,2,3}` that the *existing* lowering called
   `lv.__contains__(rv)` (left operand as receiver) instead of the
   correct `rv.__contains__(lv)` — Python's `in` is asymmetric, the
   container (right operand) is always the receiver. Also dropped
   the fictitious `__ncontains__` dunder (`not in` doesn't have a
   separate protocol in real Python) in favor of `__contains__` +
   `__not__`, matching how `is not` is already lowered. This bug
   predated this issue and affected **every** container type
   (`list`, `dict`, `set`) — confirmed via `2 in [1,2,3]`, which was
   equally broken before this fix. Also implemented
   `list.__contains__` (`__pyc__/04_sequence.py`), previously a
   `pass` stub that silently returned `None` — invisible until the
   operand-order fix made it reachable.

Verified against real `python3` output (`tests/set_literal_basic.py`,
`tests/genexpr_basic.py` locks in the clean `fail()` via
`.expect_fail`): set literals, dedup, `in`/`not in`, iteration,
`str()`, comprehensions (plain and filtered), `add`/`discard`,
multiple concurrent instances. Passes on both the C and v2 LLVM
backends.

**While testing, found (and later fixed) issue 017**: constructing a
second instance of `dict` (or `set`) in the same function and
mutating it produced silently wrong values — nothing to do with sets
specifically (a minimal `dict`-only repro reproduced it identically).
It initially constrained how the new set tests could be composed
(each test kept mutation to a single instance, or kept multiple
instances read-only); once issue 017 was fixed, `tests/set_literal_basic.py`
was consolidated back into one natural test exercising several
concurrent instances (`s`, `dup`, `comp`, `evens`) plus mutation,
since the artificial isolation was no longer needed.

## Verification plan

1. `{1, 2, 3}` compiles and `len(s) == 3` at runtime. ✓
2. `(x * 2 for x in [1, 2, 3])` fails cleanly (interim; real
   laziness tracked in issue 014). ✓
3. No `Assertion .* failed` / core dump for either construct. ✓
4. `tests/set_literal_basic.py` and `tests/genexpr_basic.py` added. ✓

## What this unblocks

Set literals are common in idiomatic Python (deduplication,
membership tests) and now work correctly on both backends. Generator
expressions remain unimplemented but no longer crash the compiler —
tracked jointly with issue 014. The `in`/`not in` operator fix
unblocks membership testing for *any* container, not just sets.
