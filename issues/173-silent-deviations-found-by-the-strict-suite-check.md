# 173 — silent wrong answers the strict-suite comparison found (every mode)

**Status:** open. Filed 2026-09-29 from
[171](closed/171-permissive-accommodations-must-be-flagged-and-non-strict.md)'s
verification step: every `tests/*.py` compiled with `--strict`, run, and
compared with CPython's stdout and exit status. 336 matched, 38 were
refused, and 24 differed. Of the 24, 14 are programs CPython cannot run
(pyc extensions such as `pyc_compat`/`__pyc_c_call__`, async runtime
tests) and 2 differ only in where a traceback goes (stderr vs stdout). The
rest are below. None is an accommodation: each is wrong the same way in
permissive mode, so none belongs behind a flag. Each needs a fix or a
refusal.

## 1. `isinstance` on a `{list, str}` value folds to False

`tests/isinstance_dynamic.py`:

```python
def check(v):
    if isinstance(v, list): return 1
    return 0
for v in [[1, 2], "hello"]:
    print(check(v))     # CPython: 1 0.   pyc: 0 0, no diagnostic
```

`v` is `{list, str}`, emitted as `_CG_any`, and the isinstance test is
folded to the constant False. Two defects. First, the union reached
codegen, although neither member carries a runtime tag that could answer
the question. Second, the test was folded. Either a class tag must be
available to test (list and str are both heap objects), or the union must
be refused like any other unrepresentable one. Folding it is never right.

## 2. A class attribute read through a subclass after the base is mutated

`tests/class_attr_mutation.py` (`.python.expect_fail`: its `.exec.check`
bakes in pyc's answer):

```python
class A: n = 2
class B(A): o = 3
A.n = 4
print(B.n)          # CPython: 4.   pyc: 2
```

The subclass's prototype holds its own copy of `n`, taken at class
creation, so a later store to `A.n` never reaches `B.n` (or `B().n`).
Recorded in closed [003](closed/003-subclass-struct-layout-mismatch.md),
never fixed. CPython looks an unset class attribute up through the MRO at
read time. The fix is for a subclass NOT to copy an inherited class
attribute it does not assign, and to read through the base's.

## 3. A global read before its definition

`tests/scope_global_before_define.py` (a scoping test run with
`--test-scoping`, so the suite never runs the binary):

```python
class a:
  global z
  print(z)          # CPython: NameError.   pyc: prints 1
z = 1
```

pyc reads the one module-level value, which is assigned later. Same family
as [103](103-unknown-kwarg-silently-bound-positionally.md)'s executed
undefined name: a read that CPython rejects at run time must be refused, or
checked at run time. It must not see a value from the future.

## Already tracked elsewhere (for completeness)

- `list_tuple_union_method`: strict prints `[1, 2]` for `(1, 2)`
  (ifa/102; `.known_issue` at the EXEC stage).
- ~~`repr_without_str`: `<object>` for a class's `__repr__`~~ FIXED
  2026-10-03 ([123](closed/123-str-does-not-fall-back-to-repr.md)):
  `object.__str__` now calls `__repr__`.
- `lambda_class_attr`: the default `repr` of an instance (`<object>` for
  `<__main__.A object at 0x…>`). This is the residual of 123, which fixed
  the fallback but not the default text.
- `zip_builtin`, `tuple_list_mix`: `len(zip(...))` and `zip(...)[i]`
  compile, where CPython raises `TypeError`. `zip`/`map`/`filter` return
  eager lists. The author's decision (2026-09-29) is to keep the eager
  lists and not add lazy iterator objects, so this is a documented
  representation (171 #5).
- `list_index_type_mismatch_salvage` (= corpus `loop`): segfaults in
  every mode. `--strict` used to refuse it, but only because of the
  premature check fixed in 171 (see there). Triaged 2026-09-29: it is
  **C-stack exhaustion**, not a miscompile. gdb shows 16,883 frames of the
  DFS recursion, and with `ulimit -s unlimited` the binary runs to
  completion (rc 0). The program calls `sys.setrecursionlimit(100000)`,
  which pyc ignores. [025](025-shedskin-examples-coverage.md) lists `loop
  139` under ifa/102; that attribution is wrong. **Fix:** honour the
  recursion limit by running the program on a stack sized to it (e.g. the
  runtime's `main` starts a thread with a large stack), or at least turn
  the overflow into CPython's `RecursionError` rather than a SIGSEGV.

## Verification

Re-run the comparison (the script is in 171's verification record). Each
item above either matches CPython or is refused with a named diagnostic.
