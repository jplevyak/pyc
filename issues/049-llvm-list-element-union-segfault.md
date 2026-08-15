# 049 — LLVM backend segfaults on `list_element_type_union.py`; the C backend is correct

**Status:** open, 2026-08-15. **This is a live regression on a supported
backend**, introduced today by making `PYC_NOMARK=1` the default
(ifa/issues/074). `tests/list_element_type_union.py` is deliberately left
FAILING under `PYC_FLAGS="-b"` rather than tagged `.known_issue`, because
suppressing a regression the same day it is introduced is how it gets
forgotten.

## Symptom

```
$ PYC_FLAGS="-b" python3 test_pyc.py list_element_type_union
  failed 1   (EXEC: no output; the binary segfaults)
```

The C backend compiles and runs the same program correctly:

```
0 15 16
1 125 141
2 576 717
```

The LLVM backend compiles it with **rc=0 and no diagnostics**, then
segfaults.

## Bisected to the flag, exactly

```
PYC_NOMARK=0 PYC_SELFPROD=0  -> correct
PYC_NOMARK=0                 -> correct
PYC_SELFPROD=0               -> segfault
(defaults)                   -> segfault
```

So it is `PYC_NOMARK=1` alone; `PYC_SELFPROD` is not involved.

## What this is (and is not)

It is **not** evidence that marks-off is wrong. Marks-off is a large net
win on the C backend and the 84-program corpus (074), and this same
program is *correct* on C with marks off. What changed is the FA output
the backends are handed: marks-off yields a list whose element type is a
union where marks previously kept it separated, and the **LLVM backend
lacks a guard the C backend has** for that shape.

That is the same family as
[035](035-list-element-cast-salvage-guard-and-set-item-union.md), whose
guards live in `cg.cc` — including the read-side one added the same day
(`P_prim_index_object`'s constant-index record branch). `cg_emit_llvm.cc`
has no counterpart, so where C emits a defined value (or the established
runtime assert), LLVM emits code that dereferences a scalar.

## Where to look

`cg_emit_llvm.cc`'s element load/store against a container whose element
type is a union — compare with `cg.cc`'s `P_prim_index_object` /
`P_prim_set_index_object` `num_kind` guards and mirror whichever of them
applies. Note the C read-side guard is deliberately asymmetric (see 035's
"The READ side"): only the constant-index record branch needs it, because
only that branch reads a field at its own declared type.

## Verification plan

- `PYC_FLAGS="-b" python3 test_pyc.py` returns to 0 failures.
- The C backend suite stays at its current count.
- Re-check whether any other corpus program's LLVM output changed under
  marks-off; only this one test was caught, and the shedskin sweep is
  C-backend only, so LLVM coverage of the change is thin.

## What this unblocks

The LLVM backend as a supported target under the current defaults. Until
it is fixed, `PYC_FLAGS="-b"` has one failing test.

## Process note

This was missed for several commits because `test_pyc.py` with no
`PYC_FLAGS` runs the **C backend only** — several commit messages in the
074 series claim "both backends" on the strength of that single run. They
are wrong; the LLVM backend needs `PYC_FLAGS="-b"` explicitly.
