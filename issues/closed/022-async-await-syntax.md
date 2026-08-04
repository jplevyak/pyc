# Issue 022: Implement `async`/`await` (PEP 492)

**Status:** **closed 2026-08-03** — feature-complete and verified
end-to-end on **both** backends, including the constant-folded-await
side-effect bug (found during this same landing, initially deferred,
then root-caused and fixed the same day — see "Constant-folded async
calls dropped their side effects" below) and `test_async_read.py`/
`test_async_sleep.py`'s missing `.exec.check` runtime verification
(see "Two older tests wired up to real `.exec.check` verification"
below). No known remaining correctness gaps. `test_async_net.py`
(a real network round-trip against `example.com` — deliberately left
compile-only, not suitable for a deterministic `.exec.check`) and
`test_async_real.py` (never wired to `_CG_run_coro`, same
never-actually-driven gap the two tests below had — not addressed
this round, out of the scope the user asked for) remain compile-only.
**Affects:** `ifa/codegen/cg.cc`, `ifa/codegen/cg_emit_llvm.cc`,
`ifa/optimize/inline.cc`, `ifa/optimize/dead.cc`,
`ifa/codegen/codegen_common.cc`, `pyc_runtime.c`.

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

## Constant-folded async calls dropped their side effects (fixed 2026-08-03)

Found while landing the fixes above (a first draft test using a
literal argument, `await step(1)`, appeared to work — printed a
plausible-looking result — but was later proven to be a compile-time
constant-fold artifact, not real execution: adding an observable
`print()` inside the awaited function showed it never actually ran).
Confirmed **async-specific**: an equivalent plain/non-async function
call with the same fully-constant arguments does *not* lose its
`print()`. Initially judged narrower/lower-impact and deliberately
deferred — but root-caused and fixed the same day once the LLVM
chaining work above was done, since it turned out to be a simple,
general compiler bug rather than anything async-architecture-specific.

### Root cause

Nothing to do with `await`'s own codegen. `ifa/optimize/inline.cc`'s
`sub_constants` (run once per live PNode across the whole program,
part of `simple_inlining`) unconditionally replaces **every** live
send's constant-valued rvals with a freshly allocated, disconnected
"bare constant" `Var` (`new_live_Var(c)`) — sound for ordinary pure
operations, where "the value" and "the effect" of evaluating an
operand are the same thing, so substituting the known value for the
real one changes nothing observable. `await`'s operand breaks that
assumption: awaiting it is what actually *runs* the coroutine's body
(including any side effects), and the coroutine's call-graph-provable
*return value* being constant says nothing about whether its body is
side-effect-free. Once `sub_constants` replaced the operand Var,
codegen was left with a bare constant (`co_await 2` — not even valid
C++, since `2` isn't awaitable) instead of the real coroutine handle;
`virtual_cg_is_const_folded_send`'s existing `P_prim_await` exemption
(added months earlier for genuinely non-call operands like `await
42`) then treated the whole send as foldable and skipped emitting
`co_await`/`llvm.coro.*` entirely — silently orphaning the coroutine
the call had already constructed, on both backends.

Traced by comparing a working (non-constant-argument) case's generated
C — `t12 = step(t13); t11 = co_await t12;`, the same `Var` used both as
the call's result and the await's operand — against the broken
(constant-argument) case, where the await's operand turned out to be a
*different* `Var` object entirely (confirmed via id/pointer
comparison): one with no `.def` and zero registered `AVar`s, i.e.
never touched by FA's own analysis at all — exactly `sub_constants`'s
`new_live_Var(c)` stand-in.

### Fix

Three coordinated pieces, all under the same "an `await` operand's
*identity*, not just its value, matters" rationale:

1. **`ifa/optimize/inline.cc`** (the actual bug): `sub_constants` now
   leaves a `P_prim_await` send's rvals untouched entirely (still
   recurses into `phi`/`phy`), keeping the operand as the real,
   call-linked `Var`.
2. **`ifa/optimize/dead.cc`** (`mark_live_avars`): backward liveness
   propagation through a live send's rvals already skipped any
   constant-valued rval (sound for the same reason `sub_constants`'
   default behavior is sound) — needed the identical `P_prim_await`
   exemption, otherwise the awaited call's own result `Var` never got
   marked live and codegen had no real C variable to declare for the
   coroutine handle regardless of (1).
3. **`ifa/codegen/codegen_common.cc`** (`virtual_cg_is_const_folded_send`):
   the pre-existing `P_prim_await` exemption (there specifically so
   `await 42`-style literal, non-call operands can still fold) now
   additionally checks that the operand does *not* derive from a real
   call (`Code_SEND` with no `prim`, i.e. `emit_send_call`'s shape) —
   with (1) and (2) in place, a real awaited coroutine's operand
   reliably keeps a `.def` pointing at its constructing call, so this
   is a simple, direct check.

All three are necessary together: without (1), the operand is gone
before (2)/(3) ever see it; without (2), the operand survives but has
no live backing variable to declare; without (3), a real coroutine
call still gets its `co_await`/`llvm.coro.*` codegen skipped whenever
its result happens to be foldable.

### Verified

Added `tests/async_const_fold.py` (+ `.exec.check`,
`.python.expect_fail`) — the same shape as the original repro
(`step(1)`, both argument and return fully compile-time-constant,
observable `print()` inside `step`), driven via `_CG_run_coro`.
Confirmed the awaited body's `print()` now runs on **both** backends,
matching `.exec.check` (`in step, x = 1\na: 2\n`). Confirmed the
original reason for `P_prim_await`'s codegen exemption still works
(`tests/test_is_const_folded.py`'s `await 42` — an actual non-call
literal operand — still compiles on both backends). Full regression
suite clean on both: `test_pyc.py` and `PYC_FLAGS=-b test_pyc.py` each
239 passed / 0 failed / 8 expected fails / 4 skipped (one more pass
than the previous landing — the new test). `ifa`'s own unit suite
(`./ifa --test`, 58 tests) also clean.

## Two older tests wired up to real `.exec.check` verification (2026-08-03)

`test_async_read.py`/`test_async_sleep.py` compiled and linked but
verified nothing at runtime: both ended with a bare `main()` call —
constructing a coroutine and immediately leaving it suspended,
un-awaited, the exact "coroutine was never awaited" shape noted at the
top of this issue — so neither test's body had ever actually executed
before this fix, regardless of anything else landed this session.

**`test_async_read.py`** had a second, independent bug: `open("tests/
async_simple.py", "r")` used a path relative to a `tests/` prefix, but
the test harness runs compiled binaries from `tests/build/`, where
`.py` sources are symlinked directly (not under a `tests/`
subdirectory) — the same latent bug `async_driven.py`'s first draft
hit and fixed during this session's earlier work, and noted at the
time as "likely present in `test_async_read.py` too." Confirmed:
fixing the path to `"async_simple.py"` was required before real,
correct output was even possible.

**Fix**: both tests' trailing bare `main()` replaced with `res =
__pyc_c_call__(int, "_CG_run_coro", int, main())` (the same drive
mechanism `test_async_net.py`/`async_driven.py`/`async_const_fold.py`
already use), plus the path fix above for `test_async_read.py`. Both
now need `.python.expect_fail` (newly using `__pyc_c_call__`, not
valid CPython syntax, matching every other driven async test's
convention).

**`test_async_sleep.py`** note: `sleep()`'s body (`async def
sleep(seconds): return seconds`) is a no-op stub — it does not use the
runtime's real timer/event-loop primitive (`pyc_runtime.c`'s
`_CG_event_loop_sleep`, confirmed present and unused by this test).
`end - start >= 0` is deliberately a monotonic-time sanity check, not
a real sleep-duration check, and was left as-is (out of the scope
asked for here) — genuinely exercising `_CG_event_loop_sleep` would
need actual timing, which is harder to make both deterministic and
fast enough for routine test runs. **Superseded** — see "Real sleep
wired up" below, landed the same day once asked to revisit it.

### Verified

Golden output captured directly from the compiled binaries (not
hand-typed, to avoid trailing-whitespace mistakes — `test_async_read
.py`'s second line is `"async def "`, ending in a real trailing
space, the literal first 10 characters of `async_simple.py`).
`test_async_read.py.exec.check`: `1\nasync def \n`.
`test_async_sleep.py.exec.check`: `True\n`. Both compile, link, run,
and match their `.exec.check` on **both** backends; both correctly
`XFAIL` the CPython-comparison stage (expected, `__pyc_c_call__` has
no CPython equivalent). Full regression suite clean on both:
`test_pyc.py` and `PYC_FLAGS=-b test_pyc.py` each 237 passed / 0
failed / 10 expected fails / 4 skipped (two more expected-fails than
the previous landing — these two tests, previously silent
"compile-only" passes, now real XFAILs with verified runtime output).

## Real sleep wired up + test_async_real.py driven (2026-08-03)

Asked to revisit the two loose ends noted just above: `sleep()`'s
no-op stub, and `test_async_real.py` never being driven. Both fixed.

### `test_async_real.py`

Same never-actually-driven shape `test_async_read.py`/
`test_async_sleep.py` had before the previous fix: a bare `main()`
constructed the coroutine and left it suspended forever. Replaced with
`res = __pyc_c_call__(int, "_CG_run_coro", int, main())`, added
`.exec.check` (`Value: 42\n`, captured from the compiled binary) and
`.python.expect_fail`. No other changes needed — `get_arg()` → 21,
`get_value` doubles it, `21 * 2 == 42`.

### Real `_CG_event_loop_sleep` support

`pyc_runtime.c` already had a complete, correct timer-queue
implementation (`_CG_event_loop_sleep` registers a wakeup time;
`_CG_event_loop_run`'s `poll()` timeout and post-poll sweep already
handle firing it) — it just had **zero callers anywhere**, C or
Python. Wired it up end to end, mirroring the existing
`__pyc_net_wait_read__`/`__pyc_net_wait_write__` pattern exactly (a
name-matched special case inside `__pyc_c_call__`'s codegen, not a new
first-class language construct):

1. **`pyc_c_runtime.h`**: new `_CG_Await_Sleep` awaiter struct (C
   backend) — `await_suspend` calls `_CG_event_loop_sleep(h.address(),
   seconds)`, structurally identical to `_CG_Await_Net_Read/Write`.
2. **`python_ifa_main.cc`** (`c_call_codegen`, C backend): a new
   `"__pyc_sleep__"` name match emits `co_await _CG_Await_Sleep{(double)
   seconds};`, alongside a `prim_reg(..., "__pyc_sleep__", ...)
   ->is_visible = 1` registration matching the two net-wait entries
   right above it.
3. **`ifa/codegen/cg_emit_llvm.cc`** (`emit_send_primitive`, LLVM
   backend): a new `"__pyc_sleep__"` branch, copying the net-wait
   branch's exact `coro.save`/`coro.suspend`/switch/resume-block
   shape, calling `_CG_event_loop_sleep(coro_hdl, seconds)` in place of
   `_CG_event_loop_register_io` before suspending (defensively
   `CreateFPExt`s to `double` if the operand isn't already one, though
   Python `float` already maps to LLVM `double` everywhere else in
   this codebase, so this is just belt-and-suspenders).
4. **`tests/test_async_sleep.py`**: `sleep()` now calls
   `__pyc_c_call__(int, "__pyc_sleep__", float, seconds)` instead of
   returning its argument untouched; `main()` sleeps `0.05` and asserts
   `end - start >= 0.05` (a one-sided bound — safe from flakiness,
   since a correct timer can only take *at least* the requested time,
   never less; scheduling jitter only ever pushes it later). Verified
   the compiled binary's real wall-clock time is ~50ms on both
   backends (`time ./test_async_sleep`), confirming this is a genuine
   suspend-and-resume-later round trip through the event loop, not a
   fast-path no-op.

### A genuine, previously-unreachable bug found along the way

`_CG_event_loop_run`'s timer-wakeup sweep (`pyc_runtime.c`) called
`free(task)` on a `_CG_TimerTask` that was `GC_MALLOC`'d, not
`malloc`'d — undefined behavior under Boehm GC (calling libc `free()`
on GC-managed memory), not merely a no-op. The two sibling task types
in the same function (`_CG_ReadyTask`, `_CG_IoTask`) already followed
the correct "GC reclaims it, don't call `free()`" convention (visible
in this file's own `/* removed free(task) */` comments) — only the
timer path was inconsistent, and since nothing had ever called
`_CG_event_loop_sleep` before this fix, this branch had literally never
executed. Fixed by removing the `free(task)` call, matching the
established convention. (`pfds` in the same function is a real, plain
`malloc()`, so its `free(pfds)` a few lines above is correct and
untouched.)

### Verified

`tests/test_async_sleep.py` and `tests/test_async_real.py` both
compile, link, run, and match their `.exec.check` on **both**
backends; both correctly `XFAIL` the CPython-comparison stage. Full
regression suite clean on both: `test_pyc.py` and `PYC_FLAGS=-b
test_pyc.py` each 236 passed / 0 failed / 11 expected fails / 4
skipped (one more expected-fail than the previous landing —
`test_async_real.py`, previously a silent "compile-only" pass, now a
real XFAIL with verified runtime output; `test_async_sleep.py`'s pass
count is unchanged, it was already counted as an expected-fail).
`ifa`'s own unit suite (`./ifa --test`, 58 tests) also clean.
`test_async_net.py` (the pre-existing consumer of the net-wait
special-case this shares codegen with) re-verified unaffected on both
backends.

## Verification plan (original — superseded by "New test coverage" above)

1. ~~async/await: minimal `async def` + `await` round-trip~~ — was
   already implemented before this issue was picked up.
2. ~~Add one test file for async/await once implemented~~ — done,
   `tests/async_driven.py`, plus the fixes above needed to make it pass
   for real rather than by accident.
