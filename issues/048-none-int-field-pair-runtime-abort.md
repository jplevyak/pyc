# 048 — two `None`-initialised instance fields later holding ints: clean compile, runtime abort

**Status:** open, 2026-08-15. Found while narrowing
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
- That places it in the `None`-in-a-union boxing family with
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
