# Issue 008: Set literals and generator expressions crash the compiler

**Status:** open.
**Affects:** `python_ifa_build_if1.cc:1319-1327` (`PY_set` /
`PY_genexpr` fall into the generic default case in
`build_if1_pyda`); `ifa/if1/if1.cc:171` (`if1_move`, where the
crash actually fires).
**Related:** issue 009 (dict/set comprehensions — same family of
gap, different failure mode).

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

## Verification plan

1. `{1, 2, 3}` compiles and `len(s) == 3` at runtime (once
   implemented), or fails cleanly with a diagnostic (interim).
2. `(x * 2 for x in [1, 2, 3])` iterates correctly in a `for`
   loop and produces `[2, 4, 6]` (once implemented), or fails
   cleanly (interim).
3. No `Assertion .* failed` / core dump for either construct —
   the interim bar is "clean fail() instead of crash."
4. Add `tests/set_literal_basic.py` and
   `tests/genexpr_basic.py` (+ `.exec.check` or
   `.expect_fail`/`.check_fail` markers matching whichever fix
   tier lands) — neither exists today (confirmed: no test in
   `tests/*.py` uses set-literal or genexpr syntax).

## What this unblocks

Set literals are common in idiomatic Python (deduplication,
membership tests); generator expressions are the standard
memory-efficient alternative to list comprehensions and appear
throughout ported CPython code (e.g. `sum(x*x for x in items)`).
Beyond the missing feature, an uncaught process abort on valid
Python syntax is a much worse failure mode than a clean
diagnostic — it should be triaged ahead of the "add the feature"
work purely as a crash-hardening fix.
