# Issue 011: Exception handling (`try`/`except`/`finally`/`raise`) is unimplemented

**Status:** open.
**Affects:** `python_ifa_build_if1.cc:1261-1269` (`build_if1_pyda`
— `PY_raise_stmt`, `PY_try_stmt`, `PY_except_clause`,
`PY_except_handler`, `PY_finally_clause` all hit the same
`fail(...)`).
**Related:** issue 012 (`with` statement — same `fail()` call
site, different feature); `issues/closed/005-while-true-fa-crash.md`
mentions a fib-heap idiom (`while True: ... break`) that had to
route around a *different* crash — exception handling was never
attempted in that work because it hits this wall immediately.
`closed/013-assert-statement-unimplemented.md` — `assert` landed a
minimal "abort the process" version explicitly *not* depending on
this issue; once this lands, `__pyc_assert_fail__`
(`__pyc__/05_builtins.py`) should be upgraded to raise a catchable
`AssertionError(msg)` instead of printing + `exit(1)`.

## Symptom

The grammar (`python.g:182-188` `try_stmt`, `:116` `raise_stmt`)
parses `try`/`except`/`finally`/`raise` without error, but the
frontend lowering rejects all of them identically:

```python
try:
    x = 1
except Exception:
    x = 2
print(x)
```

```
fail: error line 1, statement not supported in pyda path
```

Same message, same line, for a bare `raise ValueError("bad")`.

No test in `tests/*.py` exercises `try`/`except`/`raise`/`finally`
(confirmed via `grep` across the corpus) — this is an intentional,
total, cleanly-rejected gap, not a partially-working feature.

## Root cause

`build_if1_pyda` has no case for any of the exception-handling AST
kinds; they're grouped into one `fail()`:

```cpp
case PY_yield_stmt:
case PY_raise_stmt:
case PY_try_stmt:
case PY_except_clause:
case PY_except_handler:
case PY_finally_clause:
case PY_with_stmt:
case PY_with_item:
  fail("error line %d, statement not supported in pyda path", ctx.lineno);
  return -1;
```

There is no exception-object representation, no unwinding model,
and no IF1-level construct for "this send may transfer control to
a handler" in the current IR — this is a from-scratch subsystem,
not a small gap.

## Proposed fix sketch

This is the largest of the "missing core feature" issues filed in
this batch; a real implementation needs decisions at multiple
layers:

1. **IF1/runtime representation**: pyc has no exception object
   type today. Needs a base `Exception`-like class in `__pyc__/`
   plus a runtime unwind mechanism. Two common compiler strategies:
   - **setjmp/longjmp-style** in the C backend (simplest to bolt
     onto the existing direct-C codegen; matches the "value type /
     no GC pause" style of the rest of the runtime), or
   - **status-code threading** (each call site checks a
     "did this raise" flag/return-channel and early-returns up the
     call chain) — more explicit in the generated code, easier to
     reason about for FA, but touches every call site's codegen.
2. **Frontend lowering**: `try`/`except`/`finally` need to become
   IF1 constructs FA can reason about — at minimum, the `except`
   body must be reachable from any send inside the `try` body (a
   different control-flow shape than the current straight-line
   Code_SEQ/Code_IF/Code_GOTO model), which likely needs new
   groundwork in `ifa/optimize/cfg.cc`/`ifa/if1/pnode.h` before
   FA/codegen can process it at all.
3. **`raise`** needs to at least support re-raising the currently
   propagating exception (bare `raise` inside an `except` block)
   and constructing+raising a new exception instance.
4. Given the scope, consider landing a **minimal first slice**:
   `try`/`except Exception:`/`finally` with a single unconditional
   handler (no exception-type matching, no `except as e:` binding)
   before tackling typed multi-clause `except` matching.

## Verification plan

1. Minimal: `try: risky() except: handle()` runs the handler when
   `risky()` (implemented as `raise ValueError()` or similar)
   raises, and skips it otherwise.
2. `finally` runs on both the exception and no-exception paths.
3. `except Exception as e:` binds the exception object.
4. Re-raise (`raise` with no argument inside an `except` block).
5. Add `tests/exception_basic.py`, `tests/exception_finally.py`,
   `tests/exception_reraise.py` + `.exec.check` files — currently
   zero coverage.

## What this unblocks

Exception handling is fundamental to idiomatic Python — file I/O,
dict/list bounds checking, parsing, and virtually all
error-handling code relies on it. It's likely the single largest
category of "why won't my ported script compile" for real-world
Python code.
