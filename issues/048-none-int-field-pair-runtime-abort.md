# 048 — genuine scalar unions: strict refuses, permissive may accommodate under a flag

**Status:** open on one item. Rewritten 2026-11-03; history in git
(`git show 23cd789:issues/048-none-int-field-pair-runtime-abort.md`).

## Scope

Unions that are REAL in CPython (one slot genuinely holds both kinds over
its lifetime) with no exact unboxed representation. A union pyc invented
is an inference bug, filed under `ifa/`. A placeholder that is always
overwritten before any read is not genuine either: [ifa/191](../ifa/issues/191-FA-an-always-overwritten-initial-value-stays-in-the-field-type.md)
keeps it out of the type (fields done; `[None] * n` with scalar elements
open there).

## The table

| union | fixture | state | exact? |
| --- | --- | --- | --- |
| `{None, T*}` | `tests/nil_union_prealloc.py`, `none_receiver_raises.py` | nullable pointer; a `None` that arrives raises CPython's `TypeError` (catchability: [ifa/165](../ifa/issues/165-none-reaching-an-operation-is-silently-accepted.md)) | yes |
| `{int, float}` element | `tests/list_mul_heterogeneous_element.py` (`.known_issue`) | permissive widens and warns (`show_numeric_coercions`; `fruntime_errors`, ifa/145), printing `[1.5, 0.0, 0.0]` for CPython's `[1.5, 0, 0]`; `--strict` refuses | no |
| `{None, int}` / `{None, float}` that is READ | `tests/field_none_read_before_overwrite.py`, `none_or_int_operator_raises.py` | refused in every mode: a method or operator on a `{None, scalar}` value is a BOXING violation (ifa/165, closed/183) | no |
| `{bool, None}` | `tests/bool_or_none_fallthrough.py` (`.known_issue`) | refused on width | **yes**, bool has spare codes: a representation, [ifa/118](../ifa/issues/118-union-field-representation-and-polymorphic-field-offset.md) |
| `{int, str}` branch merge | `tests/branch_merged_scalar_union.py` | refused in every mode | no |

## The one open item: a permissive sentinel for a read `{None, scalar}`

Represent `None` in a `{None, int}` slot as `INT64_MIN` and in a
`{None, float}` slot as a reserved NaN payload; `is None` and truthiness
test the sentinel. Inexact only if the program stores that exact value,
which is what makes it an accommodation: behind `PYC_NIL_SCALAR_SENTINEL`,
warning at the field, off under `--strict`.

**Build it only on demand.** No corpus program needs it: genetic2's was
removed by an edit, pisang's `None` was unreachable, and the
preallocation idiom is ifa/191's. Until a program needs it, "refused in
every mode" is the correct, documented state, and this issue is the
record of why. A refusal of a program that CPython runs is still rule 1's
third-worst outcome, so the first program that needs it is the trigger.

## Verification

`--strict`: every row refused with a diagnostic naming the union, except
the exact rows. Default: unchanged until the sentinel lands; then
`field_none_read_before_overwrite` prints CPython's answer with one
warning. Keep the two `.known_issue` sidecars: both still differ from
CPython, and the check files hold CPython's answer.
