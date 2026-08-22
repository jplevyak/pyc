# 052 — LLVM backend: a `{None, int}` value that is 0 prints as `None`

**Status:** CLOSED — resolved 2026-08-19 (`e1381dc7`) by REFUSING, not
by fixing the representation; see "Resolved by refusing" below. The
silent wrong answer this issue exists for is gone (both backends now
reject the dispatch at compile time). Giving `{None, int}` an actual
representation remains [048](../048-none-int-field-pair-runtime-abort.md)'s
open problem, which is what `tests/none_int_field_zero.py` still waits
on.
[048](../048-none-int-field-pair-runtime-abort.md) in `cg.cc`. **Silent
wrong answer** — no diagnostic, correct-looking output, wrong value.
Repro: `tests/none_int_field_zero.py`.

## Symptom

```python
class V:
    def __init__(self):
        self.a = None
        self.b = None
v = V(); v.a = 0; v.b = 2
print(v.a, v.b)
```

| | output |
|---|---|
| CPython | `0 2` |
| **LLVM (`-b`)** | **`None 2`** |
| C | aborts (`matching function not found`) — see 048 |

## Cause

The LLVM backend keeps a `{nil, int64}` union in *pointer*
representation and discriminates with a null test:

```llvm
store ptr inttoptr (i64 1 to ptr), ptr %2      ; v.a = 1
%5 = load ptr, ptr %4
%nilcmp = icmp eq ptr %5, null                 ; "is it None?"
br i1 %nilcmp, label %poly.nil, label %poly.nonnull
```

`inttoptr (i64 0)` **is** null, so the integer 0 takes the `None`
branch. Any non-zero value happens to work, which is why this went
unnoticed — the obvious test value is 1.

## The C backend already refuses this, on purpose

`cg.cc`'s dispatch emitter has an explicit veto:

> A nil test on a SCALAR-typed operand can't distinguish None from
> 0/0.0/False: if the shared dispatch operand's C type is scalar and a
> nil branch exists, bail rather than miscompile (print of a `{nil,int64}`
> union would render 0 as "None").

That comment describes this bug exactly. The C backend bails to a loud
`assert` (which is 048); the LLVM backend has no counterpart and emits
the miscompile the veto exists to prevent.

Note the veto's documented exception — the truthiness selectors
(`__pyc_to_bool__`, `__bool__`, `__not__`), where `None` and `0` are both
falsy so the conflation is invisible. Any LLVM-side fix wants the same
carve-out.

## Fix direction

Mirror the C veto in `cg_emit_llvm.cc`: when the dispatch operand's
representation is scalar and a nil branch exists, refuse rather than
emit `icmp eq ptr … null` — unless the selector is one of the
truthiness ones. That converts a silent wrong answer into the same loud
failure the C backend gives, which is the honest state of affairs until
the representation gap ([048](../048-none-int-field-pair-runtime-abort.md),
[030](../../ifa/issues/030-DISPATCH-polymorphic-dispatch-fat-pointers.md))
is closed.

Making it actually *work* on either backend needs the union to stop being
scalar-represented — a tagged/boxed representation, or FA narrowing the
field so the union never reaches codegen.

## Verification plan

- `tests/none_int_field_zero.py` — LLVM must stop printing `None 2`.
  Matching CPython (`0 2`) requires the representation fix; failing
  loudly is the acceptable interim.
- `tests/none_int_field_pair.py` (the same shape with `1`) must not
  regress on LLVM, where it currently passes.

## What this unblocks

Trust in the LLVM backend for any program with a `None`-initialised
numeric field — currently it can produce a plausible wrong number (or
`None`) with no warning at all.


## Resolved by refusing (2026-08-19, `e1381dc7`)

The LLVM backend no longer emits this dispatch. When the receiver is a
`Type_SUM` containing `sym_nil_type` and a member with `num_kind`, it
fails at COMPILE time:

```
z1.py:9: codegen: '__str__' on a {None, int64} union has no
  representation: a null test cannot distinguish None from
  0/0.0/False (issues/048)
```

This is the same judgement the C backend already made, and it removes
the silent wrong answer that made this issue. It does NOT give the
union a representation — that is still issues/048's upstream problem
(FA narrowing, or issues/030's tagged pointers). What changed is that
both backends now refuse rather than one of them guessing.

The truthiness exception is carried over from the C guard:
`__pyc_to_bool__` / `__bool__` / `__not__` still emit, because None and
zero are both falsy there and the conflation is invisible.

Measured: all 77 corpus programs compile under `-b` and **zero** hit
the new refusal. In the suite, `none_int_field_pair` and
`none_float_field` moved from "pass" to the `.known_issue` tag they
already carried — they had only been passing because their test values
are 1, and 1 is non-null.
