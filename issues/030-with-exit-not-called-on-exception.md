# Issue 030: `with` never calls `__exit__` when the body raises

**Status:** open. Found 2026-07-20 while auditing
[closed/012](closed/012-with-statement-unimplemented.md) (`with`
statement) for staleness — its own original text anticipated exactly
this gap and deferred it pending exception support, but that
follow-up was never revisited once issue 011 (exception handling)
landed 2026-07-17, three weeks after 012 was closed.
**Affects:** `python_ifa_build_if1.cc`'s `build_if1_with_items` and
the `ctx.with_stack` cleanup emission at `PY_return_stmt`/
`PY_break_stmt`/`PY_continue_stmt` — none of these are wired into
issue 011's `raise`/unwinding machinery at all; `with`'s cleanup only
fires on normal fallthrough, `return`, and loop `break`/`continue`.
**Related:** [011](closed/011-exception-handling-unimplemented.md)
(exception handling — the unwinding mechanism this issue needs to
hook into); [012](closed/012-with-statement-unimplemented.md) (the
`with` implementation this is a follow-on gap for).

## Symptom

```python
class Trace:
    def __enter__(self):
        print("enter")
    def __exit__(self, a, b, c):
        print("exit")
        return True

def foo():
    with Trace():
        print("body")
        raise ValueError("boom")
    print("after with")

try:
    foo()
except ValueError:
    print("caught")
print("done")
```

CPython:
```
enter
body
exit
after with
done
```

pyc:
```
enter
body
caught
done
```

`__exit__` never runs, so its side effects are lost, and — worse —
its ability to *suppress* the exception by returning a truthy value
is silently ignored: real Python would print `after with` (the
exception is swallowed by `__exit__`'s `return True`), pyc instead
lets the exception propagate straight past the `with` to the outer
`try`/`except`.

## Root cause

`with`'s cleanup (`ctx.with_stack`) is only consulted at three
specific statement kinds: `PY_return_stmt`, `PY_break_stmt`,
`PY_continue_stmt` (see `python_ifa_build_if1.cc`, added in `c14a214b`
alongside the rest of `with`'s implementation, back on 2026-07-04 —
three weeks *before* issue 011's exception/unwinding mechanism
existed at all). `raise` and the exception-propagation path issue 011
later built have no knowledge of `with_stack` whatsoever; a raise
inside a `with` body just unwinds straight to the nearest handler (or
out of the function) without ever consulting the pending cleanups.
This is a structural gap, not a bug in either issue's own code taken
alone — `with` predates 011's unwinding model, and 011 was never
extended to walk `with_stack` the way it presumably already handles
`finally` blocks (needs confirming — see "Investigation notes").

## Investigation notes

Not yet checked: how issue 011's `finally` clause cleanup is
implemented internally — if `finally` already has a mechanism for
"run this code when unwinding past me, regardless of the unwind
reason," the most direct fix is very likely to route `with`'s
`__exit__` calls through that *same* mechanism (a `with EXPR: BODY`
is, after all, semantically close to
`try: BODY finally: EXPR.__exit__(...)`, modulo passing real
exception info and honoring a truthy suppress). If `finally`'s
implementation is itself statement-position-specific (only fires at
literal `try/finally` unwind points, not general stack-tracked
cleanups), this may need its own unwind-aware mechanism rather than
reusing `finally`'s as-is.

## Proposed fix sketch

1. Desugar (or directly lower) `with EXPR as VAR: BODY` closer to
   CPython's real model:
   ```python
   VAR = EXPR.__enter__()
   try:
       BODY
   except BaseException as e:
       if not EXPR.__exit__(type(e), e, e.__traceback__):
           raise
   else:
       EXPR.__exit__(None, None, None)
   ```
   — reusing issue 011's now-existing `try`/`except`/`raise` lowering
   directly, rather than hand-rolling a second cleanup-tracking
   mechanism (`ctx.with_stack`) alongside it.
2. If reusing full `try`/`except` per with-item is too heavyweight
   (extra exception-typed formal, extra dispatch), at minimum extend
   whatever internal hook issue 011's `finally` clause uses for
   "run on any unwind past this point" and register each `with_item`'s
   `__exit__` call through that hook instead of only at
   `return`/`break`/`continue`.
3. Either way, `__exit__`'s three arguments need to be the real
   exception triple on the raising path (currently always
   `sym_nil, sym_nil, sym_nil` even on the non-raising paths that do
   fire) and its return value needs to gate whether the exception
   re-raises (truthy) or is suppressed (falsy/no exception).

## Verification plan

1. The repro above: `with`'s body raises, `__exit__` runs, printed
   output shows `exit` and — since this `__exit__` returns `True`
   — `after with` prints too (exception suppressed), matching
   CPython exactly.
2. A second case where `__exit__` returns `False`/`None`: the
   exception should still propagate after `__exit__` runs.
3. `__exit__` receives the real `(type, value, traceback)` triple
   when raising, not `(None, None, None)`.
4. Nested `with`s: an inner exception unwinds through every
   enclosing `with`'s `__exit__`, innermost first, before reaching
   a `try`/`except` further out.
5. Regression: `tests/with_basic.py`, `with_break.py`,
   `with_return.py` (the non-exception paths issue 012 already
   covers) continue to pass unchanged.
6. New test: `tests/with_exception.py` + `.exec.check`, verified
   against real `python3` output.

## What this unblocks

`with` is the standard resource-management idiom in Python
specifically *because* it guarantees cleanup even when something
goes wrong (closing a file/socket/lock on an exception path). Without
this fix, `with` only behaves correctly on the happy path — any
program relying on `with` for exception-safe cleanup (the majority of
its real-world motivation) gets silently-wrong-instead-of-crashing
behavior: no diagnostic, just a resource leak or a suppressed
exception that should have propagated.
