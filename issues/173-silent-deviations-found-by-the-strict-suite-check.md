# 173 — silent wrong answers the strict-suite comparison found

**Status:** open on three items. Filed 2026-09-29 from
[171](closed/171-permissive-accommodations-must-be-flagged-and-non-strict.md)'s
verification: every `tests/*.py` compiled with `--strict`, run, and compared
with CPython. Its first item, `isinstance` on a `{list, str}` value folding
to False, was fixed 2026-10-15 by refusing what no run-time tag can decide
(`cg_isinstance_tag_targets`; `tests/isinstance_dynamic.py.check_fail`).
Each item below is wrong the same way in every mode, so none is an
accommodation: each needs a fix or a refusal.

## 1. A class attribute read through a subclass after the base is mutated

`tests/class_attr_mutation.py` (`.known_issue`; its check holds CPython's
output):

```python
class A: n = 2
class B(A): o = 3
A.n = 4
print(B.n)          # CPython: 4.   pyc: 2
```

The subclass prototype holds its own copy of `n`, taken at class
creation. CPython looks an unset class attribute up through the MRO at
read time. Fix: a subclass does not copy an inherited class attribute it
never assigns; its read goes to the base's prototype. The same gap
refuses a decorator stored on a class after its definition
([007](007-decorators-not-applied.md)).

## 2. A global read before its definition sees the later value

`tests/scope_global_before_define.py` (run under `--test-scoping`, so the
binary never runs):

```python
class a:
  global z
  print(z)          # CPython: NameError.   pyc: prints 1
z = 1
```

A read CPython rejects at run time must be refused or checked at run
time, as closed/103 does for an executed undefined name; it must not see
a value from the future. The flow-insensitive global slot is
[ifa/050](../ifa/issues/050-FA-general-constant-propagation-unreachable-code.md)'s
subject; a write-once global (its 1b) answers this case exactly.

## 3. Deep recursion overflows the C stack; `sys.setrecursionlimit` is ignored

`tests/list_index_type_mismatch_salvage.py` (corpus `loop`) segfaults in
every mode: 16,883 frames of DFS recursion, and `ulimit -s unlimited`
runs it to completion. The program calls `sys.setrecursionlimit(100000)`,
which `pyc_lib/sys.py` accepts and ignores. Fix: run the program on a
stack sized to the limit (the runtime's `main` starts the program on a
thread with a large stack), and turn an overflow into CPython's
`RecursionError` rather than a SIGSEGV.

## Noted, not actionable here

- `lambda_class_attr`: the default instance repr is `<object>` where
  CPython prints `<__main__.A object at 0x…>`. The address can never
  match; printing the class name is cheap and is all that can.
- `zip_builtin`, `tuple_list_mix`: `len(zip(...))` and `zip(...)[i]`
  compile where CPython raises. The eager lists are a documented
  representation by the author's decision (README accommodation table).
- `list_tuple_union_method`: a `{list, tuple}` receiver,
  [ifa/102](../ifa/issues/closed/102-codegen-detected-conditions-are-refusals-or-demands.md).

## Verification

Re-run the comparison (the script is in 171's verification record):
every item matches CPython or is refused with a named diagnostic.
