# 048 — genuine scalar unions: strict refuses, permissive accommodates under a flag

**Status:** open. Rewritten 2026-09-28. It now also carries
[035](closed/035-list-element-cast-salvage-guard-and-set-item-union.md)'s
open half. History in git:
`git show e3b44e2c:issues/048-none-int-field-pair-runtime-abort.md`.
The policy it implements is
[171](171-permissive-accommodations-must-be-flagged-and-non-strict.md).

## Scope

This covers unions that are REAL in CPython (the program genuinely holds
both kinds in one slot over its lifetime) and that have no exact unboxed
representation. A union pyc invented is out of scope: that is an
inference bug (AGENTS.md), filed under ifa/.

## The table (re-verified 2026-09-28)

| union | fixture | today | exact representation? | verdict |
| --- | --- | --- | --- | --- |
| `{None, T*}` | `tests/nil_union_prealloc.py` | works: nullable pointer; a `None` that does arrive raises CPython's `TypeError` (`tests/none_receiver_raises.py`) | yes | ✓ (ifa/165, 2026-09-28) |
| `{int, float}` element | `tests/list_mul_heterogeneous_element.py` (`n*[0]`, then `x[i] += 1.5`) | permissive: widened, prints `[1.5, 0.0, 0.0]`; CPython `[1.5, 0, 0]` | no | accommodation (ifa/145 gating ✓). Needs the 171 warning. Strict refuses ✓ |
| `{None, int}` fields | `tests/none_int_field_pair.py`, `none_int_field_zero.py` | **refused at compile time** in every mode (2026-09-28): a method dispatched on a `{None, scalar}` receiver is a BOXING violation in FA, `expression has mixed basic types: (__pyc_None_type__ int64)` | no: every int64 bit pattern is a valid int | ✓ strict. Permissive: see below |
| `{None, float}` field | `tests/none_float_field.py` | refused at the store/load guard with a named warning | no | ✓ strict; permissive: see below |
| `{bool, None}` return | `tests/bool_or_none_fallthrough.py` | refused (width) | **yes**: bool has spare codes | a representation, not an accommodation: [ifa/118](../ifa/issues/118-union-field-representation-and-polymorphic-field-offset.md) |
| `{int, str}` branch merge | `tests/branch_merged_scalar_union.py` | refused | no | ✓ refused in every mode; no reasonable accommodation |

## What to build

1. ~~**No runtime abort for `{None, int}`.**~~ DONE 2026-09-28:
   `collect_var_type_violations` (fa.cc) raises BOXING on the receiver of
   any live `P_prim_period` whose type holds `None` and a scalar, except
   for the truthiness selectors, where a null test is exact. It is the
   end state for strict.
2. **Permissive accommodation, behind a flag
   (`PYC_NIL_SCALAR_SENTINEL`):** represent `None` in a `{None, int}`
   slot as a reserved sentinel (`INT64_MIN`), and in `{None, float}` as a
   reserved NaN payload. `is None` and truthiness test the sentinel. It
   is inexact only if the program stores that exact value, which is what
   makes it an accommodation. It warns when it fires, naming the field.
   Off under `--strict`.
3. **Corpus programs:** where a corpus program needs 2 and the `None` is a
   placeholder the program always overwrites (the `[None] * n`
   preallocation idiom, or an `__init__` default), prefer the edit that
   states the intent (`0`, or `0.0`, as the initial value) when it is a
   CPython no-op on every path that runs (PYC_CHANGES.md).

## Verification

- `--strict`: every fixture in the table is refused with a diagnostic
  naming the union, except the ✓-exact rows, which pass.
- Default: `none_int_field_pair` prints `1 2` with one warning (the
  sentinel). `list_mul_heterogeneous_element` prints `[1.5, 0.0, 0.0]`
  with one warning (the widening). Keep the `.known_issue` sidecars:
  both still differ from CPython, and the check files hold CPython's
  answer.
- No runtime `matching function not found` from any row.
