# 114 — a generator can only carry integers; other yields come back as raw pointers

**Status:** open, found 2026-08-20 while clearing ifa/issues/090's
sunfish residue. **Silent wrong answer** — no diagnostic, plausible
output, wrong values.

## Symptom

```python
def gen():
    yield (1, 2)
    yield (3, 4)
for x in gen():
    print(x)
```

    CPython:  (1, 2)          pyc:  138797340475344
              (3, 4)                138797340475280

The numbers are the tuples' addresses. Nothing warns.

Comparing instead of printing does produce a diagnostic, but an opaque
one:

```python
for x in gen():
    print(x == (1, 2))      # warning: illegal primitive argument type 'x' illegal: tuple
```

## Cause

`__pyc_generator__` (`__pyc__/09_generator.py`) moves values through an
int-typed channel:

```python
nextval = 0
...
self.nextval = __pyc_c_call__(int, "_CG_generator_value", int, self.handle)
```

and the runtime matches — `long long _CG_generator_value(long long)`.
A machine word is fine for a pointer, so the DATA survives; what is
wrong is the declared TYPE. `nextval` is an `int`, so a yielded tuple
is an integer from FA's point of view onward.

## Why nothing caught it

Every generator test in the suite yields integers —
`generator_basic`, `generator_infinite`, `generator_return_value`,
`generator_yield_from`, `fibheap_full`, `cs_split_pools`. The
limitation has simply never been exercised.

## The missing conduit

`yield X` lowers (python_ifa_build_if1.cc, `PY_yield_expr`) to

    if1_send(..., sym_primitive, "yield", yval, yval_result)

and that send does **not** reference the `__pyc_generator__` instance
that will deliver the value. So there is no path for `yval`'s type to
reach the instance's `nextval` slot, and the hardcoded `int` in the
c_call is the only thing typing it.

A fix has to create that conduit: give the yield prim (or the
generator-construction lowering around
python_ifa_build_if1.cc:2500) a constraint flowing the yielded value's
type into the generator object's `nextval`, and stop hardcoding `int`
as the c_call's return type. FA already clones per contour, so once the
type is not pinned, each generator should specialise.

## What this blocks

`shedskin_examples/sunfish` — its `gen_moves` yields move TUPLES, so
`move not in hist[-1].gen_moves()` (line 448) cannot work regardless of
the containment fix that landed alongside this filing. This is the
remaining half of issues/025 item 4.

More broadly: any generator over strings, tuples, lists or objects is
silently wrong today, which is most non-numeric generator code.

## Verification plan

- The repro above prints `(1, 2)` / `(3, 4)`.
- `x == (1, 2)` inside the loop compiles and matches CPython.
- A generator yielding strings round-trips.
- sunfish's line 448 no longer reports `unresolved call '__not__'`.
- Existing int-yielding generator tests unchanged.
