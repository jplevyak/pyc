# Issue 030: `with` never calls `__exit__` when the body raises

**Status:** **closed 2026-08-03** — fixed via option 1 of the
proposed fix sketch (direct IF1-level desugaring, not literal
try/except AST synthesis). See "Fix landed" below.
Found 2026-07-20 while auditing
[closed/012](012-with-statement-unimplemented.md) (`with`
statement) for staleness — its own original text anticipated exactly
this gap and deferred it pending exception support, but that
follow-up was never revisited once issue 011 (exception handling)
landed 2026-07-17, three weeks after 012 was closed.
**Affects:** `python_ifa_build_if1.cc`'s `build_if1_with_items` and
the `ctx.with_stack` cleanup emission at `PY_return_stmt`/
`PY_break_stmt`/`PY_continue_stmt` — none of these are wired into
issue 011's `raise`/unwinding machinery at all; `with`'s cleanup only
fires on normal fallthrough, `return`, and loop `break`/`continue`.
**Related:** [011](011-exception-handling-unimplemented.md)
(exception handling — the unwinding mechanism this issue needs to
hook into); [012](012-with-statement-unimplemented.md) (the
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

## Fix landed (2026-08-03)

**"Investigation notes" resolved first:** checked how `finally` is
implemented (`PY_try_stmt` in `python_ifa_build_if1.cc`) before
writing any code. It's built entirely inline and statement-position-
specific — a local `Lresume`/`Ldispatch` label pair and `try_stack`
frame scoped to that one AST node — there is no general, reusable
"run this on any unwind past this point" hook to plug `with` into.
That ruled out the fix sketch's option 2 (extend finally's existing
hook) and confirmed option 1 (desugar `with` to reuse the *mechanism*
`try`/`except` already has, i.e. `ctx.try_stack`/`exc_slot`/
`goto_exc_target`/`emit_exc_check`) was the only one actually
available, done as **direct IF1 construction** rather than literal
AST-to-AST synthesis — this codebase has no precedent for the latter
(every construct lowers straight to IF1 in its own `build_if1_pyda`
case), and issue 014's `yield from` had already established the exact
template for a try/except-shaped desugaring built this way (a real
`ctx.try_stack` frame + `emit_exc_check`/`goto_exc_target`, mirrored
here almost line-for-line).

### Design

`build_if1_with_items` (`python_ifa_build_if1.cc`) now wraps each
with-item's body/nested-items in a real `ctx.try_stack` frame before
recursing, exactly `PY_try_stmt`'s own approach — so a `raise`
anywhere inside (direct, or via any nested call's post-call
`emit_exc_check`) routes to a new `Ldispatch` label instead of
propagating untouched:

```
cm = EXPR.__enter__() result already bound (unchanged)
try_stack.push({Ldispatch, fun})
<recurse into nested with-items / body>
try_stack.pop()
# normal completion (no exception): unconditional __exit__(None, None, None), unchanged
goto Ldone

Ldispatch:
  exc = __pyc_exc__; __pyc_exc__ = None      # save + clear
  suppress = cm.__exit__(None, exc, None)     # real exception VALUE passed
  if to_bool(suppress): goto Ldone
  __pyc_exc__ = exc                           # restore
  goto <outer exc target, computed AFTER this item's try_stack pop>

Ldone:
```

`ctx.with_stack` (the pre-existing mechanism `return`/`break`/
`continue` use) is left completely untouched, running alongside the
new `try_stack`-based path — the two mechanisms track the same
nesting for different unwind reasons, exactly as `try_stack` and
`with_stack` already coexisted independently before this fix.
Recursion naturally gives correct nested-`with`/comma-form ordering
(innermost `__exit__` first) for free, the same way `PY_try_stmt`'s
own nesting does, with no extra bookkeeping.

**Deliberately out of scope** (matching the fix sketch's own "either
way" framing on this point): `__exit__`'s `type`/`traceback`
arguments stay `sym_nil` always — pyc's exception model has no
type()-of-instance or traceback-object concept to construct them
from; only `value` (the real exception instance) is passed, which is
what every practical `__exit__` implementation actually inspects.
`__exit__` itself raising is not specially chained (no post-call
check on that call) — unchanged from the pre-existing normal-
completion `__exit__` call just above it, which never checked either.

**No syms-pass change needed.** Checked whether `with` needs to arm
`pyc_program_has_raise` (`python_ifa_build_syms.cc`) the way issue
014's `yield`/`yield from` had to — it doesn't: unlike a generator's
`StopIteration` (raised from *builtin* module code invisible to the
"any user-level raise" scan), `with`'s new dispatch relies on
`emit_exc_check` firing for calls *inside the user's own with-body*,
and any user-level `raise`/`assert` anywhere in the reachable program
already arms the gate globally (confirmed `PY_try_stmt`/`PY_with_stmt`
themselves don't arm it either, in `build_syms_pyda` — same
"nothing to catch if nothing can raise" reasoning already applies to
plain `try`/`except`, `with` just needed to match it).

### Verified

All six verification-plan items, each checked against real `python3`
output on **both** backends:
1. The issue's own repro (`__exit__` returns `True`) — `enter / body /
   exit / after with / done`, byte-for-byte.
2. `__exit__` returns `False` — exception still propagates to the
   outer `try`/`except`, `__exit__` still ran first with the real
   exception value.
3. `__exit__` receives the real exception *value* (confirmed:
   `print("exit", b)` inside `__exit__` prints the actual message,
   not `None`) — `type`/`traceback` remain `None` per the documented
   scope limit above.
4. Nested `with`s: innermost `__exit__` runs first, then each
   enclosing one, before reaching the outer `try`/`except` — verified
   3 levels deep (2 nested `with`s + outer `try`).
5. Comma-form `with A, B:` additionally verified: when the inner
   item's `__exit__` suppresses, the outer item's `__exit__` still
   runs normally afterward with `(None, None, None)` (its own child
   block completed "normally" from its perspective) — matches
   CPython exactly.
6. `tests/with_basic.py`, `with_break.py`, `with_return.py` (the
   non-exception paths) pass unchanged on both backends.
7. New test `tests/with_exception.py` + `.exec.check` added, covering
   suppress / propagate / nested / comma-form-inner-suppress in one
   file, byte-for-byte matching real `python3` on both backends (no
   `.python.expect_fail` needed — plain Python, no pyc-only FFI).

Full regression suite clean on both: `test_pyc.py` and `PYC_FLAGS=-b
test_pyc.py` each 238 passed / 0 failed / 11 expected fails / 4
skipped (one more pass than before — the new test). `ifa`'s own unit
suite (`./ifa --test`, 58 tests) also clean. Grepped the shedskin
corpus for `with` usage — zero hits, so no corpus regression surface
for this change.
