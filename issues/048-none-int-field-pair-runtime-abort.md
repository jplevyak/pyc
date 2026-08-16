# 048 — two `None`-initialised instance fields later holding ints: clean compile, runtime abort

**Status:** open, 2026-08-15; **substantially corrected 2026-08-16 — this
is NOT a codegen bug** and the C backend's abort is a deliberate, correct
refusal (see the CORRECTION section). Found while narrowing
[046](046-default-arg-omitted-differently-silently-wrong.md); a separate
defect. Repro landed as `tests/none_int_field_pair.py` with a
`.known_issue` tag, so the suite tracks it and will flip it to PASS on its
own when this is fixed.

## Symptom

```python
class V:
    def __init__(self):
        self.a = None
        self.b = None

v = V()
v.a = 1
v.b = 2
print(v.a, v.b)
```

CPython prints `1 2`. pyc compiles it with **zero diagnostics** — no
warning, no violation — and the binary aborts:

```
runtime error: matching function not found
```

The lack of any diagnostic is the notable part: this is not a
salvage-degraded contour that pyc knows it failed on. It believes it
compiled the program.

## CORRECTION (2026-08-16): the C backend is RIGHT to refuse

The 2026-08-15 section this replaces said the LLVM backend "compiles and
runs this correctly" and was "the reference implementation to diff
against", so the fix belonged in `cg.cc`. **That was wrong, and it was
wrong because the test value was 1.**

With the field set to **0** instead:

| | output |
|---|---|
| CPython | `0 2` |
| LLVM | **`None 2`** — silently wrong, filed as [052](052-llvm-nil-test-on-scalar-union-prints-none-for-zero.md) |
| C | aborts, as here |

LLVM keeps the `{nil, int64}` union in pointer representation and
discriminates with `icmp eq ptr %x, null`; `inttoptr (i64 0)` *is* null,
so 0 takes the `None` branch. It only looked correct because 1 is
non-null.

`cg.cc` declines to emit that test on purpose, and its comment describes
the bug precisely:

> A nil test on a SCALAR-typed operand can't distinguish None from
> 0/0.0/False: if the shared dispatch operand's C type is scalar and a
> nil branch exists, bail rather than miscompile (print of a
> `{nil,int64}` union would render 0 as "None").

So **there is nothing to fix in `cg.cc`** — the abort is a correct,
deliberate refusal to miscompile, and it is the better of the two
behaviours. This issue is not a codegen bug at all.

## What this issue actually is

The defect is upstream: a `{nil, int64}` union survives to codegen on a
field that is provably an `int` at the point of use. `self.a = None` in
`__init__`, `v.a = 0`, then `print(v.a)` — nothing can observe the `None`.
Either FA should narrow the field so the union never reaches codegen, or
the union needs a representation that can carry the tag
([030](../ifa/issues/030-DISPATCH-polymorphic-dispatch-fat-pointers.md)).

That also places it with 018's surviving half (a branch-merged `int|str`
scalar) rather than apart from it, reversing the other half of
yesterday's classification: both are basic-type unions that codegen
cannot represent, and neither is a `cg.cc` defect.

## What is known

- Both fields **do** get their own slots (`e12 /* a */`, `e13 /* b */`),
  so this is *not* the elided-slot confusion of
  [046](046-default-arg-omitted-differently-silently-wrong.md) — the two
  bugs share a discovery path and nothing else.
- Each field's type is the union `None | int64`. `matching function not
  found` is the runtime dispatch failure that pyc emits when no branch of
  a polymorphic send matches the receiver, so the likely story is `print`
  (or `int64::__str__`) being dispatched on a boxed `None|int` field whose
  runtime tag matches neither arm.
- It places the shape in the `None`-in-a-union family with
  [018](018-dict-mixed-key-types-boxing-failure.md),
  [030](030-int-float-in-place-list-mutation.md) and
  [035](035-list-element-cast-salvage-guard-and-set-item-union.md),
  but unlike those it needs no container, no in-place mutation and no
  heterogeneous element type — two plain scalar fields are enough, which
  makes it by far the smallest witness in that family.

Not investigated further than that; the above is inference from the
message plus the field layout, not a traced dispatch.

## Verification plan

- `tests/none_int_field_pair.py` prints `1 2` and passes without its
  `.known_issue` tag, which should then be deleted.
- Check whether the same shape with only ONE field is affected (it is
  believed not to be — the single-field variants exercised while
  narrowing 046 ran correctly).

## What this unblocks

A two-field class with `None` initialisers is thoroughly ordinary Python,
and today it miscompiles silently. Beyond the shape itself, this is the
smallest known repro in the `None`-union boxing family, so it is the
cheapest place to attack that family.
