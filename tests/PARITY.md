# pyc-suite C/LLVM backend parity

Snapshot of how the pyc-suite tests in this directory behave
under each backend. This is the live audit referenced by
`ifa/codegen/CG_IR_PLAN.md` §5.6 ("Stabilize the LLVM
pyc-suite baseline") and the source of CI's
`LLVM_BASELINE_PASS=37` floor.

This file is a measurement artifact, refreshed when the parity
shifts. Do not hand-edit individual entries — re-run the
audit (commands below) and replace the matrix.

## Methodology

```bash
# C backend (production):
./test_pyc -v

# LLVM backend (gated; requires `make USE_LLVM=1`):
PYC_FLAGS=-b ./test_pyc -v
```

`PYC_FLAGS=-b` selects the LLVM backend at runtime (see
`ifa/issues/013-pyc-llvm-default-off.md`); requires `pyc` built
with `make USE_LLVM=1`.

Stability validation: the LLVM-suite results below are
reproducible across **5 consecutive runs back-to-back**, each
~2:15 wall-clock. The five run-outputs `md5sum`-matched
byte-identically. Results stored under `/tmp/pyc_baseline/`
during the audit.

## Snapshot (June 2026)

|       | C backend | LLVM backend |
|-------|-----------|--------------|
| passed | 74 | 37 |
| expected fails | 1 | 0 |
| failed | 0 | 38 |
| skipped | 2 | 2 |
| total | 77 | 77 |

C backend: green. The single `expected fail` is
`class_attr_mutation.py` (Python semantics: see below).

LLVM backend: 37/77 = 48% parity. 38 failures, no flakes. All
38 fall into one of four documented buckets (none are CG_IR
blockers in themselves):

- **A. Construction-flow / SSU binding** (issue 014 + 016) —
  closes structurally when `cg_normalize` emits all Code_MOVE
  unconditionally and materializes phi/phy with strict
  intersection liveness.
- **B. POD records / `is_value_type`** (issue 015) — heap
  aggregates default to pointer; struct-by-value still needs
  `@pyc_struct` decorator support.
- **C. Compile-fail / unimplemented primitive** — LLVM backend
  doesn't yet emit a primitive (list/dict/string method,
  comprehensions). Tracked individually under
  `ifa/codegen/CODEGEN_PLAN.md` §3.5.
- **D. Runtime helper not linked** —
  `ifa/CODEGEN_LLVM.md` §14.5; `_CG_*` runtime helpers
  (`_CG_write`, `_CG_writeln`, etc.) declared but not
  resolved at link time.

## Per-test status

### Skipped on both backends (2)

| Test | Reason |
|---|---|
| `dict_methods.py` | (test-runner `.ignore`) |
| `t34_import.py` | (test-runner `.ignore`) |

### Pass on both backends (37)

`arithmetic_ops.py`, `basic_function.py`, `class_attributes.py`,
`class_definition_order.py`, `class_inheritance.py`,
`class_inherited_attrs.py`, `dynamic_attr.py`,
`empty_list_check.py`, `fibonacci.py`,
`forward_function_call.py`, `function_reassignment.py`,
`global_forward_ref.py`, `global_in_class.py`,
`lambda_basic.py`, `lambda_class_attr.py`,
`list_index_read.py`, `method_alias_both.py`,
`method_alias_class.py`, `method_alias_external.py`,
`method_on_literal.py`, `module_import.py`,
`multi_assignment.py`, `none_value.py`,
`print_multiple_args.py`, `return_value.py`,
`scope_ahead_of_use.py`, `scope_class_global.py`,
`scope_conditional_global.py`, `scope_global_after_local.py`,
`scope_global_before_define.py`,
`scope_global_local_conflict.py`, `scope_nested_global.py`,
`scope_nested_locals.py`, `tuple_len.py`, `tuple_unpack.py`,
`tuple_unpack_multiprint.py`, `while_loop.py`.

### Pass on C, fail on LLVM (37 + 1 xfail/fail mismatch)

Grouped by failure mode, then by hypothesized root-cause
bucket (A / B / C / D above).

#### EXEC-TIMEOUT — infinite loop (2)

Bucket A (issue 016 — for-loop / iterator binding). The
SSU-renamed iterator's self-binding doesn't survive into the
loop body; the loop never makes progress.

| Test | Bucket | Tracked under |
|---|---|---|
| `chained_comparison.py` | A | issue 016 |
| `logical_operators.py` | A | issue 016 |

#### EXEC — runtime diff or SEGV (20)

Bucket A (issue 014 construction flow / 016 binding) and
Bucket B (struct-vs-pointer drift):

| Test | Bucket | Tracked under |
|---|---|---|
| `attr_augmented_assign.py` | A | issue 014 |
| `bitwise_operators.py` | A | construction flow |
| `break_continue.py` | A | issue 016 |
| `class_init.py` | A | issue 014 |
| `conditional.py` | A | construction flow |
| `for_over_list.py` | A | issue 016 |
| `for_over_range.py` | A | issue 016 |
| `for_over_tuple.py` | A | issue 016 |
| `for_range_from_zero.py` | A | issue 016 |
| `list_index_assign.py` | A | construction flow |
| `list_print.py` | A | construction flow |
| `multi_type_unpack.py` | B | issue 015 |
| `operator_overload.py` | A | issue 014 |
| `polymorphic_function.py` | A | issue 014 |
| `sieve.py` | A | issue 016 |
| `string_augmented_concat.py` | A | issue 014 |
| `string_format.py` | A | issue 014 |
| `string_index.py` | C | string method |
| `string_unpack.py` | B | issue 015 |
| `tuple_mixed_types.py` | A | issue 014 |

#### COMPILE — pyc itself fails (16)

Mostly Bucket C (unimplemented LLVM primitives / typing):

| Test | Bucket | Tracked under |
|---|---|---|
| `builtins.py` | C | unimpl primitive |
| `class_attr_mutation.py` | (XFAIL on C, also fails on LLVM) | Python-semantics expected fail |
| `cross_type_method.py` | (also fails on C compile) | language limitation |
| `default_args.py` | C | unimpl primitive |
| `dict_basic.py` | C | dict primitives |
| `for_over_string.py` | C | string iter |
| `lambda_closure.py` | C | closure lowering |
| `list_comprehension.py` | C | list comp lowering |
| `list_concat.py` | C | list primitive |
| `list_multiply.py` | C | list primitive |
| `list_or_concat.py` | C | list primitive |
| `list_reassign.py` | C | list primitive |
| `list_slicing.py` | C | slice primitive |
| `pyc_declare.py` | C | declare primitive |
| `string_len.py` | C | string primitive |
| `string_repr.py` | C | string primitive |

Note: `class_attr_mutation.py` is the C backend's single
expected-fail (Python class-attribute mutation through
inheritance doesn't update derived-class struct layout
post-clone). It also fails to compile under LLVM, which is
consistent with the C-side behavior (both backends hit the same
underlying limitation; only the failure surface differs).

## CI floor

`LLVM_BASELINE_PASS=37` is grounded in this audit: 37 is the
*reproducible* pass count, not an aspirational moving target.
The CG_IR_PLAN Phase 3 ratchet (§8.2) treats this floor as the
post-PR-3.0 baseline; each subsequent PR must hold or improve
it.

## When to re-audit

Re-run the 5x stability check and refresh this file when:
- A new pyc-suite test is added (it slots into "pass on both"
  or one of the LLVM failure buckets).
- A CG_IR_PLAN phase lands and changes the pass count.
- Anyone notices flakes (results diverging across runs) — file
  a new issue and update the methodology note above.

## Cross-references

- `ifa/issues/014-llvm-construction-flow-to-slots.md` —
  construction-flow gap, Bucket A.
- `ifa/issues/015-pyc-pod-records-no-frontend-hook.md` — POD
  records / `is_value_type`, Bucket B.
- `ifa/issues/016-llvm-ssu-formal-arg-binding.md` —
  SSU formal-arg binding, Bucket A (iterator-binding subset).
- `ifa/codegen/CODEGEN_PLAN.md` §3.5 — LLVM-backend parity gap.
- `ifa/codegen/CG_IR_PLAN.md` §8.2 — pyc-suite ratchet.
- `ifa/CODEGEN_LLVM.md` §14.5 — runtime-helper linking (Bucket
  D once it surfaces).
