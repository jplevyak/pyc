# Issue 022: Implement `async`/`await` (PEP 492)

**Status:** open — core landed and verified end-to-end on **both**
backends 2026-08-03 (this session); one narrower, lower-impact gap
(constant-folded async calls silently drop side effects) found and
deliberately deferred, see "What's still missing" at the bottom.
**Affects:** `ifa/codegen/cg.cc`, `ifa/codegen/cg_emit_llvm.cc`,
`pyc_runtime.c`.

## Original filing (stale — kept for history)

```python
async def foo():
    return 1
```
```
async_test.py:1: syntax error after 'async'
```
No `async`/`await` keyword or rule anywhere in `python.g`.
Implementing this meaningfully requires an event-loop/coroutine runtime
model — likely the largest remaining syntax gap, probably larger in
scope than issue 014's generators (which it depends on/relates to,
since `async def` bodies are generator-like).

**This premise turned out to be completely stale.** Picked this issue
up to assess "how easy is it" and found `async`/`await` syntax already
parses, lowers, and compiles on both backends (`is_async` in `sym.h`,
`PY_await_expr` in `python_ifa_build_if1.cc`, `_CG_Coroutine` in
`pyc_c_runtime.h` for the C backend, an `llvm.coro.*`-based
implementation in `cg_emit_llvm.cc` for LLVM) — substantial prior work,
undocumented anywhere in this filing. What was actually missing: **zero
tests drove the event loop to completion.** Every existing async test
(`tests/async_simple.py`, `tests/async_suspend.py`, etc.) just
constructed a coroutine object and left it sitting uninvoked (real
Python: `RuntimeWarning: coroutine was never awaited`) — nothing had
ever verified real execution. Driving one via `_CG_run_coro` (the same
`__pyc_c_call__` pattern `tests/test_async_net.py` already used)
surfaced three real bugs, all fixed this session — see below.

## Bugs found and fixed (2026-08-03)

### 1. C backend: `co_await`'s result never cast to the awaiting variable's type

A function with 2+ sequential/chained `await` expressions failed to
compile: `incompatible pointer to integer conversion assigning to
'_CG_int64' from 'void *'`. This was an **already-documented** gap — a
comment in `cg.cc`'s `P_prim_yield` case explicitly noted "`P_prim_await`
has this same latent gap, just never hit by an existing test" — just
never actually hit until this session's new multi-await test.

**Fix** (`ifa/codegen/cg.cc`, `P_prim_await`): give it the same explicit
cast `P_prim_yield` already had —
```cpp
fprintf(fp, "%s = (%s)(uintptr_t)(co_await %s);\n", cg_get_string(n->lvals[0]), c_type(n->lvals[0]),
        cg_get_string(n->rvals[o]));
```

### 2. LLVM backend: `await` never actually resumed the awaited coroutine

The deeper bug. On the C backend, C++20's built-in awaiter protocol
(`await_suspend`/`final_suspend`) handles "resume the awaiter once the
awaited coroutine finishes" automatically via symmetric transfer — pyc
gets awaiter-chaining for free just by emitting `co_await`. The LLVM
backend has no such built-in mechanism; `cg_emit_llvm.cc`'s existing
`llvm.coro.*`-based `P_prim_await` suspended the awaiting coroutine
correctly but never told the *awaited* coroutine "resume, and when
you're done, resume me" — so a function with two or more sequential or
nested real `await`s silently stopped executing after the first
suspend point. No error, just missing output; the single-await tests
that existed happened not to notice.

**Design considered and rejected:** hand-rolling LLVM's `musttail`-based
symmetric-transfer pattern directly (closer to what C++20 does
internally) — judged too risky to get right blind, with no existing
example in this codebase to mirror.

**Design landed:** reuse the existing event loop
(`_CG_event_loop_spawn`/`_CG_event_loop_run`/`_CG_resume_coro`, already
proven by `tests/test_async_net.py`) for awaiter-to-awaited chaining —
trading one extra ready-queue round-trip for much lower implementation
risk. Needed a side-state struct (mirroring `gen_state`, the pattern
issue 014's LLVM generator work established) because LLVM's
`llvm.coro.promise` intrinsic can't be used from a separately-compiled,
generic C runtime helper (no compile-time link back to a specific
`coro.id`):

1. **`EmitCtx::async_state`** + **`async_state_struct_type()`**
   (`cg_emit_llvm.cc`): a heap-allocated `{ptr coro_hdl, i64 value, ptr
   awaiter, i64 done}` struct (`_CG_async_state` — mirrors `gen_state`'s
   shape), GC-allocated in the prologue alongside the coroutine's own
   frame, storing `coro_hdl` into field 0. `coro_return_value` now
   returns this struct's address (instead of the raw `coro.begin`
   handle) for `is_async` functions — matching `is_generator`'s existing
   handle-smuggling convention.
2. **`P_prim_await`** (`emit_send_any_prim`): before suspending, stores
   the awaiter's own `async_state` pointer into the *awaited*
   coroutine's `awaiter` field, then calls `_CG_event_loop_spawn` on the
   awaited coroutine's handle to schedule it. After resuming, loads the
   result out of the awaited coroutine's `value` field.
3. **Epilogue** (`emit_block_terminator`'s `P_prim_reply` handling for
   `is_async`): on completion, stores the return value into
   `AsyncState_Value`, marks `AsyncState_Done`, then checks
   `AsyncState_Awaiter` — if non-null, loads its `AsyncState_CoroHdl` and
   calls `_CG_event_loop_spawn` on it, waking the awaiter before falling
   through to the (issue 014's already-fixed, shared)
   `ensure_coro_suspend_destroy_bbs` cleanup path.
4. **`pyc_runtime.c`**: `_CG_run_coro` previously treated its argument
   directly as the raw coroutine handle; now must unwrap the new
   `_CG_async_state` wrapper first (`st->coro_hdl`) before handing it to
   `_CG_event_loop_run`, matching the new smuggling shape.

### 3. LLVM backend: await result not converted to its declared type

Found via a string-returning awaited coroutine, which hit an LLVM
verifier error: `Call parameter type does not match function signature!
ptr call void @_CG_write(i64 %16)`. Root cause: `put_result`'s implicit
ptr↔int conversion only fires for alloca/global-backed storage, not
plain SSA-cached values — the raw `i64` loaded from `AsyncState_Value`
was being used directly as a `ptr`-typed argument with no conversion.

**Fix**: in `P_prim_await`'s resume block, explicitly convert the loaded
`i64` to the awaiting variable's real declared LLVM type
(`sym_to_llvm_type`) — `inttoptr` if pointer-typed, `sext`/`trunc` if a
narrower integer type — before calling `put_result`.

### Debug-noise cleanup

`pyc_runtime.c`'s `_CG_resume_coro` and `_CG_event_loop_run` had debug
`printf`s (polling status, per-fd revents, resume tracing) left in from
earlier development — harmless for tests with no `.exec.check`, but
would corrupt any output-comparing test's captured stdout. Removed all
of them as part of landing the first real `.exec.check`-based async
test.

## New test coverage

Added `tests/async_driven.py` (+ `.exec.check`, `.python.expect_fail` —
the latter because it uses `__pyc_c_call__`, a pyc-only FFI primitive
with no CPython shim, matching the pre-existing convention other
`__pyc_c_call__`-using tests already use). Drives three separate
`_CG_run_coro` calls to completion:
- `sequential()`: three chained sequential `await`s (`a = await
  step(n); b = await step(a); c = await step(b)`).
- `nested()`: three levels of nested `await` (`level1` awaits `level2`
  awaits `level3`).
- `strings()`: a string-returning awaited coroutine, exercising the
  type-conversion fix.

Inputs are derived from reading this repo's own `async_simple.py`
(mirroring `test_async_read.py`'s existing file-read pattern) rather
than literal constants — see "constant-folded async calls" below for
why literal arguments would have produced a false pass.

Also removed 4 now-stale `.check_fail` markers
(`tests/async_syntax.py.check_fail`,
`tests/async_unfoldable.py.check_fail`,
`tests/test_async_read.py.check_fail`,
`tests/test_async_sleep.py.check_fail`) — all 4 corresponding tests
compile and link cleanly on both backends now; the markers predated
this session's fixes and were no longer accurate.

### Verified

`tests/async_driven.py` compiles, links, and runs correctly on **both**
backends, byte-for-byte matching its `.exec.check` (`6 7 8\n114\nas!\n`).
Full regression suite clean on both: `test_pyc.py` and `PYC_FLAGS=-b
test_pyc.py` each 239 passed / 0 failed / 7 expected fails / 4 skipped.
`ifa`'s own unit suite (`./ifa --test`, 58 tests) also clean.

## What's still missing

1. **Constant-folded async calls silently drop side effects.** Found
   while debugging (a first draft test using a literal argument,
   `await step(1)`, appeared to work — printed a plausible-looking
   result — but was later proven to be a compile-time constant-fold
   artifact, not real execution: adding an observable `print()` inside
   the awaited function showed it never actually ran). Confirmed
   **async-specific**: an equivalent plain/non-async function call with
   the same fully-constant arguments does *not* lose its `print()` (verified
   with a standalone repro). Root cause not investigated further — FA's
   constant-propagation is presumably folding the whole call (including
   the coroutine machinery and its side effects) down to its provably-
   constant result. Judged narrower and lower real-world impact than the
   chaining bug (async functions called with fully compile-time-constant
   arguments and no side effects observed elsewhere are an unusual
   shape) and deliberately left unfixed this round — worth a dedicated
   pass if it turns out to matter. `tests/async_driven.py` was
   specifically designed (file-read-derived inputs) to avoid this and
   exercise the *real* coroutine machinery.
2. `test_async_read.py`/`test_async_sleep.py` have no `.exec.check` —
   they compile and link (confirmed) but nothing verifies their runtime
   output. Not addressed this round; `async_driven.py` covers the same
   underlying mechanisms (file I/O via `read_seed`, chained awaits) with
   real output verification instead.

## Verification plan (original — superseded by "New test coverage" above)

1. ~~async/await: minimal `async def` + `await` round-trip~~ — was
   already implemented before this issue was picked up.
2. ~~Add one test file for async/await once implemented~~ — done,
   `tests/async_driven.py`, plus the fixes above needed to make it pass
   for real rather than by accident.
