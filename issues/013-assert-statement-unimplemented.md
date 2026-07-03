# Issue 013: `assert` statement is unimplemented

**Status:** open.
**Affects:** `python_ifa_build_if1.cc:1272-1274` (`PY_assert_stmt`
in `build_if1_pyda`).
**Related:** issue 011 (exception handling) — a fully faithful
`assert` raises `AssertionError` on failure, which depends on
issue 011's exception model; a minimal `assert` (abort the
program) does not.

## Symptom

The grammar (`python.g:153-155` `assert_stmt`) parses `assert`
without error; the frontend rejects it with its own distinct
message (separate from the general "statement not supported"
bucket, suggesting it was already flagged as a near-term target):

```python
assert 1 == 1
print("ok")
```

```
fail: error line 1, 'assert' not yet supported
```

No test in `tests/*.py` exercises `assert` (confirmed via `grep`
across the corpus) — total, cleanly-rejected gap.

## Root cause

```cpp
case PY_assert_stmt:
  fail("error line %d, 'assert' not yet supported", ctx.lineno);
  return -1;
```

No lowering exists at all — this is the smallest of the
missing-statement issues filed in this batch.

## Proposed fix sketch

This is a good first target among the missing-statement issues —
much smaller than exception handling or `with`:

1. **Minimal version** (no exception dependency): `assert COND[,
   MSG]` lowers to something like an IF1 conditional that, when
   `COND` is falsy, calls a runtime abort helper
   (`__pyc_assert_fail__` or similar, printing `MSG` if given and
   the source location) — essentially C's `assert()` semantics,
   which the runtime already leans on internally
   (`assert(!"runtime error: ...")` appears throughout the
   generated C for other unimplemented paths).
2. **Faithful version** (once issue 011 lands): raise
   `AssertionError(MSG)` instead of aborting, so it's catchable
   like real Python `assert`.
3. Python also supports disabling asserts entirely under `-O`;
   out of scope for a first pass, but worth a one-line note in
   the follow-up doc if/when `-O`-equivalent flags are considered
   for pyc.

## Verification plan

1. `assert True` / `assert 1 == 1` are no-ops; program continues.
2. `assert False` (or any falsy condition) aborts with a clear
   message including the source location; ideally including the
   optional message argument (`assert False, "boom"`).
3. `assert` inside a function body, not just at module scope.
4. Add `tests/assert_basic.py` — likely as a `.check_fail`-style
   test (assert-failure paths are expected to abort, similar to
   the existing `tests/negative_missing_c_call.py.check_fail`
   pattern) plus a passing-assert companion test.

## What this unblocks

`assert` is extremely common in Python for invariant-checking,
test code, and defensive programming; it's also one of the
simplest gaps in this batch to close, making it a good first
target among the missing-statement issues.
