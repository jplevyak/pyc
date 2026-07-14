# Issue 014: Generators (`yield`) are unimplemented

**Status:** core landed and working 2026-07-14, including scalar
*and* list generator arguments, interleaved manual `next()` calls, and
`.send()` (C backend only — LLVM is a known, explicit gap, see "What's
still missing" below). `def gen(): yield 1; yield 2`, loop-embedded
`yield` (`for i in range(n): yield i*i`), argument-taking generators
(`def counter(start, stop): ...`, `def codes(s): for c in s: yield
ord(c)`, `def echoer(vals): for v in vals: yield v`), bare repeated
`.__next__()` calls outside any `for` loop, mixing direct `.__next__()`
with a `for` loop on the same instance, and `.send(v)` driving a
running accumulator (`x = yield total; total += x`) — all produce
correct output matching CPython, verified end-to-end, committed as
`tests/generator_basic.py`. A list-argument dangling-capture bug
(found while adding argument support) and a `.send()`-breaking FA
constant-collapse bug (found while adding `.send()`) were both
root-caused and fixed the same day — see "What's still missing",
items 2 and 3. `yield from`, generator expressions (issue 008), and
infinite-loop (`while True:` with no `break`) generator bodies remain
unimplemented/broken.
**Affects:** `python_ifa_build_if1.cc:1261` (`PY_yield_stmt` in
`build_if1_pyda`, shares the `fail()` with issue 011's exception
kinds); `python_ifa_build_if1.cc:1319-1327` (`PY_yield_expr` falls
into the silent default-recursion case — see note below, this is
actually a second, quieter bug).
**Related:** issue 008 (generator *expressions*, `(x for x in y)`
— a different syntax form that also needs a lazy-iterator
mechanism; likely shares the eventual generator-state-machine
implementation with this issue once one exists).

## Symptom

`yield` as a statement is cleanly rejected:

```python
def gen():
    yield 1
    yield 2
for v in gen():
    print(v)
```

```
fail: error line 2, statement not supported in pyda path
```

No test in `tests/*.py` exercises generator functions (the one
`yield` hit in the corpus, in `tests/fibheap_full.py`, is inside a
comment, not executable code — confirmed by inspection).

**Second, quieter bug**: `yield` as an *expression* (`x = yield
foo`, used for generator `.send()` semantics) is not in the
`fail()` list at all — `PY_yield_expr` falls into the generic
default case (`python_ifa_build_if1.cc:1319-1327`) that just
recurses into children with no codegen, the same silent-gap shape
as issue 008's `PY_set`/`PY_genexpr`. This means `x = yield foo`
likely crashes the same way (`if1_move` assertion, per issue 008)
rather than hitting the clean `PY_yield_stmt` message — worth
confirming with a targeted repro when this issue is picked up, and
folding into the same fix (or filing as a sub-crash if the
generator statement fix doesn't naturally cover the expression
form too).

## Root cause

`yield`-as-statement:

```cpp
case PY_yield_stmt:
  ...
  fail("error line %d, statement not supported in pyda path", ctx.lineno);
  return -1;
```

Generators require the function to be resumable — the current
compilation model (each `def` lowers to one closure invoked
start-to-finish per call) has no notion of suspending execution
mid-function and resuming later with local state preserved. This
is a fundamentally different execution model than everything else
pyc currently compiles, and is the most involved feature in this
filing batch.

## Proposed fix sketch

Two standard implementation strategies, both substantial:

1. **State-machine transform** (classic compiler approach): lower
   the generator function body into an explicit state machine —
   each `yield` becomes a state boundary; locals live in a
   heap-allocated frame struct (the GC already backs pyc's object
   model, so this fits); resuming means dispatching into the
   right state based on a saved program-counter-like field. This
   is a frontend/IF1-level transform done *before* FA sees the
   function, so FA and codegen don't need new primitives — the
   generator becomes an ordinary object with a `__next__` method
   that's really "run until next yield or return."
2. **Stackful coroutines** (fiber/ucontext-style): give each
   generator instance its own stack and use `makecontext`/
   `swapcontext` (or a hand-rolled equivalent) to suspend/resume.
   Simpler to reason about for arbitrary control flow (yield
   inside nested loops/conditionals "just works" since it's real
   stack unwinding), but adds a runtime dependency and a
   per-generator stack allocation cost, and interacts awkwardly
   with GC stack-scanning if the collector needs to see live
   pointers on generator stacks.

(1) is more in keeping with pyc's existing "everything is ordinary
IF1 + FA-specialized C/LLVM" architecture and is the recommended
starting point; (2) is a fallback if the state-machine transform
proves too disruptive to FA's existing CFG/SSU assumptions
(`ifa/optimize/cfg.cc`, `ifa/optimize/ssu.cc`).

Either way this is large enough that it likely deserves its own
design doc before implementation starts, similar in spirit to
`ifa/issues/030`'s design-first approach for polymorphic dispatch.

## Verification plan

1. Basic generator: `def gen(): yield 1; yield 2` iterated with
   `for v in gen(): print(v)` prints `1\n2`.
2. Generator with loop-embedded `yield` (`for i in range(3): yield
   i`).
3. Generator state persists correctly across multiple `next()`
   calls interleaved with other code (not just a `for` loop
   drained in one shot).
4. `StopIteration` (or pyc's equivalent iterator-protocol
   end-of-sequence signal — check how `__pyc__/00_runtime.py`'s
   `__base_iter__` signals exhaustion today) fires correctly when
   the generator function returns.
5. Generator expression (`(x for x in y)`, issue 008) reuses
   whichever mechanism lands here.
6. Add `tests/generator_basic.py`, `tests/generator_loop.py` +
   `.exec.check` — zero coverage today.

## What this unblocks

Generators are a core Python idiom for lazy sequences, pipelines,
and memory-efficient iteration; this is likely the single largest
undertaking among the "missing core feature" issues filed in this
batch, and worth scoping as its own project phase rather than a
quick fix.

## Spike findings (2026-07-14)

Pursued per the user's direction to use "the state-machine
transformation approach." Investigation first established that pyc's
existing `async`/`await` support is **not** a frontend IR rewrite —
FA/CFG/SSU treat an `async def` as an ordinary function; the entire
suspend/resume mechanism is a C++20 coroutine (`_CG_Coroutine` in
`pyc_c_runtime.h`) synthesized by the C++ compiler itself, triggered
purely by `co_await`/`co_return` appearing in the generated C++. This
made a from-scratch, hand-rolled frontend state machine (this issue's
original option 1) unnecessary — the *C++ compiler's own coroutine
transform* is the state machine, and pyc only needs to steer codegen
into emitting it. This is architecturally much smaller than either
option this issue originally sketched.

### Design landed

1. **`is_generator`** (`ifa/if1/sym.h`/`sym.cc`): a `Sym` flag mirroring
   `is_async`, set in `python_ifa_build_syms.cc`'s `def_fun_pyda` by
   scanning a funcdef's own body for `PY_yield_stmt`/`PY_yield_expr`
   (stopping at nested def/lambda/class boundaries — a function is a
   generator iff *its own* body yields, no separate keyword unlike
   `async def`).
2. **`yield` lowering** (`python_ifa_build_if1.cc`, `PY_yield_stmt` /
   `PY_yield_expr`): a new `"yield"` primitive send, structurally
   identical to `PY_await_expr`'s existing `"await"` primitive send.
   Required a new primitive registration end-to-end, hand-edited
   (matching how `await`/`is`/`copy` were added before it —
   `ifa/prim_data.dat` is stale/unmaintained despite
   `ifa/PRIMITIVES.md` prescribing it as the source of truth; the
   *actual* practice in this codebase is hand-editing
   `ifa/if1/prim_data.{h,cc}` directly, see `P_prim_yield = 59`) plus
   an FA transfer function (`ifa/analysis/fa.cc`, mirrors
   `P_prim_await`'s `flow_vars`).
3. **Two-`Fun` split** (`python_ifa_build_if1.cc`'s `PY_funcdef` case,
   where the existing "public name is an ordinary variable, the
   function body is a separate internal Sym" split — issues/007 —
   already lives): a generator's *coroutine body* (`ast->sym`, the
   user's real yield-containing code) is kept separate from a
   *synthesized wrapper* bound to the public name. The wrapper is
   built via direct IF1 construction (`new_fun` + `if1_send1` +
   `if1_closure`, the same primitives `gen_class_pyda` uses to build
   a class's `__new__` wrapper — no working example of a *non-class*
   synthetic wrapper existed to copy, `PycCompiler::default_wrapper`
   in `python_ifa_sym.cc` came closest but is a **post-hoc** pass
   run after the whole program's IF1 already exists; calling its
   `install_new_fun` step **mid-pass**, inside the same
   `build_if1_pyda` walk, hit `if1/if1.cc:530`'s
   `assert(!c->flattened)` — double-flattening, because the
   already-scheduled whole-program finalization pass
   (`if1_finalize_flatten_and_fixup_nesting`) flattens every
   registered closure once on its own. **Do not call
   `install_new_fun` for a Fun synthesized during the normal
   per-module build_if1_pyda walk** — it's for late-arriving,
   post-finalization Funs only). The wrapper: calls the coroutine
   body, wraps its raw handle in a `__pyc_generator__` instance
   (`__pyc__/09_generator.py`, modeled directly on `__pyc_file__`'s
   FILE\*-smuggled-through-int64 pattern in `07_file.py`), returns
   it. This keeps `PY_for_stmt`'s existing generic
   `__iter__`/`__pyc_more__`/`__next__` dispatch **completely
   unmodified** — `for v in gen():` just sees an ordinary object of
   an ordinary class; no special-casing needed there.
4. **Codegen** (`ifa/codegen/cg.cc`): `write_c_fun_proto` forces an
   `is_generator` Fun's C return type to a plain `_CG_int64`
   (mirroring the `_CG_Coroutine` override `is_async` already gets),
   *not* `_CG_Generator` — the real coroutine mechanics are wrapped
   in a local, immediately-invoked lambda inside the function body
   (`write_c`: `auto __coro_1014 = [&]() -> _CG_Generator { <normal
   PNode-walk output, containing co_yield/co_return> }; return
   (int64)(uintptr_t)__coro_1014().handle.address();`). This confines
   *all* the C++-coroutine-specific text generation to one fixed
   template and keeps the function's declared signature, and hence
   every caller's view of it, completely ordinary — no `coro_vars`-
   style call-site type overriding needed anywhere else, unlike
   async's `is_async` handling. `pyc_c_runtime.h` gained
   `_CG_Generator` (a `_CG_Coroutine`-like promise type using
   `yield_value`/`suspend_always` at every yield point, no event-loop
   integration needed — driven synchronously by `.resume()`) and two
   int64-in/int64-out helpers, `_CG_generator_advance`/
   `_CG_generator_value`, in the same "smuggle a C++ handle through a
   plain int" style as every other opaque-handle runtime helper.

### Two genuine pre-existing compiler bugs found and fixed along the way

1. **`mark_live_code` didn't know `yield` has essential side effects**
   (`ifa/optimize/dead.cc`, two near-identical sites): a `P_prim_await`
   send is hard-coded live regardless of whether its result is
   consumed, because suspending matters even when the awaited value
   is discarded. `P_prim_yield` needed the identical treatment (its
   result is *always* discarded in this issue's v1 scope, no
   `.send()`) — without it, DCE stripped the entire coroutine body as
   dead code the moment its yields' results went unused, which they
   always do in v1 scope.
2. **`simple_inlining` doesn't know a coroutine can't be inlined**
   (`ifa/optimize/inline.cc`): a generator's coroutine body, once kept
   alive by fix #1, looked like a textbook trivial single-call-site
   inlining candidate (the synthesized wrapper calls it exactly
   once) and got merged into the wrapper — destroying the function
   boundary a C++20 coroutine depends on entirely (the merged
   function had `co_yield` in a body cg.cc never marked as a
   coroutine, so it wasn't `_CG_Generator`-returning and didn't
   compile). Fixed by excluding `is_generator` (and, for the same
   latent reason, `is_async` — untested, but the identical
   architectural risk applies) Funs from `inline_single_sends`'s two
   candidate-collection loops. **This is a real, previously-unknown
   risk for `async` too** — no existing async test happens to shape a
   trivial-single-call-site async function, so it was never
   triggered, but the guard is cheap insurance either way.

### Two more bugs found and fixed getting to a working end-to-end case

With the two bugs above fixed, the coroutine body and the wrapper
both compiled as separate, correct-*looking* functions, but produced
wrong runtime results (and, before the second fix below, a segfault).
Both were tracked down and fixed:

**1. The handle value got lost (constant-folded to 0).** The
coroutine body's default/fall-through reply value
(`python_ifa_build_syms.cc`'s `gen_fun_pyda`) originally used
`int64_constant(0)` — a literal FA constant — as a type-anchor for
functions with no explicit `return` (the v1 test case, `def gen():
yield 1; yield 2`, has none). FA correctly, faithfully propagated
that this Fun's return value is the *compile-time constant* 0 to
every caller, including the synthesized wrapper — so `handle_result`
(the wrapper's local holding "the coroutine body's call result") was
treated as constant 0 too, and everything downstream constant-folded
accordingly (`__pyc_generator__.handle` always 0,
`_CG_generator_advance(0)`/`_CG_generator_value(0)` everywhere). This
is different from `is_async`, which never hits this: an async
function's `fn->ret` is whatever its real `return X` computes (a
genuine non-constant value for any non-trivial function), so async's
C-type override (`_CG_Coroutine`, divorced from FA's inferred type)
never collides with FA's own constant-propagation this way.
is_generator's placeholder-return design introduced a divergence
between "what FA thinks this function returns" and "what it actually
returns at runtime" that's *categorically* wider than async's (a fake
type standing in for a real handle, not just a differently-typed real
value) — and a literal constant is exactly the shape FA is built to
propagate aggressively.

**Fix:** route the placeholder through a genuine opaque C call
instead of a literal constant — the same IF1 shape
`__pyc_c_call__(int, "name", ...)` produces from Python source
(`PY_power`'s `sym___pyc_c_call__` case in `python_ifa_build_if1.cc`
was the working reference to copy), calling a new no-op C helper,
`pyc_c_runtime.h`'s `_CG_generator_placeholder_return()`. FA anchors
the int64 *type* from this without believing it knows the *value*
(same mechanism `_CG_fopen`'s int64 result relies on — a real,
opaque, foreign call, never constant-folded). This alone fixed the
handle-value propagation completely: `__pyc_more__`/`__next__`
correctly compiled *with* a `self` parameter and read the real
`handle` field through it — the "self-parameter collapses for a
single-creation-site type" concern raised in the prior version of
this section turned out to be a symptom of the same constant-folding
bug, not a second, independent issue.

**2. Dangling reference in the coroutine's captured locals
(segfault).** Once the handle flowed correctly, the compiled program
segfaulted on the *first* resume (from `__pyc_more__`, called well
after `gen()`'s own C stack frame had already returned). Root cause:
`write_c`'s lambda-wrap (`auto __coro_1014 = [&]() -> _CG_Generator {
... }`) captured the coroutine's locals **by reference** from the
*outer* function's stack frame. `_CG_Generator::promise_type
::initial_suspend()` returns `std::suspend_always`, so the outer
function constructs the suspended coroutine and returns immediately
— by the time anything resumes the coroutine (from a later, unrelated
call), the outer function's stack frame is long gone, and the
by-reference-captured locals are dangling. **Fix:** move every local
variable declaration *inside* the lambda (`ifa/codegen/cg.cc`'s
`write_c`) instead of capturing them from outside, and switch the
capture list from `[&]` to `[=]` (value capture — matters once
generator arguments are supported; today's v1 scope is 0-arg so
there's nothing to capture yet either way). Locals declared *inside*
a coroutine function are correctly promoted to its own
heap-allocated frame by the C++ compiler, so they survive suspension
like any other coroutine-local state.

### Verified working end-to-end

`tests/generator_basic.py`: `def gen(): yield 1; yield 2`,
loop-embedded `yield i*i` over `range(n)`, a two-scalar-argument
generator with a `while` loop (`counter(start, stop)`), and a
string-argument generator iterated char-by-char (`codes(s): for c in
s: yield ord(c)`) — all driven by `for`, compile and run correctly on
the C backend, byte-for-byte matching real `python3` output.
Committed with `tests/generator_basic.py.exec.check` (expected
output) and `tests/generator_basic.py.check_fail` (documents the
expected LLVM-backend link failure — `make test` runs both backends
unconditionally and this test is C-only by design). Multiple
concurrent generator instances with different arguments, driven by
direct `.__pyc_more__()`/`.__next__()` calls rather than `for`, were
also verified to keep independent, correctly-parameterized state (not
committed as a test — exercises the documented "must alternate
more/next one-for-one" caveat, not a`for` loop). Full regression suite
(191 e2e + 58 unit + 16 IFA-phase tests) passes with every change from
this issue in place — all bug fixes (dead-code elimination, inlining,
constant placeholder, dangling capture) and the new is_generator
machinery are additive/gated and introduce no regressions elsewhere.

### What's still missing

1. **LLVM backend.** This work targeted the C++20-coroutine C backend
   only, per the approved plan. `cg_emit_llvm.cc` has no
   `is_generator` handling and `pyc_runtime.c` has no
   `_CG_generator_*`/`_CG_prim_yield` — `pyc -b` fails to link on any
   generator (`tests/generator_basic.py.check_fail` documents this).
2. ~~List-typed generator arguments are broken when iterated.~~
   **Root-caused and fixed 2026-07-14.** Not an argument-forwarding
   or FA/clone bug at all — it was a dangling-capture bug in the
   coroutine codegen shape itself, `ifa/codegen/cg.cc`'s `write_c`
   (the `is_generator` lambda-wrap block). When a **lambda's**
   `operator()` is itself a C++20 coroutine (triggered by containing
   `co_yield`/`co_return`), the standard specifies that `[=]`-captured
   values live in the **closure object** (the lambda variable itself),
   not in the coroutine frame — the frame only stores an implicit
   pointer back to the closure. The codegen declared the closure as a
   plain stack-local (`auto __coro_1014 = [=]() -> _CG_Generator
   {...};`), which is destroyed the instant the outer (wrapper-calling)
   function returns — and because `_CG_Generator`'s `initial_suspend()`
   is `std::suspend_always`, that return happens immediately, before
   the coroutine body ever runs. Every later resume (from
   `__pyc_more__`, on a completely different call stack) then read
   captured parameters through a dangling closure pointer. Scalar
   (int) arguments "worked" only by luck — the stale stack memory
   often hadn't been overwritten yet by the time of first resume in
   small test programs; struct/list-pointer arguments reliably did,
   because more intervening calls (list `__iter__`/`__new__`, etc.)
   ran between construction and first resume, clobbering the stack
   slot first. Confirmed with a minimal, pyc-independent C++ repro
   (outer function taking a struct pointer, capturing it `[=]` into an
   immediately-invoked coroutine lambda) that reproduced the garbage
   read outside of pyc entirely, and confirmed fixed by
   heap-allocating the closure (`auto *__coro_1014 = new
   auto([=]()... {...}); ... (*__coro_1014)();`) so the object the
   frame's implicit pointer refers to survives indefinitely, the same
   way the coroutine frame itself already does. `tests/generator_basic
   .py` now includes a list-argument case (`echoer(vals): for v in
   vals: yield v`), verified byte-for-byte against `python3`.
3. ~~`.send()` / interleaved manual `next()` calls.~~ **Landed
   2026-07-14.** Three independent pieces, all needed together:
   - **Grammar**: `x = yield foo` (yield as the RHS of a plain
     assignment, no parens) didn't parse at all — `expr_stmt`'s `'='`
     alternative only accepted `testlist`, and `yield_expr` was only
     reachable inside `atom`'s `LP yield_expr RP` form. Added a
     dedicated `testlist '=' yield_expr` alternative to `expr_stmt`
     (`python.g`) parsing straight to `PY_assign`; `PY_assign`'s
     existing `build_syms_pyda`/`build_if1_pyda` handling is already
     generic over its value child's kind, so nothing downstream needed
     to change once this could parse.
   - **Frontend**: `PY_yield_expr` (`python_ifa_build_if1.cc`) used to
     hardcode `ast->rval = sym_nil` unconditionally, discarding the
     "yield" primitive send's own result Sym — the send happened (so
     `co_yield` still ran) but nothing could ever read what came back.
     Now wires `ast->rval` to that result Sym directly.
   - **FA**: naively flowing the yielded value's own type into that
     result Sym (`P_prim_yield`'s transfer function in
     `ifa/analysis/fa.cc`, previously identical in shape to
     `P_prim_await`'s) is unsound the moment a generator's yielded
     expression depends on its own previously-*received* values —
     e.g. `total = 0; ...; x = yield total; total += x`. There is no
     IF1 edge from `__pyc_generator__.send()`'s `value` formal (a
     different, unrelated Fun) into this primitive at all — the only
     connection is the C++ promise's `sent` field, invisible to FA —
     so with `result`'s type tied to `total`'s own flow, FA's fixed
     point (soundly, given what it can see) concludes the whole
     `total`/`x`/yielded-value cycle is a compile-time constant seeded
     by the initial `total = 0`, and silently constant-folds the real
     `co_yield` mechanism down to a hardcoded literal — `.send()`'s
     delivered values never reached `total` at runtime (observed:
     every `.send(v)` returned the same value regardless of `v`).
     Fixed by anchoring `result`'s type to the generic int64 type
     (`update_gen(result, sym_int64->abstract_type)`) instead of
     flowing it from the yielded expression — the same "opaque,
     non-constant anchor" trick as the coroutine-handle placeholder
     (`_CG_generator_placeholder_return`), and equally justified by
     today's only supported payload shape (int64 smuggled through
     `void*`, both directions).
   - **Runtime** (`pyc_c_runtime.h`): `_CG_Generator::promise_type
     ::yield_value` used to return `std::suspend_always` directly, so
     `co_yield expr` was a void-valued C++ expression with nowhere to
     receive a delivered value even at the C++ level. Added a custom
     `yield_awaiter` (`await_ready`/`await_suspend` matching
     `suspend_always`, `await_resume` returning the promise's new
     `sent` field) so `co_yield expr` itself evaluates to whatever the
     *next* resume delivers. `_CG_generator_advance` now clears `sent`
     to null before resuming (plain `next()` delivers None, matching
     real Python's `next(gen) == gen.send(None)`); a new
     `_CG_generator_send(handle, value)` sets `sent` then resumes.
   - **`__pyc_generator__`** (`__pyc__/09_generator.py`): redesigned
     around a `primed` flag instead of the old
     "`__pyc_more__` always advances, `__next__` only reads" split
     (which required exact `__pyc_more__`-then-`__next__` alternation
     and broke under direct repeated `__next__()`/`.send()` calls with
     no `__pyc_more__` in between — the actual "interleaved manual
     `next()`" bug). Both `__pyc_more__` and `__next__` now check
     `primed` before advancing, so each stays correct standalone *and*
     in the original `for`-loop alternation; nothing resumes the
     coroutine until the first real call, matching CPython's laziness
     (creating a generator runs none of its body). Added `.send(v)`,
     which always advances (delivering `v`) and returns the newly
     yielded value directly. No StopIteration/exception signaling on
     exhaustion (issue 011: no exception model yet) — matches every
     other pyc iterator's current unchecked past-exhaustion behavior.
   - Verified: bare interleaved `g.__next__()` calls outside any `for`
     loop; mixing direct `.__next__()` calls with a subsequent `for`
     loop on the *same* generator instance; `.send()` delivering into
     a running accumulator (`x = yield total; total += x`) across four
     sends, each returning the correct running total. All committed to
     `tests/generator_basic.py`, byte-for-byte matching `python3`.
   - **New limitation found while testing this** (not fixed, out of
     this item's scope): a generator whose loop never reaches its
     normal fall-through exit (`while True: yield i; i += 1`, no
     `break`) fails to compile (`FA: expression has no type` /
     `return statement not allowed in coroutine`) — reproduces with a
     bare `yield` statement, unrelated to `.send()`/assignment-form
     yield. Every currently-committed generator test uses a bounded
     loop (`for`, or a `while` with a real exit condition), so this
     was never hit before. Needs its own investigation — likely the
     `is_generator` placeholder-reply path (`gen_fun_pyda`) assumes
     its own code is reachable, which an unconditionally-looping body
     violates.
4. **`yield from`** — not attempted.
5. **Generator expressions** (issue 008, `(x for x in y)`) — not
   attempted; likely thin sugar over this mechanism once someone
   wants it, but genuinely unverified.
6. **`return value` inside a generator** (→ `StopIteration.value` in
   real Python) — v1 only supports bare `return`/fall-through; an
   explicit `return expr` is parsed and lowered but its value is
   discarded (same placeholder-swap as the bare case, see
   `PY_return_stmt` in `python_ifa_build_if1.cc`).
7. **Infinite generator loops fail to compile** (`while True:` with no
   `break`) — see item 3's "new limitation found" note above.
