# Issue 013: `assert` statement is unimplemented

**Status:** fixed (minimal version — see "What landed" — UPGRADED
2026-07-17: issue 011 landed, so `__pyc_assert_fail__` now raises a
real, catchable `AssertionError(msg)` instead of print+exit(1); see
issue 011 for the exception-handling implementation and the bugs it
flushed out along the way. `tests/assert_fail.py`'s exec.check
updated to match — an unhandled failed assertion now prints
"Unhandled exception: ..." via `__pyc_unhandled_exception__` rather
than "AssertionError: ...").
**Affects:** `python_ifa_build_if1.cc` (`PY_assert_stmt` in
`build_if1_pyda`); `__pyc__/05_builtins.py` (new
`__pyc_assert_fail__` helper); `ifa/codegen/cg_emit_llvm.cc` (fixed
an unrelated pre-existing bug found along the way — see below).
**Related:** issue 011 (exception handling) — a fully faithful
`assert` raises `AssertionError` on failure, which depends on
issue 011's exception model; the minimal version landed here
(abort the program) does not.

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

## What landed

Landed the minimal version exactly per the fix sketch:

1. **`__pyc__/05_builtins.py`**: new `__pyc_assert_fail__(msg)`
   helper — prints `AssertionError` (or `AssertionError: msg` if a
   message was given) via the existing `print()` builtin, then calls
   the existing `exit(1)` builtin. No new runtime primitives needed.
2. **`build_if1_pyda`'s `PY_assert_stmt` case**: lowers `assert cond[,
   msg]` to `if not cond.__pyc_to_bool__(): __pyc_assert_fail__(msg)`,
   using the same `if1_if_goto`/label pattern already used for
   `list_if`/`comp_if` (issues 008/009). The message expression is
   only built inside the false branch — never evaluated when the
   assertion passes, matching real Python's `assert cond, msg`
   semantics exactly (verified: a passing `assert True,
   side_effect()` does not run `side_effect()`). The function Sym for
   `__pyc_assert_fail__` is resolved with `make_PycSymbol(ctx, ...,
   PYC_USE)` — a plain scope lookup, safe to call from `build_if1_pyda`
   since `build_syms_pyda` has already registered every global name by
   the time any `build_if1_pyda` code runs.

**Found and fixed a pre-existing, unrelated bug along the way**: the
v2 LLVM backend's `__pyc_c_call__` lowering
(`ifa/codegen/cg_emit_llvm.cc`, `emit_send_primitive`) passed the
target function name to `get_runtime_helper` verbatim, including any
leading `::` — e.g. `exit()`'s existing implementation
(`__pyc__/05_builtins.py`) calls `__pyc_c_call__(int, "::exit", ...)`,
where `::` is a C++ global-scope qualifier meaningful only to the C
backend's generated C++ output. The LLVM backend doesn't compile
C++ — it took `"::exit"` as a literal symbol name, declaring and
calling a function that could never link (`undefined reference to
'::exit'`). This meant `exit()` — and therefore this issue's own
assert-fail path — was completely broken on the LLVM backend
(confirmed via a plain `exit(1)` call, no assert involved). Fixed by
stripping a leading `::` before passing the name to
`get_runtime_helper`.

Verified against real `python3` output on both the C and v2 LLVM
backends (`tests/assert_basic.py`): passing asserts (bare, with a
message, inside a function), and confirmed the message expression's
laziness. `tests/assert_fail.py` (+ `.check_fail`, since a failing
assert's nonzero exit code is inherently treated as a harness
failure otherwise) locks in the abort path: correct stdout content
and exit code 1 on both backends. Full suite 115/0 (was 113/0).

## Verification plan

1. `assert True` / `assert 1 == 1` are no-ops; program continues. ✓
2. `assert False` (or any falsy condition) aborts with a clear
   message including the optional message argument
   (`assert False, "boom"`). ✓ (source-location reporting deferred —
   no exception/traceback model exists yet, tracked with issue 011)
3. `assert` inside a function body, not just at module scope. ✓
4. `tests/assert_basic.py` (passing cases) and `tests/assert_fail.py`
   + `.check_fail` (failure path) added. ✓

## What this unblocks

`assert` is extremely common in Python for invariant-checking,
test code, and defensive programming, and now works (as an abort,
not yet a catchable exception) on both backends.
