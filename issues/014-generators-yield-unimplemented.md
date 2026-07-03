# Issue 014: Generators (`yield`) are unimplemented

**Status:** open.
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
